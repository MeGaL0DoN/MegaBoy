#include "GBCore.h"
#include <fstream>
#include <string>

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

bool saveStatePending{ false };

void GBCore::update(int cyclesToExecute)
{
	if (!cartridge.ROMLoaded || paused) return;

	int currentCycles { 0 };

	while (currentCycles < cyclesToExecute)
	{
		currentCycles += cpu.execute();
		currentCycles += cpu.handleInterrupts();

		if (saveStatePending)
			saveState(std::move(currentSaveSt));
	}
}

void GBCore::stepComponents()
{
	cpu.updateTimer();
	mmu.executeDMA();
	ppu.execute();
	serial.execute();
}

void ppuVBlankEnd()
{
	saveStatePending = true;
	gbCore.ppu.VBlankEndCallback = nullptr;
}

void GBCore::restartROM()
{
	if (!cartridge.ROMLoaded)
		return;

	reset();
	cartridge.getMapper()->reset();
	loadBootROM();
}

void GBCore::loadFile(std::ifstream& st)
{
	std::string filePrefix(SAVE_STATE_SIGNATURE.length(), 0);
	st.read(filePrefix.data(), SAVE_STATE_SIGNATURE.length());

	if (filePrefix == SAVE_STATE_SIGNATURE)
		loadState(st);
	else
	{
		if (cartridge.loadROM(st))
		{
			romFilePath = filePath;
			loadBootROM();
		}
	}
}

void GBCore::saveState(std::ofstream&& st)
{
	if (!saveStatePending && !gbCore.paused)
	{
		currentSaveSt = std::move(st);
		ppu.VBlankEndCallback = ppuVBlankEnd;
		return;
	}

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

	saveStatePending = false;
	st.close();
}

void GBCore::loadState(std::ifstream& st)
{
	uint16_t filePathLen { 0 };
	st.read(reinterpret_cast<char*>(&filePathLen), sizeof(filePathLen));

	std::string romPath(filePathLen, 0);
	st.read(romPath.data(), filePathLen);

	uint32_t checkSum;
	st.read(reinterpret_cast<char*>(&checkSum), sizeof(checkSum));

	if (!gbCore.cartridge.ROMLoaded || gbCore.cartridge.checksum != checkSum)
	{
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
		{
			std::cout << "Rom doesn't exist! \n";
			return;
		}
	}

	std::cout << "successfully loaded! \n";

	mmu.loadState(st);
	cpu.loadState(st);
	ppu.loadState(st);
	serial.loadState(st);
	input.loadState(st);
	cartridge.getMapper()->loadState(st);
}