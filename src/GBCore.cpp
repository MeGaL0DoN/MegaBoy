#include <fstream>
#include <string>
#include <miniz/miniz.h>

#include "GBCore.h"
#include "appConfig.h"
#include "Utils/fileUtils.h"
#include "Utils/memstream.h"
#include "debugUI.h"

GBCore gbCore{};

void GBCore::reset(bool resetBattery, bool clearBuf, bool updateSystem)
{
	if (updateSystem)
		cartridge.updateSystem(); // If dmg or cgb preference has changed.

	updatePPUSystem();
	ppu->reset(clearBuf);

	cpu.reset();
	mmu.reset();
	serial.reset();
	input.reset();
	apu.reset();
	cartridge.getMapper()->reset(resetBattery);

	emulationPaused = false;
	breakpointHit = false;
	cycleCounter = 0;
}

constexpr uint16_t DMG_BOOTROM_SIZE = sizeof(MMU::baseBootROM);
constexpr uint16_t CGB_BOOTROM_SIZE = DMG_BOOTROM_SIZE + sizeof(MMU::cgbBootROM);

bool GBCore::isBootROMValid(const std::filesystem::path& path)
{
	if (path.filename() == DMG_BOOTROM_NAME)
		return std::filesystem::file_size(path) == DMG_BOOTROM_SIZE;

	if (path.filename() == CGB_BOOTROM_NAME)
	{
		const auto fileSize = std::filesystem::file_size(path);
		// There is 0x100 bytes gap between dmg and cgb boot. Some files may or may not include it.
		return fileSize == CGB_BOOTROM_SIZE || fileSize == CGB_BOOTROM_SIZE + 0x100;
	}

	return false;
}
void GBCore::loadBootROM() 
{
	if (!appConfig::runBootROM) 
		return;

	const std::filesystem::path bootRomPath {
		System::Current() == GBSystem::DMG ? appConfig::dmgBootRomPath : appConfig::cgbBootRomPath
	};

	if (!isBootROMValid(bootRomPath)) 
		return;

	std::ifstream st { bootRomPath, std::ios::binary };
	if (!st) return;

	ST_READ_ARR(mmu.baseBootROM);

	if (System::Current() == GBSystem::GBC)
	{
		if (FileUtils::remainingBytes(st) == CGB_BOOTROM_SIZE)
			st.seekg(0x100, std::ios::cur);

		ST_READ_ARR(mmu.cgbBootROM);
	}

	ppu->setLCDEnable(false);
	cpu.enableBootROM();
}

void GBCore::update(uint32_t cyclesToExecute)
{
	if (!cartridge.ROMLoaded() || emulationPaused || breakpointHit) [[unlikely]] 
		return;

	const uint64_t targetCycles = cycleCounter + (cyclesToExecute * speedFactor);

	while (cycleCounter < targetCycles)
	{
		if (breakpoints[cpu.getPC()]) [[unlikely]]
		{
			breakpointHit = true;
			debugUI::signalBreakpoint();
			break;
		}

		cycleCounter += cpu.execute();
	}
}

void GBCore::stepComponents()
{
	cpu.updateTimer();
	ppu->execute(cpu.TcyclesPerM());
	mmu.execute();
	serial.execute();
}

bool GBCore::isSaveStateFile(std::istream& st)
{
	std::string fileSignature(SAVE_STATE_SIGNATURE.length(), 0);
	st.read(fileSignature.data(), SAVE_STATE_SIGNATURE.length());
	return fileSignature == SAVE_STATE_SIGNATURE;
}

FileLoadResult GBCore::loadFile(std::istream& st)
{
	if (!st)
		return FileLoadResult::FileError;

	autoSave();
	bool isSaveState { false };

	if (isSaveStateFile(st))
	{
		const auto result = loadState(st);

		if (result != FileLoadResult::SuccessSaveState)
			return result;

		isSaveState = true;
	}
	else
	{
		if (filePath.extension() == ".sav")
		{
			FileUtils::removeFilenameSubstr(filePath, STR(" - BACKUP"));
			st.seekg(0, std::ios::beg);

			constexpr std::array romExtensions = { ".gb", ".gbc", ".zip" };
			bool romLoaded = false;

			if (romFilePath.stem() != filePath.stem())
			{
				for (auto ext : romExtensions)
				{
					const auto romPath = FileUtils::replaceExtension(filePath, ext);
					std::ifstream ifs{ romPath, std::ios::in | std::ios::binary };

					if (loadROM(ifs, romPath))
					{
						if (cartridge.hasBattery)
						{
							backupBatteryFile();
							cartridge.getMapper()->loadBattery(st);
						}

						romLoaded = true;
						break;
					}
				}
			}

			if (!romLoaded) 
			{
				if (!cartridge.ROMLoaded() || !cartridge.hasBattery)
					return FileLoadResult::ROMNotFound;

				if (!cartridge.getMapper()->loadBattery(st))
					return FileLoadResult::InvalidBattery;

				currentSave = 0;
				reset(false);
			}

			loadBootROM();
		}
		else
		{
			if (!loadROM(st, filePath))
				return FileLoadResult::InvalidROM;

			if (cartridge.hasBattery && appConfig::batterySaves)
			{
				if (std::ifstream ifs{ getBatteryFilePath(), std::ios::in | std::ios::binary })
				{
					backupBatteryFile();
					cartridge.getMapper()->loadBattery(ifs);
				}
			}

			loadBootROM();
		}
	}

	saveStateFolderPath = FileUtils::executableFolderPath / "saves" / (gameTitle + " (" + std::to_string(cartridge.getChecksum()) + ")");

	if (!std::filesystem::exists(saveStateFolderPath))
		std::filesystem::create_directories(saveStateFolderPath);

	appConfig::updateConfigFile();
	return isSaveState ? FileLoadResult::SuccessSaveState : FileLoadResult::SuccessROM;
}

bool GBCore::loadROM(std::istream& st, const std::filesystem::path& filePath)
{
	if (!st)
		return false;

	if (filePath.extension() == ".zip")
	{
		const auto romData = extractZippedROM(st);
		memstream ms { romData.data(), romData.data() + romData.size() };

		if (!cartridge.loadROM(ms))
			return false;
	}
	else if (!cartridge.loadROM(st))
		return false;

	currentSave = 0;
	romFilePath = filePath;
	updatePPUSystem();
	reset(true);

	return true;
}

std::vector<uint8_t> GBCore::extractZippedROM(std::istream& st)
{
	st.seekg(0, std::ios::end);
	const auto size = st.tellg();

	if (size > mz_compressBound(Cartridge::MAX_ROM_SIZE))
		return {};

	st.seekg(0, std::ios::beg);

	std::vector<uint8_t> zipData(size);
	st.read(reinterpret_cast<char*>(zipData.data()), size);

	mz_zip_archive zipArchive{};

	if (!mz_zip_reader_init_mem(&zipArchive, zipData.data(), zipData.size(), 0))
		return {};

	const int fileCount = mz_zip_reader_get_num_files(&zipArchive);

	for (int i = 0; i < fileCount; i++)
	{
		mz_zip_archive_file_stat file_stat;

		if (!mz_zip_reader_file_stat(&zipArchive, i, &file_stat))
			continue;

		std::string filename = file_stat.m_filename;
		if (filename.length() < 3) continue;

		const std::string ext = filename.substr(filename.length() - 3);
		const bool is_gb = (ext == ".gb" || ext == "gbc");

		if (is_gb)
		{
			const size_t uncompressedSize = file_stat.m_uncomp_size;
			std::vector<uint8_t> buffer(uncompressedSize);

			if (mz_zip_reader_extract_to_mem(&zipArchive, i, buffer.data(), uncompressedSize, 0))
			{
				mz_zip_reader_end(&zipArchive);
				return buffer;
			}

			break;
		}
	}

	mz_zip_reader_end(&zipArchive);
	return {};
}

void GBCore::autoSave() const 
{
	if (currentSave != 0 && appConfig::autosaveState)
		saveState(getSaveStateFilePath(currentSave));

	if (!cartridge.hasBattery || !appConfig::batterySaves) 
		return;

	if (romFilePath.empty() && customBatterySavePath.empty()) 
		return;

	auto mapper = cartridge.getMapper();
	if (mapper->sramDirty) 
	{
		saveBattery(getBatteryFilePath());
		mapper->sramDirty = false;
	}
}

void GBCore::backupBatteryFile() const
{
	if (!cartridge.hasBattery || !appConfig::batterySaves || !customBatterySavePath.empty())
		return;
	
	const auto batterySavePath{ getBatteryFilePath() };
	auto batteryBackupPath { batterySavePath };
	batteryBackupPath.replace_filename(FileUtils::getNativePathStr(batterySavePath.stem()) + STR(" - BACKUP.sav"));

#ifdef __MINGW32__
	std::filesystem::remove(batteryBackupPath); // mingw bug, copy_file crashes if the file already exists, even with overwrite_existing.
#endif
	std::filesystem::copy_file(batterySavePath, batteryBackupPath, std::filesystem::copy_options::overwrite_existing);
}

void GBCore::loadState(int num)
{
	if (!cartridge.ROMLoaded()) return;

	if (currentSave != 0)
		saveState(currentSave);

	const auto _filePath = getSaveStateFilePath(num);
	std::ifstream st(_filePath, std::ios::in | std::ios::binary);

	if (!st || !isSaveStateFile(st))
		return;

	if (loadState(st) == FileLoadResult::SuccessSaveState)
		updateSelectedSaveInfo(num);
}

void GBCore::saveState(int num)
{
	if (!cartridge.ROMLoaded()) return;

	saveState(getSaveStateFilePath(num));

	if (currentSave == 0)
		updateSelectedSaveInfo(num);
}

void GBCore::saveState(std::ostream& st) const
{
	st.write(SAVE_STATE_SIGNATURE.data(), SAVE_STATE_SIGNATURE.length());

	const auto romFilePathStr { FileUtils::pathToUTF8(romFilePath) };
	const auto filePathLen { static_cast<uint16_t>(romFilePathStr.length()) };

	ST_WRITE(filePathLen);
	st.write(romFilePathStr.data(), filePathLen);

	const auto checksum{ cartridge.getChecksum() };
	ST_WRITE(checksum);

	std::ostringstream ss { };
	saveGBState(ss);

	const uint64_t uncompressedSize = ss.tellp();
	const auto uncompressedData = ss.view().data();

	mz_ulong compressedSize = mz_compressBound(uncompressedSize);
	std::vector<uint8_t> compressedBuffer(compressedSize);

	int status = mz_compress(compressedBuffer.data(), &compressedSize, reinterpret_cast<const unsigned char*>(uncompressedData), uncompressedSize);
	const bool isCompressed = status == MZ_OK;

	saveFrameBuffer(st); // Save frame buffer first to easily locate for thumbnails.

	ST_WRITE(isCompressed);

	if (isCompressed)
	{
		ST_WRITE(uncompressedSize);
		st.write(reinterpret_cast<const char*>(compressedBuffer.data()), compressedSize);
	}
	else
		st.write(uncompressedData, uncompressedSize);
}

void GBCore::saveFrameBuffer(std::ostream& st) const
{
	mz_ulong compressedSize = mz_compressBound(PPU::FRAMEBUFFER_SIZE);
	std::vector<uint8_t> compressedBuffer(compressedSize);

	int status = mz_compress(compressedBuffer.data(), &compressedSize, reinterpret_cast<const unsigned char*>(ppu->framebufferPtr()), PPU::FRAMEBUFFER_SIZE);
	const bool isCompressed = status == MZ_OK;

	ST_WRITE(isCompressed);
	
	if (isCompressed)
	{
		uint32_t compressedSize32 { static_cast<uint32_t>(compressedSize) };
		ST_WRITE(compressedSize32);
		st.write(reinterpret_cast<const char*>(compressedBuffer.data()), compressedSize);
	}
	else
		st.write(reinterpret_cast<const char*>(ppu->framebufferPtr()), PPU::FRAMEBUFFER_SIZE);
}
bool GBCore::loadFrameBuffer(std::istream& st, uint8_t* framebuffer)
{
	bool isCompressed;
	ST_READ(isCompressed);

	if (!isCompressed)
		st.read(reinterpret_cast<char*>(framebuffer), PPU::FRAMEBUFFER_SIZE);
	else
	{
		uint32_t compressedSize;
		ST_READ(compressedSize);

		std::vector<uint8_t> compressedBuffer(compressedSize);
		st.read(reinterpret_cast<char*>(compressedBuffer.data()), compressedSize);

		mz_ulong uncompressedSize{ PPU::FRAMEBUFFER_SIZE };
		int status = mz_uncompress(reinterpret_cast<unsigned char*>(framebuffer), &uncompressedSize, compressedBuffer.data(), compressedSize);

		if (status != MZ_OK)
			return false;
	}

	return true;
}

FileLoadResult GBCore::loadState(std::istream& st)
{
	uint16_t filePathLen { 0 };
	ST_READ(filePathLen);

	std::string romPath(filePathLen, 0);
	st.read(romPath.data(), filePathLen);

	uint8_t saveStateChecksum;
	ST_READ(saveStateChecksum);

	if (!cartridge.ROMLoaded() || cartridge.getChecksum() != saveStateChecksum)
	{
		const std::filesystem::path newRomPath { FileUtils::nativePathFromUTF8(romPath) };
		std::ifstream romStream { newRomPath, std::ios::in | std::ios::binary };

		if (!romStream)
			return FileLoadResult::ROMNotFound;

		const auto loadRomWithValidation = [&](std::istream& st) {
			return cartridge.calculateHeaderChecksum(st) == saveStateChecksum && cartridge.loadROM(st);
		};

		if (newRomPath.extension() == ".zip")
		{
			const auto romData = extractZippedROM(romStream);
			memstream ms{ romData.data(), romData.data() + romData.size() };

			if (!loadRomWithValidation(ms))
				return FileLoadResult::ROMNotFound;
		}
		else if (!loadRomWithValidation(romStream))
			return FileLoadResult::ROMNotFound;

		romFilePath = newRomPath;
	}

	uint32_t framebufDataOffset = st.tellg();

	bool isFramebufCompressed;
	ST_READ(isFramebufCompressed);

	if (isFramebufCompressed)
	{
		uint32_t compressedSize;
		ST_READ(compressedSize);
		st.seekg(compressedSize, std::ios::cur);
	}
	else
		st.seekg(PPU::FRAMEBUFFER_SIZE, std::ios::cur); 

	bool isStateCompressed;
	ST_READ(isStateCompressed);

	if (!isStateCompressed)
		loadGBState(st);
	else
	{
		uint64_t uncompressedSize;
		ST_READ(uncompressedSize);

		const auto compressedSize { static_cast<mz_ulong>(FileUtils::remainingBytes(st)) };

		std::vector<uint8_t> compressedBuffer(compressedSize);
		st.read(reinterpret_cast<char*>(compressedBuffer.data()), compressedSize);

		std::vector<uint8_t> buffer(uncompressedSize);
		int status = mz_uncompress(reinterpret_cast<unsigned char*>(buffer.data()), reinterpret_cast<mz_ulong*>(&uncompressedSize), compressedBuffer.data(), compressedSize);

		if (status != MZ_OK)
			return FileLoadResult::CorruptSaveState;

		memstream ms { buffer.data(), buffer.data() + buffer.size() };
		loadGBState(ms);
	}

	// For the first frame not to be as teared.
	st.seekg(framebufDataOffset, std::ios::beg);
	loadFrameBuffer(st, ppu->backbufferPtr());

	if (drawCallback != nullptr)
		drawCallback(ppu->backbufferPtr(), true);

	return FileLoadResult::SuccessSaveState;
}

void GBCore::saveGBState(std::ostream& st) const
{
	const auto system{ System::Current() };
	ST_WRITE(system);
	ST_WRITE(cycleCounter);

	cpu.saveState(st);
	ppu->saveState(st);
	mmu.saveState(st);
	serial.saveState(st);
	input.saveState(st);
	cartridge.getMapper()->saveState(st); // Mapper must be saved last.
}
void GBCore::loadGBState(std::istream& st)
{
	GBSystem system;
	ST_READ(system);

	System::Set(system);
	reset(true, false, false);

	ST_READ(cycleCounter);

	cpu.loadState(st);
	ppu->loadState(st);
	mmu.loadState(st);
	serial.loadState(st);
	input.loadState(st);
	cartridge.getMapper()->loadState(st);
}

bool GBCore::loadSaveStateThumbnail(const std::filesystem::path& path, uint8_t* framebuffer)
{
	if (!cartridge.ROMLoaded())
		return false;

	std::ifstream st { path, std::ios::in | std::ios::binary };

	if (!st || !isSaveStateFile(st))
		return false;

	uint16_t filePathLen{ 0 };
	ST_READ(filePathLen);
	st.seekg(filePathLen, std::ios::cur);

	uint8_t saveStateChecksum;
	ST_READ(saveStateChecksum);

	if (cartridge.getChecksum() != saveStateChecksum)
		return false;

	return loadFrameBuffer(st, framebuffer);
}