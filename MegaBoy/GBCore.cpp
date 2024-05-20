#include "GBCore.h"
#include <fstream>
#include <string>
#include <chrono>

GBCore gbCore{};

GBCore::GBCore()
{
	runBootROM = std::filesystem::exists("data/boot_rom.bin");
}

void GBCore::reset()
{
	paused = false;

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

	if (runBootROM)
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

void GBCore::update(int cyclesToExecute)
{
	if (!cartridge.ROMLoaded || paused) return;

	int currentCycles { 0 };

	while (currentCycles < cyclesToExecute)
	{
		currentCycles += cpu.execute();
		currentCycles += cpu.handleInterrupts();
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

FileLoadResult GBCore::loadFile(std::ifstream& st)
{
	std::string filePrefix(SAVE_STATE_SIGNATURE.length(), 0);
	st.read(filePrefix.data(), SAVE_STATE_SIGNATURE.length());

	FileLoadResult result;
	bool success { true };

	if (filePrefix == SAVE_STATE_SIGNATURE)
	{
		if (loadState(st))
			result = FileLoadResult::SuccessState;
		else
		{
			result = FileLoadResult::SaveStateROMNotFound;
			success = false;
		}
	}
	else
	{
		saveCurrentROM();

		if (filePath.ends_with(".sav"))
		{
			if (cartridge.ROMLoaded && cartridge.hasBattery)
			{
				restartROM();
				st.seekg(0, std::ios::beg);
				cartridge.getMapper()->loadBattery(st);
				result = FileLoadResult::SuccessState;
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
				loadBootROM();
				result = FileLoadResult::SuccessROM;
			}
			else
			{
				result = FileLoadResult::InvalidROM;
				success = false;
			}
		}
	}

	if (success)
		saveFolderName = "saves/" + gbCore.gameTitle + " (" + std::to_string(cartridge.checksum) + ")";

	return result;
}

void GBCore::autoSave()
{
	if (!cartridge.ROMLoaded) return;
	saveState(saveFolderName + "/autosave.mbs");
}

void GBCore::backupSave()
{
	if (!cartridge.ROMLoaded || !std::filesystem::exists(saveFolderName + "/autosave.mbs"))
		return;

	const std::chrono::zoned_time time { std::chrono::current_zone(), std::chrono::system_clock::now() };
	const std::string timeStr = std::format("{:%d-%m-%Y %H-%M-%OS}", time);

	const std::string backupPath = saveFolderName + "/backups";

	if (!std::filesystem::exists(backupPath))
		std::filesystem::create_directories(backupPath);

	std::filesystem::copy_file(saveFolderName + "/autosave.mbs", backupPath + "/" + timeStr + ".mbs");
}

void GBCore::saveState(std::ofstream& st)
{
	if (!cartridge.ROMLoaded || cpu.isExecutingBootROM())
		return;

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