#include "GBCore.h"

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
	apu.reset();
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
	ppu.execute();
	serial.execute();
}