#include <fstream>
#include <string>
#include <chrono>

#include "GBCore.h"
#include "appConfig.h"
#include "Utils/fileUtils.h"
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
	if (!cartridge.ROMLoaded || emulationPaused || breakpointHit) [[unlikely]] 
		return;

	const uint64_t targetCycles = cycleCounter + cyclesToExecute;

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

bool GBCore::isSaveStateFile(std::ifstream& st)
{
	std::string filePrefix(SAVE_STATE_SIGNATURE.length(), 0);
	st.read(filePrefix.data(), SAVE_STATE_SIGNATURE.length());
	return filePrefix == SAVE_STATE_SIGNATURE;
}

FileLoadResult GBCore::loadFile(std::ifstream& st)
{
	FileLoadResult result { FileLoadResult::Success };
	bool success { true };

	autoSave();

	if (isSaveStateFile(st))
	{
		if (!loadState(st))
		{
			result = FileLoadResult::SaveStateROMNotFound;
			success = false;
		}
	}
	else
	{
		if (filePath.extension() == ".sav")
		{
			st.seekg(0, std::ios::beg);

			const auto gbRomPath = FileUtils::replaceExtension(filePath, ".gb");
			const auto gbcRomPath = FileUtils::replaceExtension(filePath, ".gbc");

			auto loadRomAndBattery = [this, &st](const std::filesystem::path& romPath) -> bool
			{
				if (std::ifstream ifs{ romPath, std::ios::in | std::ios::binary })
				{
					if (loadROM(ifs, romPath))
					{
						loadBattery(st);
						loadBootROM();
						return true;
					}
				}

				return false;
			};

			if (loadRomAndBattery(gbRomPath) || loadRomAndBattery(gbcRomPath)) { }
			else if (cartridge.ROMLoaded && cartridge.hasBattery)
			{
				currentSave = 0;
				reset(false);
				loadBattery(st);
				loadBootROM();
			}
			else
			{
				result = FileLoadResult::SaveStateROMNotFound;
				success = false;
			}
		}
		else
		{
			if (loadROM(st, filePath))
			{
				if (cartridge.hasBattery && appConfig::batterySaves)
				{
					if (std::ifstream ifs{ getBatteryFilePath(filePath), std::ios::in | std::ios::binary })
						loadBattery(ifs);
				}

				loadBootROM();
			}
			else
			{
				result = FileLoadResult::InvalidROM;
				success = false;
			}
		}
	}

	if (success)
	{
		saveStateFolderPath = FileUtils::executableFolderPath / "saves" / (gbCore.gameTitle + " (" + std::to_string(cartridge.checksum) + ")");
		appConfig::updateConfigFile();
	}

	return result;
}

void GBCore::autoSave() const
{
	if (currentSave != 0 && appConfig::autosaveState)
		saveState(getSaveStateFilePath(currentSave));

	if (cartridge.hasBattery && appConfig::batterySaves)
	{
		if (cartridge.getMapper()->sramDirty)
		{
			saveBattery(getBatteryFilePath(romFilePath));
			cartridge.getMapper()->sramDirty = false;
		}
	}
}

void GBCore::backupBatteryFile() const
{
	if (!cartridge.hasBattery || !appConfig::batterySaves || !customBatterySavePath.empty())
		return;
	
	const auto batterySavePath{ getBatteryFilePath(romFilePath) };
	auto batteryBackupPath { batterySavePath };

	batteryBackupPath.replace_filename(FileUtils::pathToUTF8(batterySavePath.stem()) + " - BACKUP.sav");
	std::filesystem::copy_file(batterySavePath, batteryBackupPath, std::filesystem::copy_options::overwrite_existing);
}

void GBCore::loadState(int num)
{
	if (!cartridge.ROMLoaded) return;

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
	if (!cartridge.ROMLoaded) return;

	saveState(getSaveStateFilePath(num));

	if (currentSave == 0)
		updateSelectedSaveInfo(num);
}

void GBCore::saveState(std::ofstream& st) const
{
	if (!std::filesystem::exists(saveStateFolderPath))
		std::filesystem::create_directories(saveStateFolderPath);

	st << SAVE_STATE_SIGNATURE;

	const auto romFilePathStr = FileUtils::pathToUTF8(romFilePath);

	uint16_t filePathLen { static_cast<uint16_t>(romFilePathStr.length()) };
	ST_WRITE(filePathLen);
	st << romFilePathStr;

	ST_WRITE(cartridge.checksum);

	const auto system = System::Current();
	ST_WRITE(system);

	mmu.saveState(st);
	cpu.saveState(st);
	ppu->saveState(st);
	serial.saveState(st);
	input.saveState(st);
	cartridge.getMapper()->saveState(st);
}

bool GBCore::loadState(std::ifstream& st)
{
	uint16_t filePathLen { 0 };
	ST_READ(filePathLen);

	std::string romPath(filePathLen, 0);
	st.read(romPath.data(), filePathLen);

	uint8_t checkSum;
	ST_READ(checkSum);

	GBSystem system;
	ST_READ(system);

	if (!gbCore.cartridge.ROMLoaded || gbCore.cartridge.checksum != checkSum)
	{
		bool romExists{ true };
		
		const auto nativeRomPath = std::filesystem::path { FileUtils::nativePath(romPath) };
		std::ifstream romStream(nativeRomPath, std::ios::in | std::ios::binary);

		if (romStream)
		{
			gbCore.cartridge.loadROM(romStream);

			if (checkSum != gbCore.cartridge.checksum)
			{
				gbCore.cartridge.ROMLoaded = false;
				romExists = false;
			}
		}
		else
			romExists = false;

		if (romExists)
		{
			romFilePath = nativeRomPath;
			currentSave = 0;
		}
		else
			return false;
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