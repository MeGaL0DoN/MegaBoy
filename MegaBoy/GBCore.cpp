﻿#include "GBCore.h"
#include "appConfig.h"
#include <fstream>
#include <string>
#include <chrono>

GBCore gbCore{};

void GBCore::reset()
{
	emulationPaused = false;

	input.reset();
	serial.reset();
	cpu.reset();
	ppu.reset();
	mmu.reset();
	apu.reset();
}

void GBCore::loadBootROM()
{
	cpu.disableBootROM();

	if (appConfig::runBootROM)
	{
		const auto romPath = StringUtils::executableFolderPath / "data" / (System::Current() == GBSystem::DMG ? "dmg_boot.bin" : "cgb_boot.bin");
		std::ifstream ifs(romPath, std::ios::binary | std::ios::ate);

		if (ifs)
		{
			std::ifstream::pos_type pos = ifs.tellg();

			if (System::Current() == GBSystem::DMG)
			{
				if (pos == sizeof(mmu.base_bootROM))
				{
					ifs.seekg(0, std::ios::beg);
					ifs.read(reinterpret_cast<char*>(&mmu.base_bootROM[0]), pos);
				}
				else
					return;
			}
			else if (System::Current() == GBSystem::GBC)
			{
				uint16_t size = pos;

				ifs.seekg(0, std::ios::beg);
				ifs.read(reinterpret_cast<char*>(&mmu.base_bootROM[0]), sizeof(mmu.base_bootROM));

				if (size == 2048)
					ifs.seekg(sizeof(mmu.base_bootROM), std::ios::beg);
				else if (size == 2304)
					ifs.seekg(0x200, std::ios::beg);
				else
					return;

				ifs.read(reinterpret_cast<char*>(&mmu.GBCbootROM[0]), sizeof(mmu.GBCbootROM));
			}

			// LCD disabled on boot ROM start
			ppu.regs.LCDC = resetBit(ppu.regs.LCDC, 7);
			cpu.enableBootROM();
		}
	}
}

void GBCore::update(uint32_t cyclesToExecute)
{
	if (!cartridge.ROMLoaded || emulationPaused) return;

	uint32_t currentCycles { 0 };

	while (currentCycles < cyclesToExecute)
	{
		uint8_t cycles = cpu.execute() * (cpu.doubleSpeed() ? 2 : 4);
		currentCycles += cycles;

		if (cartridge.hasTimer)
			cartridge.timer.addRTCcycles(cycles);
	}
}

void GBCore::stepComponents()
{
	cpu.updateTimer();
	ppu.execute();
	mmu.execute();
	//apu.execute();
	serial.execute();
}

void GBCore::restartROM(bool resetBattery)
{
	if (!cartridge.ROMLoaded)
		return;

	reset();
	cartridge.getMapper()->reset(resetBattery);
	loadBootROM();
}

bool GBCore::isSaveStateFile(std::ifstream& st)
{
	std::string filePrefix(SAVE_STATE_SIGNATURE.length(), 0);
	st.read(filePrefix.data(), SAVE_STATE_SIGNATURE.length());
	return filePrefix == SAVE_STATE_SIGNATURE;
}

inline std::filesystem::path replaceExtension(std::filesystem::path path, const char* newExt)
{
	path.replace_extension(newExt);
	return path;
}

FileLoadResult GBCore::loadFile(std::ifstream& st)
{
	FileLoadResult result;
	bool success { true };

	saveCurrentROM();

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

			const auto gbRomPath = replaceExtension(filePath, ".gb");
			const auto gbcRomPath = replaceExtension(filePath, ".gbc");

			auto loadRomAndBattery = [this, &st](const std::filesystem::path& romPath) -> bool
			{
				if (std::ifstream ifs{ romPath, std::ios::in | std::ios::binary })
				{
					if (loadROM(ifs, romPath))
					{
						cartridge.getMapper()->loadBattery(st);
						loadBootROM();
						return true;
					}
				}

				return false;
			};

			if (loadRomAndBattery(gbRomPath)) ;
			else if (loadRomAndBattery(gbcRomPath)) ;

			else if (cartridge.ROMLoaded && cartridge.hasBattery) // TODO: somehow check if the save file is valid for the current ROM
			{
				currentSave = 0;
				restartROM();
				cartridge.getMapper()->loadBattery(st);
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
					const auto batterySavePath = replaceExtension(filePath, ".sav");

					if (std::ifstream ifs{ batterySavePath, std::ios::in | std::ios::binary })
						cartridge.getMapper()->loadBattery(ifs);
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
		saveFolderPath = StringUtils::executableFolderPath / "saves" / (gbCore.gameTitle + " (" + std::to_string(cartridge.checksum) + ")");
		emulationPaused = false;
		appConfig::updateConfigFile();
		return FileLoadResult::Success;
	}

	return result;
}

void GBCore::autoSave()
{
	if (!cartridge.ROMLoaded || currentSave == 0) return;
	saveState(getSaveFilePath(currentSave));
}

void GBCore::batteryAutoSave()
{
	if (cartridge.hasBattery && appConfig::batterySaves)
	{
		auto batterySavePath = replaceExtension(romFilePath, ".sav");

		if (appConfig::backupSaves && std::filesystem::exists(batterySavePath))
		{
			auto batteryBackupPath { batterySavePath };
			batteryBackupPath.replace_filename(StringUtils::pathToUTF8(batterySavePath.stem()) + " - BACKUP.sav");
			std::filesystem::copy_file(batterySavePath, batteryBackupPath, std::filesystem::copy_options::overwrite_existing);
		}

		saveBattery(batterySavePath);
	}
}

void GBCore::backupSave(int num)
{
	if (!appConfig::backupSaves)
		return;

	const auto saveFilePath = getSaveFilePath(num);

	if (!cartridge.ROMLoaded || !std::filesystem::exists(saveFilePath))
		return;

	const std::chrono::zoned_time time { std::chrono::current_zone(), std::chrono::system_clock::now() };
	const auto timeStr = std::format("{:%d-%m-%Y %H-%M-%OS}", time);

	const auto backupPath = saveFolderPath / "backups";

	if (!std::filesystem::exists(backupPath))
		std::filesystem::create_directories(backupPath);

	std::filesystem::copy_file(saveFilePath, backupPath / ("save" + std::to_string(num) + " (" + timeStr + ").mbs"), std::filesystem::copy_options::overwrite_existing);
}

void GBCore::loadState(int num)
{
	if (!cartridge.ROMLoaded) return;

	if (currentSave != 0)
		saveState(currentSave);

	const auto _filePath = getSaveFilePath(num);
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

	const auto _filePath = getSaveFilePath(num);

	if (currentSave != num && std::filesystem::exists(_filePath))
		backupSave(num);

	saveState(_filePath);

	if (currentSave == 0)
		updateSelectedSaveInfo(num);
}

void GBCore::saveState(std::ofstream& st)
{
	if (!std::filesystem::exists(saveFolderPath))
		std::filesystem::create_directories(saveFolderPath);

	st << SAVE_STATE_SIGNATURE;

	auto romFilePathStr = StringUtils::pathToUTF8(romFilePath);

	uint16_t filePathLen { static_cast<uint16_t>(romFilePathStr.length()) };
	st.write(reinterpret_cast<char*>(&filePathLen), sizeof(filePathLen));
	st << romFilePathStr;

	st.write(reinterpret_cast<char*>(&cartridge.checksum), sizeof(cartridge.checksum));

	auto system = System::Current();
	st.write(reinterpret_cast<char*>(&system), sizeof(system));

	mmu.saveState(st);
	cpu.saveState(st);
	ppu.saveState(st);
	serial.saveState(st);
	input.saveState(st);
	cartridge.getMapper()->saveState(st);
}

bool GBCore::loadState(std::ifstream& st)
{
	uint16_t filePathLen { 0 };
	st.read(reinterpret_cast<char*>(&filePathLen), sizeof(filePathLen));

	std::string romPath(filePathLen, 0);
	st.read(romPath.data(), filePathLen);

	uint32_t checkSum;
	st.read(reinterpret_cast<char*>(&checkSum), sizeof(checkSum));

	GBSystem system;
	st.read(reinterpret_cast<char*>(&system), sizeof(system));

	if (!gbCore.cartridge.ROMLoaded || gbCore.cartridge.checksum != checkSum)
	{
		saveCurrentROM();
		bool romExists{ true };
		
		auto nativeRomPath = std::filesystem::path(StringUtils::nativePath(romPath));
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
	gbCore.reset();

	cpu.disableBootROM();

	mmu.loadState(st);
	cpu.loadState(st);
	ppu.loadState(st);
	serial.loadState(st);
	input.loadState(st);
	cartridge.getMapper()->loadState(st);

	return true;
}