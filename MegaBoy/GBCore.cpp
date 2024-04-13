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

void GBCore::stepComponents(uint8_t steps)
{
	cpu.updateTimer(steps);
	ppu.execute(steps);
	serial.execute(steps);
}