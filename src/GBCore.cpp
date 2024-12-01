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

bool GBCore::isBootROMValid(const std::filesystem::path& path)
{
	if (path.filename() == DMG_BOOTROM_NAME)
	{
		if (std::ifstream ifs{ path, std::ios::binary | std::ios::ate })
		{
			const std::ifstream::pos_type size = ifs.tellg();
			return size == sizeof(mmu.base_bootROM);
		}
	}
	else if (path.filename() == CGB_BOOTROM_NAME)
	{
		if (std::ifstream ifs{ path, std::ios::binary | std::ios::ate })
		{
			const std::ifstream::pos_type size = ifs.tellg();
			return size == 2048 || size == 2304;
		}
	}

	return false;
}

void GBCore::loadBootROM()
{
	if (appConfig::runBootROM) 
	{
		const std::filesystem::path romPath = FileUtils::nativePath(System::Current() == GBSystem::DMG ? appConfig::dmgBootRomPath : appConfig::cgbBootRomPath);

		if (std::ifstream ifs { romPath, std::ios::binary | std::ios::ate })
		{
			const std::ifstream::pos_type size = ifs.tellg();

			if (System::Current() == GBSystem::DMG)
			{
				if (size == sizeof(mmu.base_bootROM))
				{
					ifs.seekg(0, std::ios::beg);
					ifs.read(reinterpret_cast<char*>(&mmu.base_bootROM[0]), size);
				}
				else
					return;
			}
			else if (System::Current() == GBSystem::GBC)
			{
				if (size != 2048 && size != 2304)
					return;

				ifs.seekg(0, std::ios::beg);
				ifs.read(reinterpret_cast<char*>(&mmu.base_bootROM[0]), sizeof(mmu.base_bootROM));

				if (size == 2048)
					ifs.seekg(sizeof(mmu.base_bootROM), std::ios::beg);
				else if (size == 2304)
					ifs.seekg(0x200, std::ios::beg);

				ifs.read(reinterpret_cast<char*>(&mmu.GBCbootROM[0]), sizeof(mmu.GBCbootROM));
			}

			// LCD is disabled on boot ROM start
			ppu->setLCDEnable(false);
			cpu.enableBootROM();
		}
	}
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
	std::string filePrefix(SAVE_STATE_SIGNATURE.length(), 0);
	st.read(filePrefix.data(), SAVE_STATE_SIGNATURE.length());
	return filePrefix == SAVE_STATE_SIGNATURE;
}

FileLoadResult GBCore::loadFile(std::istream& st)
{
	autoSave();
	bool isSaveState { false };

	if (isSaveStateFile(st))
	{
		if (!loadState(st))
			return FileLoadResult::SaveStateROMNotFound;

		isSaveState = true;
	}
	else
	{
		if (filePath.extension() == ".sav")
		{
			st.seekg(0, std::ios::beg);

			constexpr std::array romExtensions = { ".gb", ".gbc", ".zip" };
			bool romLoaded = false;

			for (auto ext : romExtensions) 
			{
				const auto romPath = FileUtils::replaceExtension(filePath, ext);
				std::ifstream ifs { romPath, std::ios::in | std::ios::binary };

				if (ifs && loadROM(ifs, romPath)) 
				{
					loadBattery(st);
					romLoaded = true;
					break;
				}
			}

			if (!romLoaded) 
			{
				if (cartridge.ROMLoaded() && cartridge.hasBattery)
				{
					currentSave = 0;
					reset(false);
					loadBattery(st);
					loadBootROM();
				}
				else 
					return FileLoadResult::SaveStateROMNotFound;
			}
		}
		else
		{
			if (loadROM(st, filePath))
			{
				if (cartridge.hasBattery && appConfig::batterySaves)
				{
					if (std::ifstream ifs { getBatteryFilePath(), std::ios::in | std::ios::binary })
						loadBattery(ifs);
				}
			}
			else
				return FileLoadResult::InvalidROM;
		}
	}

	saveStateFolderPath = FileUtils::executableFolderPath / "saves" / (gameTitle + " (" + std::to_string(cartridge.getChecksum()) + ")");

	if (!std::filesystem::exists(saveStateFolderPath))
		std::filesystem::create_directories(saveStateFolderPath);

	appConfig::updateConfigFile();
	return isSaveState ? FileLoadResult::SuccessSaveState : FileLoadResult::SuccessROM;
}

bool GBCore::loadROMFromStream(std::istream& st)
{
	if (cartridge.loadROM(st))
	{
		updatePPUSystem();
		reset(true);
		currentSave = 0;
		return true;
	}

	return false;
}

bool GBCore::loadROMFromZipStream(std::istream& st)
{
	st.seekg(0, std::ios::end);
	const auto size = st.tellg();

	if (size > Cartridge::MAX_ROM_SIZE)
		return false;

	st.seekg(0, std::ios::beg);

	std::vector<uint8_t> zipData(size);
	st.read(reinterpret_cast<char*>(zipData.data()), size);

	mz_zip_archive zipArchive{};

	if (!mz_zip_reader_init_mem(&zipArchive, zipData.data(), zipData.size(), 0))
		return false;

	const int fileCount = mz_zip_reader_get_num_files(&zipArchive);
	std::stringstream decompressedStream;

	bool isSuccess = false;

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

			std::string buffer;
			buffer.resize(uncompressedSize);

			if (mz_zip_reader_extract_to_mem(&zipArchive, i, buffer.data(), uncompressedSize, 0))
			{
				memstream ms { buffer.data(), buffer.data() + buffer.size() };
				isSuccess = loadROMFromStream(ms); 
			}
			break;
		}
	}

	mz_zip_reader_end(&zipArchive);
	return isSuccess;
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

	batteryBackupPath.replace_filename(FileUtils::pathToUTF8(batterySavePath.stem()) + " - BACKUP.sav");
	std::filesystem::copy_file(batterySavePath, batteryBackupPath, std::filesystem::copy_options::overwrite_existing);
}

void GBCore::loadState(int num)
{
	if (!cartridge.ROMLoaded()) return;

	if (currentSave != 0)
		saveState(currentSave);

	const auto _filePath = getSaveStateFilePath(num);
	std::ifstream st(_filePath, std::ios::in | std::ios::binary);

	if (st && isSaveStateFile(st))
	{
		loadState(st);
		updateSelectedSaveInfo(num);
	}
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
	std::ostringstream ss { };
	saveGBState(ss);

	const uint64_t uncompressedSize = ss.tellp();
	const auto uncompressedData = ss.view().data();

	mz_ulong compressedSize = mz_compressBound(uncompressedSize);
	std::vector<uint8_t> compressedBuffer(compressedSize);

	int status = mz_compress(compressedBuffer.data(), &compressedSize, reinterpret_cast<const unsigned char*>(uncompressedData), uncompressedSize);
	const bool isCompressed = status == MZ_OK;

	st << SAVE_STATE_SIGNATURE;

	const auto romFilePathStr{ FileUtils::pathToUTF8(romFilePath) };
	const uint16_t filePathLen{ static_cast<uint16_t>(romFilePathStr.length()) };

	ST_WRITE(filePathLen);
	st << romFilePathStr;

	const auto checksum { cartridge.getChecksum() };
	ST_WRITE(checksum);

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
		uint32_t compressedSize32 = static_cast<uint32_t>(compressedSize);
		ST_WRITE(compressedSize32);
		st.write(reinterpret_cast<const char*>(compressedBuffer.data()), compressedSize);
	}
	else
		st.write(reinterpret_cast<const char*>(ppu->framebufferPtr()), PPU::FRAMEBUFFER_SIZE);
}
void GBCore::loadFrameBuffer(std::istream& st, uint8_t* framebuffer)
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
		mz_uncompress(reinterpret_cast<unsigned char*>(framebuffer), &uncompressedSize, compressedBuffer.data(), compressedSize);
	}
}

bool GBCore::loadState(std::istream& st)
{
	uint16_t filePathLen { 0 };
	ST_READ(filePathLen);

	std::string romPath(filePathLen, 0);
	st.read(romPath.data(), filePathLen);

	uint8_t saveStateChecksum;
	ST_READ(saveStateChecksum);

	if (!cartridge.ROMLoaded() || cartridge.getChecksum() != saveStateChecksum)
	{
		const auto newRomPath = std::filesystem::path{ FileUtils::nativePath(romPath) };
		std::ifstream romStream(newRomPath, std::ios::in | std::ios::binary);

		if (!romStream || !Cartridge::romSizeValid(romStream))
			return false;

		if (cartridge.calculateHeaderChecksum(romStream) != saveStateChecksum)
			return false;

		if (!cartridge.loadROM(romStream))
			return false;

		romFilePath = newRomPath;
		currentSave = 0;
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

		auto currentPos = st.tellg();
		st.seekg(0, std::ios::end);
		const mz_ulong compressedSize = st.tellg() - currentPos;
		st.seekg(currentPos);

		std::vector<uint8_t> compressedBuffer(compressedSize);
		st.read(reinterpret_cast<char*>(compressedBuffer.data()), compressedSize);

		std::vector<uint8_t> buffer(uncompressedSize);
		int status = mz_uncompress(reinterpret_cast<unsigned char*>(buffer.data()), reinterpret_cast<mz_ulong*>(&uncompressedSize), compressedBuffer.data(), compressedSize);

		if (status != MZ_OK)
			return false;

		memstream ms { buffer.data(), buffer.data() + buffer.size() };
		loadGBState(ms);
	}

	// For the first frame not to be as teared.
	st.seekg(framebufDataOffset, std::ios::beg);
	loadFrameBuffer(st, ppu->backbufferPtr());

	if (drawCallback != nullptr)
		drawCallback(ppu->backbufferPtr(), true);

	return true;
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

	loadFrameBuffer(st, framebuffer);
	return true;
}

void GBCore::saveGBState(std::ostream& st) const
{
	const auto system { System::Current() };
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