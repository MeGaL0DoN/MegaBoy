#include "GBCore.h"

void GBCore::update()
{
	if (!mmu.ROMLoaded || paused) return;
	int totalCycles {0};

	while (totalCycles < CYCLES_PER_FRAME)
	{
		totalCycles += cpu.execute();
		totalCycles += cpu.handleInterrupts();
	}
}

void GBCore::stepComponents()
{
	cpu.updateTimer();
	ppu.execute();
	serial.execute();
}