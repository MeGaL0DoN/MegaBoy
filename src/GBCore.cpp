#include <fstream>
#include <string>
#include <miniz/miniz.h>

#include "GBCore.h"
#include "appConfig.h"
#include "Utils/fileUtils.h"
#include "Utils/memstream.h"
#include "debugUI.h"

GBCore gbCore{};

void GBCore::reset(bool resetBattery)
{
	input.reset();
	serial.reset();
	cpu.reset();
	ppu->reset();
	mmu.reset();
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
	cpu.disableBootROM();

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

	if (isSaveStateFile(st))
	{
		if (!loadState(st))
			return FileLoadResult::SaveStateROMNotFound;
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
	appConfig::updateConfigFile();
	return FileLoadResult::Success;
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
	{
		if (!std::filesystem::exists(saveStateFolderPath))
			std::filesystem::create_directories(saveStateFolderPath);

		saveState(getSaveStateFilePath(currentSave));
	}

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
	st << SAVE_STATE_SIGNATURE;

	const auto romFilePathStr { FileUtils::pathToUTF8(romFilePath) };

	const uint16_t filePathLen { static_cast<uint16_t>(romFilePathStr.length()) };
	ST_WRITE(filePathLen);
	st << romFilePathStr;

	const auto checksum { cartridge.getChecksum() };
	ST_WRITE(checksum);

	const auto system { System::Current() };
	ST_WRITE(system);

	mmu.saveState(st);
	cpu.saveState(st);
	ppu->saveState(st);
	serial.saveState(st);
	input.saveState(st);
	cartridge.getMapper()->saveState(st);
}

bool GBCore::loadState(std::istream& st)
{
	uint16_t filePathLen { 0 };
	ST_READ(filePathLen);

	std::string romPath(filePathLen, 0);
	st.read(romPath.data(), filePathLen);

	uint8_t saveStateChecksum;
	ST_READ(saveStateChecksum);

	GBSystem system;
	ST_READ(system);

	if (!cartridge.ROMLoaded() || cartridge.getChecksum() != saveStateChecksum)
	{
		const auto newRomPath = std::filesystem::path { FileUtils::nativePath(romPath) };
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

	System::Set(system);
	updatePPUSystem();
	reset(true);

	cpu.disableBootROM();

	mmu.loadState(st);
	cpu.loadState(st);
	ppu->loadState(st);
	serial.loadState(st);
	input.loadState(st);
	cartridge.getMapper()->loadState(st);

	return true;
}