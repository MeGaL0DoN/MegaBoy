#include "GBCore.h"
#include <fstream>

GBCore gbCore{};

GBCore::GBCore()
{
	if (std::filesystem::exists("data/boot_rom.bin"))
		runBootROM = true;
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
	if (runBootROM && std::filesystem::exists("data/boot_rom.bin"))
	{
		std::ifstream ifs("data/boot_rom.bin", std::ios::binary | std::ios::ate);
		std::ifstream::pos_type pos = ifs.tellg();
		if (pos != 256) return;

		ifs.seekg(0, std::ios::beg);
		ifs.read(reinterpret_cast<char*>(&mmu.bootROM[0]), pos);

		// LCD disabled on boot ROM start
		ppu.regs.LCDC = resetBit(ppu.regs.LCDC, 7);
		cpu.enableBootROM();
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
			saveState();
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

void GBCore::saveState()
{
	if (!saveStatePending && !gbCore.paused)
	{
		ppu.VBlankEndCallback = ppuVBlankEnd;
		return;
	}

	std::ofstream st("savetest", std::ios::out | std::ios::binary);

	cartridge.getMapper()->saveState(st);
	mmu.saveState(st);
	cpu.saveState(st);
	ppu.saveState(st);
	serial.saveState(st);
	input.saveState(st);

	saveStatePending = false;
}

void GBCore::loadState()
{
	std::ifstream st("savetest", std::ios::in | std::ios::binary);

	cartridge.getMapper()->loadState(st);
	mmu.loadState(st);
	cpu.loadState(st);
	ppu.loadState(st);
	serial.loadState(st);
	input.loadState(st);
}