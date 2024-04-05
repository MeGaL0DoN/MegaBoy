#include "GBCore.h"

void GBCore::update()
{
	int totalCycles{0};
	while (totalCycles < CYCLES_PER_FRAME)
	{
		uint8_t opcodeCycles = cpu.execute();
		cpu.updateTimer(opcodeCycles);
		ppu.execute(opcodeCycles);
		totalCycles += cpu.handleInterrupts();
		totalCycles += opcodeCycles;
	}
}