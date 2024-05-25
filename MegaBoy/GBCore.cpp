#include "GBCore.h"
#include <fstream>
#include <string>
#include <chrono>

GBCore gbCore{};

GBCore::GBCore()
{
	options.runBootROM = std::filesystem::exists("data/boot_rom.bin");
}

void GBCore::reset()
{
	options.paused = false;

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

	if (options.runBootROM)
	{
		std::ifstream ifs("data/boot_rom.bin", std::ios::binary | std::ios::ate);

		if (ifs)
		{
			std::ifstream::pos_type pos = ifs.tellg();
			if (pos != 256) return;

			ifs.seekg(0, std::ios::beg);
			ifs.read(reinterpret_cast<char*>(&mmu.bootROM[0]), pos);

			// LCD disabled on boot ROM start
			ppu.regs.LCDC = resetBit(ppu.regs.LCDC, 7);
			cpu.enableBootROM();
		}
	}
}

void GBCore::update(int32_t cyclesToExecute)
{
	if (!cartridge.ROMLoaded || options.paused) return;

	int32_t currentCycles { 0 };

	while (currentCycles < cyclesToExecute)
	{
		int32_t prevCycles = currentCycles;

		currentCycles += cpu.execute();
		currentCycles += cpu.handleInterrupts();

		if (cartridge.hasTimer)
			cartridge.timer.addRTCcycles(currentCycles - prevCycles);
	}
}

void GBCore::stepComponents()
{
	cpu.updateTimer();
	mmu.executeDMA();
	ppu.execute();
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

inline auto replaceExtension(const std::string& source, const char* newExt)
{
	size_t lastindex = source.find_last_of(".");
	const auto newStr = source.substr(0, lastindex) + newExt;

	#ifdef _WIN32
		return StringUtils::ToUTF16(newStr.c_str());
	#else
		return std::move(newStr);
	#endif
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
		if (filePath.ends_with(".sav"))
		{
			st.seekg(0, std::ios::beg);

			const auto gbRomPath = replaceExtension(filePath, ".gb");
			const auto gbcRomPath = replaceExtension(filePath, ".gbc");

			auto loadRomAndBattery = [this, &st](const auto romPath) -> bool
			{
				if (std::ifstream ifs{ romPath, std::ios::in | std::ios::binary })
				{
					if (cartridge.loadROM(ifs))
					{
						cartridge.getMapper()->loadBattery(st);
						loadBootROM();

						#ifdef _WIN32
							romFilePath = StringUtils::ToUTF8(romPath);
						#else
							romFilePath = romPath;
						#endif

						return true;
					}
				}

				return false;
			};

			if (loadRomAndBattery(gbRomPath.c_str())) ;
			else if (loadRomAndBattery(gbcRomPath.c_str())) ;

			else if (cartridge.ROMLoaded && cartridge.hasBattery)
			{
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
			if (cartridge.loadROM(st))
			{
				romFilePath = filePath;
				currentSave = 0;

				if (cartridge.hasBattery && options.batterySaves)
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
		saveFolderName = "saves/" + gbCore.gameTitle + " (" + std::to_string(cartridge.checksum) + ")";
		return FileLoadResult::Success;
	}

	return result;
}

void GBCore::autoSave()
{
	if (!cartridge.ROMLoaded || currentSave == 0) return;
	saveState(saveFolderName + currentSaveName + ".mbs");
}

void GBCore::batteryAutoSave()
{
	if (cartridge.hasBattery && options.batterySaves)
	{
		const auto batterySavePath = replaceExtension(romFilePath, ".sav");

		if (std::filesystem::exists(batterySavePath))
		{
			#ifdef _WIN32
				const std::wstring batteryBackupPath = replaceExtension(romFilePath, "") + L" - BACKUP.sav";
			#else
				const std::string batteryBackupPath = replaceExtension(romFilePath, "") + " - BACKUP.sav";
			#endif

			std::filesystem::copy_file(batterySavePath, batteryBackupPath, std::filesystem::copy_options::overwrite_existing);
		}

		saveBattery(batterySavePath);
	}
}

void GBCore::backupSave(int num)
{
	const auto saveFilePath = saveFolderName + "/save" + std::to_string(num) + ".mbs";

	if (!cartridge.ROMLoaded || !std::filesystem::exists(saveFilePath))
		return;

	const std::chrono::zoned_time time { std::chrono::current_zone(), std::chrono::system_clock::now() };
	const auto timeStr = std::format("{:%d-%m-%Y %H-%M-%OS}", time);

	const auto backupPath = saveFolderName + "/backups";

	if (!std::filesystem::exists(backupPath))
		std::filesystem::create_directories(backupPath);

	std::filesystem::copy_file(saveFilePath, backupPath + currentSaveName + " (" + timeStr + ").mbs", std::filesystem::copy_options::overwrite_existing);
}

void GBCore::saveState(std::ofstream& st)
{
	if (!std::filesystem::exists(saveFolderName))
		std::filesystem::create_directories(saveFolderName);

	st << SAVE_STATE_SIGNATURE;

	uint16_t filePathLen { static_cast<uint16_t>(romFilePath.length()) };
	st.write(reinterpret_cast<char*>(&filePathLen), sizeof(filePathLen));
	st << romFilePath;

	st.write(reinterpret_cast<char*>(&cartridge.checksum), sizeof(cartridge.checksum));

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

	if (!gbCore.cartridge.ROMLoaded || gbCore.cartridge.checksum != checkSum)
	{
		saveCurrentROM();
		bool romExists{ true };

		#ifdef _WIN32
			std::ifstream romStream(StringUtils::ToUTF16(romPath.c_str()), std::ios::in | std::ios::binary);
		#else
			std::ifstream romStream(romPath, std::ios::in | std::ios::binary);
	    #endif

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
			romFilePath = romPath;
		else
			return false;
	}

	cpu.disableBootROM();

	mmu.loadState(st);
	cpu.loadState(st);
	ppu.loadState(st);
	serial.loadState(st);
	input.loadState(st);
	cartridge.getMapper()->loadState(st);

	return true;
}