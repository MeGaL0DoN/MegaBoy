#include "GBCore.h"

void GBCore::update(double deltaTime)
{
	if (!mmu.ROMLoaded || paused) return;

	const int cyclesToExecute { static_cast<int>((CYCLES_PER_FRAME * (deltaTime / FRAME_RATE))) };
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