#include "GBCore.h"

void GBCore::update()
{
	uint8_t cycles = cpu.execute();
	cpu.updateTimer(cycles);
	//ppu.execute(cycles);
	cpu.handleInterrupts();
}