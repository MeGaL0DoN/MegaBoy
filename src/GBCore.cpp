#include <fstream>
#include <string>
#include <miniz/miniz.h>

#include "GBCore.h"
#include "appConfig.h"
#include "Utils/fileUtils.h"
#include "Utils/memstream.h"
#include "debugUI.h"

void GBCore::updatePPUSystem()
{
	switch (System::Current())
	{
	case GBSystem::DMG:
		ppu = std::unique_ptr<PPU> { std::make_unique<PPUCore<GBSystem::DMG>>(mmu, cpu) };
		break;
	case GBSystem::CGB:
		ppu = std::unique_ptr<PPU> { std::make_unique<PPUCore<GBSystem::CGB>>(mmu, cpu) };
		break;
	case GBSystem::DMGCompatMode:
		ppu = std::unique_ptr<PPU> { std::make_unique<PPUCore<GBSystem::DMGCompatMode>>(mmu, cpu) };
		break;
	}

	ppu->setDebugEnable(ppuDebugEnable);
	ppu->drawCallback = [this](const uint8_t* framebuf, bool firstFrame) { vBlankHandler(framebuf, firstFrame); };
}

void GBCore::reset(bool resetBattery, bool clearBuf, bool fullReset)
{
	if (fullReset)
	{
		const auto prevSystem { System::Current() };
		cartridge.updateSystem();

		loadBootROM();

		if (!mmu.isBootROMMapped && cartridge.isDMGCompatSystem()) // Since boot rom won't be executed, DMG compat mode needs to be enabled right away.
			System::Set(GBSystem::DMGCompatMode);

		if (prevSystem != System::Current())
		{
			updatePPUSystem();
			mmu.updateSystem();
		}
	}
	else
		mmu.isBootROMMapped = false;

	ppu->reset(clearBuf);
	cpu.reset();
	mmu.reset();
	serial.reset();
	joypad.reset();
	apu.reset();
	cartridge.getMapper()->reset(resetBattery);

	if (mmu.isBootROMMapped)
	{
		// This is important since only known pre-bootrom state is that LCD and APU are disabled, and PC is 0x00.
		cpu.resetPC();
		ppu->setLCDEnable(false);
		apu.regs.apuEnable = false;
	}

	emulationPaused = false;
	breakpointHit = false;
	dmgCompatSwitch = false;
	cycleCounter = 0;
}

constexpr uint16_t DMG_BOOTROM_SIZE = sizeof(MMU::baseBootROM);
constexpr uint16_t CGB_BOOTROM_SIZE = DMG_BOOTROM_SIZE + sizeof(MMU::cgbBootROM);

bool GBCore::isBootROMValid(std::istream& st, const std::filesystem::path& path)
{
	if (!st) return false;
	const auto filename { path.filename() };

	if (filename == DMG_BOOTROM_NAME)
		return FileUtils::remainingBytes(st) == DMG_BOOTROM_SIZE;
	if (filename == CGB_BOOTROM_NAME)
	{
		// There is 0x100 bytes gap between dmg and cgb boot. Some files may or may not include it.
		const auto bootSize { FileUtils::remainingBytes(st) };
		return bootSize == CGB_BOOTROM_SIZE || bootSize == CGB_BOOTROM_SIZE + 0x100;
	}

	return false;
}
void GBCore::loadBootROM() 
{
	mmu.isBootROMMapped = false;

	if (!appConfig::runBootROM)
		return;

	const std::filesystem::path bootRomPath {
		System::Current() == GBSystem::DMG ? appConfig::dmgBootRomPath : appConfig::cgbBootRomPath
	};

	std::ifstream st { bootRomPath, std::ios::binary };

	if (!isBootROMValid(st, bootRomPath))
		return;

	ST_READ_ARR(mmu.baseBootROM);

	if (System::IsCGBDevice(System::Current()))
	{
		if (FileUtils::remainingBytes(st) == CGB_BOOTROM_SIZE)
			st.seekg(0x100, std::ios::cur);

		ST_READ_ARR(mmu.cgbBootROM);
	}

	mmu.isBootROMMapped = true;
}

// Called by CGB boot rom if ROM is dmg only.
void GBCore::enableDMGCompatMode()
{
	System::Set(GBSystem::DMGCompatMode);

	std::stringstream st;
	ppu->saveState(st); // Need to save ppu state, since ppu object is destroyed when changing the system.

	updatePPUSystem();
	mmu.updateSystem();

	ppu->loadState(st);

	// CGB boot rom doesn't do that for some reason but keeps it at 0x7F which is incorrect for DMG mode, maybe it happens implicitly on KEY0 write??
	serial.writeSerialControl(0x7E);
}

template void GBCore::_emulateFrame<true>();
template void GBCore::_emulateFrame<false>();

template <bool checkBreakpoints>
void GBCore::_emulateFrame()
{
	if (!cartridge.ROMLoaded() || emulationPaused) [[unlikely]]
		return;

	const uint64_t targetCycles { cycleCounter + (CYCLES_PER_FRAME * speedFactor) };

	while (cycleCounter < targetCycles)
	{
		if constexpr (checkBreakpoints)
		{
			if (breakpoints[cpu.getPC()] || opcodeBreakpoints[mmu.read8(cpu.getPC())]) [[unlikely]]
			{
				breakpointHit = true;
				debugUI::signalBreakpoint();
			}

			if (breakpointHit) [[unlikely]]
				break;
		}

		cycleCounter += cpu.execute();
	}
}

void GBCore::stepComponents()
{
	cpu.updateTimer();
	ppu->execute();
	mmu.execute();
	serial.execute();
}

bool GBCore::isSaveStateFile(std::istream& st)
{
	std::string fileSignature(SAVE_STATE_SIGNATURE.length(), 0);
	st.read(fileSignature.data(), SAVE_STATE_SIGNATURE.length());
	return fileSignature == SAVE_STATE_SIGNATURE;
}

FileLoadResult GBCore::loadFile(std::istream& st, std::filesystem::path filePath, bool loadBatteryOnRomload)
{
	if (!st)
		return FileLoadResult::FileError;

	autoSave();
	const bool isSaveState { isSaveStateFile(st) };

	if (isSaveState)
	{
		const auto result { loadState(st) };

		if (result != FileLoadResult::SuccessSaveState)
			return result;
	}
	else
	{
		auto ext { filePath.extension() };
		auto stem { FileUtils::pathNativeStr(filePath.stem()) };

		// Support for .sav.bak files.
		if (ext == ".bak" && stem.size() >= 4) 
		{
			if (stem.substr(stem.size() - 4) == N_STR(".sav"))
			{ 
				stem.resize(stem.size() - 4);
				ext = ".sav";
				filePath.replace_filename(stem + N_STR(".sav"));
			}
		}

		if (ext == ".sav")
		{
			st.seekg(0, std::ios::beg);

			constexpr std::array romExtensions { ".gb", ".gbc", ".zip" };
			bool romLoaded { false };

			if (romFilePath.stem() != stem)
			{
				for (auto ext : romExtensions)
				{
					const auto romPath { FileUtils::replaceExtension(filePath, ext) };
					std::ifstream ifs { romPath, std::ios::in | std::ios::binary };
					if (!ifs) continue;

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
		}
		else
		{
			if (!loadROM(st, filePath))
				return FileLoadResult::InvalidROM;

			if (loadBatteryOnRomload) 
				loadCurrentBatterySave();;
		}
	}

	saveStateFolderPath = FileUtils::executableFolderPath / "saves" / (gameTitle + " (" + std::to_string(cartridge.getChecksum()) + ")");
	appConfig::updateConfigFile();
	return isSaveState ? FileLoadResult::SuccessSaveState : FileLoadResult::SuccessROM;
}

bool GBCore::loadROM(std::istream& st, const std::filesystem::path& filePath)
{
	if (filePath.extension() == ".zip")
	{
		const auto romData { extractZippedROM(st) };
		memstream ms { romData };

		if (!cartridge.loadROM(ms))
			return false;
	}
	else if (!cartridge.loadROM(st))
		return false;

	currentSave = 0;
	romFilePath = filePath;
	reset(true);

	return true;
}

std::vector<uint8_t> GBCore::extractZippedROM(std::istream& st)
{
	st.seekg(0, std::ios::end);
	const auto size { st.tellg() };

	if (size > mz_compressBound(Cartridge::MAX_ROM_SIZE))
		return {};

	st.seekg(0, std::ios::beg);

	std::vector<uint8_t> zipData(size);
	st.read(reinterpret_cast<char*>(zipData.data()), size);

	mz_zip_archive zipArchive{};

	if (!mz_zip_reader_init_mem(&zipArchive, zipData.data(), zipData.size(), 0))
		return {};

	const auto fileCount { mz_zip_reader_get_num_files(&zipArchive) };

	for (int i = 0; i < fileCount; i++)
	{
		mz_zip_archive_file_stat file_stat;

		if (!mz_zip_reader_file_stat(&zipArchive, i, &file_stat))
			continue;

		const std::string filename { file_stat.m_filename };
		if (filename.length() < 3) continue;

		const std::string ext { filename.substr(filename.length() - 3) };
		const bool isGB { ext == ".gb" || ext == "gbc" };

		if (isGB)
		{
			const auto uncompressedSize { file_stat.m_uncomp_size };
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
		saveState(getSaveStatePath(currentSave));

	if (!cartridge.hasBattery || !appConfig::batterySaves) 
		return;

	if (romFilePath.empty() && customBatterySavePath.empty()) 
		return;

	if (cartridge.getMapper()->sramDirty)
	{
		saveBattery(getBatteryFilePath());
		cartridge.getMapper()->sramDirty = false;
	}
}

void GBCore::backupBatteryFile() const
{
	if (!cartridge.hasBattery || !appConfig::batterySaves || !customBatterySavePath.empty())
		return;
	
	const auto batterySavePath { getBatteryFilePath() };
	auto batteryBackupPath { FileUtils::replaceExtension(batterySavePath, ".sav.bak") };
	std::error_code err;

#ifdef __MINGW32__
	std::filesystem::remove(batteryBackupPath, err); // mingw bug, copy_file fails if the file already exists, even with overwrite_existing.
#endif
	std::filesystem::copy_file(batterySavePath, batteryBackupPath, std::filesystem::copy_options::overwrite_existing, err);
}

FileLoadResult GBCore::loadState(const std::filesystem::path& path)
{
	std::ifstream st { path, std::ios::in | std::ios::binary };

	if (!st) 
		return FileLoadResult::FileError;

	if (!isSaveStateFile(st))
		return FileLoadResult::CorruptSaveState;

	return loadState(st);
}
FileLoadResult GBCore::loadState(int num)
{
	std::ifstream st { getSaveStatePath(num), std::ios::in | std::ios::binary };

	if (!st)
		return FileLoadResult::FileError;

	if (!isSaveStateFile(st))
		return FileLoadResult::CorruptSaveState;

	const auto result { loadState(st) };

	if (result == FileLoadResult::SuccessSaveState)
		updateSelectedSaveInfo(num);

	return result;
}

void GBCore::saveState(const std::filesystem::path& path) const
{
	if (!canSaveStateNow())
		return;

	std::error_code err;

	if (!std::filesystem::exists(saveStateFolderPath, err))
		std::filesystem::create_directories(saveStateFolderPath, err);

	std::ofstream st { path, std::ios::out | std::ios::binary };
	if (!st) return;
	writeState(st);
}
void GBCore::saveState(int num)
{
	if (!cartridge.ROMLoaded()) 
		return;

	saveState(getSaveStatePath(num));

	if (currentSave == 0)
		updateSelectedSaveInfo(num);
}

uint64_t GBCore::calculateHash(std::span<const uint8_t> data)
{
	constexpr uint64_t FNV_PRIME = 0x100000001b3;
	uint64_t hash { 0xcbf29ce484222325 };

	for (const auto byte : data)
	{
		hash ^= byte;
		hash *= FNV_PRIME;
	}

	return hash;
}

void GBCore::writeFrameBuffer(std::ostream& st) const
{
	mz_ulong compressedSize { mz_compressBound(PPU::FRAMEBUFFER_SIZE) };
	std::vector<uint8_t> compressedBuffer(compressedSize);

	const auto framebufPtr { reinterpret_cast<const unsigned char*>(ppu->framebufferPtr()) };
	const int status { mz_compress(compressedBuffer.data(), &compressedSize, framebufPtr, PPU::FRAMEBUFFER_SIZE) };

	const bool isCompressed { status == MZ_OK };
	ST_WRITE(isCompressed);
	
	if (isCompressed)
	{
		st.write(reinterpret_cast<const char*>(&compressedSize), sizeof(uint32_t));
		st.write(reinterpret_cast<const char*>(compressedBuffer.data()), compressedSize);
	}
	else
		st.write(reinterpret_cast<const char*>(ppu->framebufferPtr()), PPU::FRAMEBUFFER_SIZE);
}
bool GBCore::loadFrameBuffer(std::istream& st, std::span<uint8_t> framebuffer)
{
	bool isCompressed;
	ST_READ(isCompressed);

	if (!isCompressed)
		st.read(reinterpret_cast<char*>(framebuffer.data()), PPU::FRAMEBUFFER_SIZE);
	else
	{
		uint32_t compressedSize;
		ST_READ(compressedSize);

		std::vector<uint8_t> compressedBuffer(compressedSize);
		st.read(reinterpret_cast<char*>(compressedBuffer.data()), compressedSize);

		mz_ulong uncompressedSize { PPU::FRAMEBUFFER_SIZE };
		const auto framebufPtr { reinterpret_cast<unsigned char*>(framebuffer.data()) };
		const int status { mz_uncompress(framebufPtr, &uncompressedSize, compressedBuffer.data(), compressedSize) };

		if (status != MZ_OK)
			return false;
	}

	return true;
}

bool GBCore::validateAndLoadRom(const std::filesystem::path& romPath, uint8_t checksum)
{
	std::ifstream st { romPath, std::ios::in | std::ios::binary };

	if (!st)
		return false;

	const auto loadRom = [&](auto&& st) -> bool {
		return cartridge.calculateHeaderChecksum(st) == checksum && cartridge.loadROM(st);
	};

	if (romPath.extension() == ".zip")
	{
		const auto romData { extractZippedROM(st) };

		if (!loadRom(memstream { romData }))
			return false;
	}
	else
	{
		if (!loadRom(st))
			return false;
	}

	return true;
}

// .mbs SAVE STATE FORMAT (LITTLE ENDIAN):
// 27 byte save signature (SAVE_STATE_SIGNATURE variable)
// 8 byte FNV-1a hash of the file (excluding the signature)
// 1 byte ROM cartridge header checksum
// 2 byte ROM file path (UTF-8) length
// N byte ROM file path
// 1 byte isFramebufCompressed flag
// if isFramebufCompressed is true: 4 byte compressed framebuffer size
// 69120 bytes of uncompressed or deflate compressed framebuffer data
// 1 byte isStateCompressed flag
// if isStateCompressed is true: 4 byte uncompressed state size
// The rest of the file is uncompressed or deflate compressed GB state data:
    // 1 byte GB system type (GBSystem enum)
    // 8 byte cycle counter
    // CPU -> PPU -> MMU -> APU -> Serial -> Input -> Mapper data. Format is defined in their respective classes, not 100% guaranteed to be compatible between versions.

void GBCore::writeState(std::ostream& os) const
{
	std::ostringstream st{};

	const auto checksum { cartridge.getChecksum() };
	ST_WRITE(checksum);

	const auto romFilePathStr { FileUtils::pathToUTF8(romFilePath) };
	const auto filePathLen { static_cast<uint16_t>(romFilePathStr.length()) };

	ST_WRITE(filePathLen);
	st.write(romFilePathStr.data(), filePathLen);

	writeFrameBuffer(st); 

	std::ostringstream gbSt{};
	writeGBState(gbSt);

	const auto uncompressedData { gbSt.view().data() };
	const auto uncompressedSize { static_cast<uint32_t>(gbSt.tellp()) };

	mz_ulong compressedSize { mz_compressBound(uncompressedSize) };
	std::vector<uint8_t> compressedBuffer(compressedSize);

	const auto dataPtr { reinterpret_cast<const unsigned char*>(uncompressedData) };
	const int status { mz_compress(compressedBuffer.data(), &compressedSize, dataPtr, uncompressedSize) };

	const bool isCompressed { status == MZ_OK };
	ST_WRITE(isCompressed);

	if (isCompressed)
	{
		ST_WRITE(uncompressedSize);
		st.write(reinterpret_cast<const char*>(compressedBuffer.data()), compressedSize);
	}
	else
		st.write(uncompressedData, uncompressedSize);

	os.write(SAVE_STATE_SIGNATURE.data(), SAVE_STATE_SIGNATURE.length());

	const auto streamData { st.view().data() };
	const auto dataSize { static_cast<size_t>(st.tellp()) };

	const uint64_t hash { calculateHash({ reinterpret_cast<const uint8_t*>(streamData), dataSize }) };
	os.write(reinterpret_cast<const char*>(&hash), sizeof(hash));
	os.write(streamData, dataSize);
}

std::vector<uint8_t> GBCore::getStateData(std::istream& st)
{
	uint64_t storedHash;
	ST_READ(storedHash);

	std::vector<uint8_t> buffer(FileUtils::remainingBytes(st));
	st.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

	if (calculateHash(buffer) != storedHash)
		return {};

	return buffer;
}

FileLoadResult GBCore::loadState(std::istream& is)
{
	const auto buffer { getStateData(is) };

	if (buffer.empty())
		return FileLoadResult::CorruptSaveState;

	memstream st { buffer };

	uint8_t stateRomChecksum;
	ST_READ(stateRomChecksum);

	uint16_t filePathLen { 0 };
	ST_READ(filePathLen);

	std::string romPath(filePathLen, 0);
	st.read(romPath.data(), filePathLen);

	if (!cartridge.ROMLoaded() || cartridge.getChecksum() != stateRomChecksum)
	{
		if (!validateAndLoadRom(romPath, stateRomChecksum))
			return FileLoadResult::ROMNotFound;
	}

	const uint32_t framebufDataOffset { static_cast<uint32_t>(st.tellg()) };

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
		readGBState(st);
	else
	{
		mz_ulong uncompressedSize { 0 };
		st.read(reinterpret_cast<char*>(&uncompressedSize), sizeof(uint32_t));

		const auto compressedSize { static_cast<mz_ulong>(FileUtils::remainingBytes(st)) };

		std::vector<uint8_t> compressedBuffer(compressedSize);
		st.read(reinterpret_cast<char*>(compressedBuffer.data()), compressedSize);

		std::vector<uint8_t> buffer(uncompressedSize);
		const auto bufPtr { reinterpret_cast<unsigned char*>(buffer.data()) };
		const int status { mz_uncompress(bufPtr, &uncompressedSize, compressedBuffer.data(), compressedSize) };

		if (status != MZ_OK)
			return FileLoadResult::CorruptSaveState;

		memstream ms { buffer };
		readGBState(ms);
	}

	// For the first frame not to be as teared.
	st.seekg(framebufDataOffset, std::ios::beg);
	loadFrameBuffer(st, { ppu->backbufferPtr(), PPU::FRAMEBUFFER_SIZE });

	if (drawCallback != nullptr)
		drawCallback(ppu->backbufferPtr(), true);

	return FileLoadResult::SuccessSaveState;
}

void GBCore::writeGBState(std::ostream& st) const
{
	const auto system { System::Current() };
	ST_WRITE(system);
	ST_WRITE(cycleCounter);

	cpu.saveState(st);
	ppu->saveState(st);
	mmu.saveState(st);
	apu.saveState(st);
	serial.saveState(st);
	joypad.saveState(st);
	cartridge.getMapper()->saveState(st); // Mapper must be saved last.
}
void GBCore::readGBState(std::istream& st)
{
	GBSystem system;
	ST_READ(system);

	if (System::Current() != system)
	{
		System::Set(system);
		updatePPUSystem();
		mmu.updateSystem();
	}

	// Passing false to fullReset, because boot rom shouldn't be loaded and system should stay the one specified in save state.
	// Also passing false to clearBuf, since save state thumbnail will be used as first frame instead of blank frame. 
	reset(true, false, false);

	ST_READ(cycleCounter);

	cpu.loadState(st);
	ppu->loadState(st);
	mmu.loadState(st);
	apu.loadState(st);
	serial.loadState(st);
	joypad.loadState(st);
	cartridge.getMapper()->loadState(st);
}

bool GBCore::loadSaveStateThumbnail(const std::filesystem::path& path, std::span<uint8_t> framebuffer) const
{
	if (!cartridge.ROMLoaded())
		return false;

	std::ifstream ifs { path, std::ios::in | std::ios::binary };

	if (!ifs || !isSaveStateFile(ifs))
		return false;

	const auto buffer { getStateData(ifs) };

	if (buffer.empty())
		return false;

	memstream st { buffer };

	uint8_t saveStateChecksum;
	ST_READ(saveStateChecksum);

	if (cartridge.getChecksum() != saveStateChecksum)
		return false;

	uint16_t filePathLen { 0 };
	ST_READ(filePathLen);
	st.seekg(filePathLen, std::ios::cur);

	return loadFrameBuffer(st, framebuffer);
}