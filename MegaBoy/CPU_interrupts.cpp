#include "CPU.h"
#include "bitOps.h"
#include "instructionsEngine.h"
#include "GBCore.h"
extern InstructionsEngine instructions;

constexpr std::array<uint8_t, 5> interruptSources = { 0x40, 0x48, 0x50, 0x58, 0x60 };

bool CPU::interruptsPending()
{
	uint8_t IE = gbCore.mmu.directRead(0xFFFF);
	uint8_t IF = gbCore.mmu.directRead(0xFF0F);
	return IE & IF & 0x1F;
}

uint8_t CPU::handleInterrupts()
{
	uint8_t IE = gbCore.mmu.directRead(0xFFFF);
	uint8_t IF = gbCore.mmu.directRead(0xFF0F);

	if (interruptsPending())
	{
		if (IME)
		{
			for (int i = 0; i < sizeof(interruptSources); i++)
			{
				if (getBit(IE, i) && getBit(IF, i))
				{
					cycles = 0;
					halted = false;
					instructions.PUSH(PC);
					PC = interruptSources[i];
					gbCore.mmu.directWrite(0xFF0F, resetBit(IF, i));
					addCycles(2); // PUSH adds 3, need 5 in total.
					IME = false;
					return cycles;
				}
			}
		}
		else
		{
			// halt bug
			halted = false;
		}
	}

	return 0;
}

void CPU::requestInterrupt(Interrupt interrupt)
{
	uint8_t IF = gbCore.mmu.directRead(0xFF0F);
	gbCore.mmu.directWrite(0xFF0F, setBit(IF, static_cast<uint8_t>(interrupt)));
}

constexpr uint16_t DIV_ADDR = 0xFF04;
constexpr uint16_t TIMA_ADDR = 0xFF05;
constexpr uint16_t TMA_ADDR = 0xFF06;
constexpr uint16_t TAC_ADDR = 0xFF07;

constexpr std::array<uint16_t, 4> TIMAcycles = { 256, 4, 16, 64 };

void CPU::updateTimer()
{
	DIV++;
	if (DIV >= 64)
	{
		DIV -= 64;
		gbCore.mmu.directWrite(DIV_ADDR, gbCore.mmu.directRead(DIV_ADDR) + 1);
	}

	uint8_t TAC = gbCore.mmu.directRead(TAC_ADDR);
	if (getBit(TAC, 2))
	{
		TIMA++;
		uint16_t currentTIMAspeed = TIMAcycles[TAC & 0x03];

		while (TIMA >= currentTIMAspeed) 
		{
			TIMA -= currentTIMAspeed;
			uint8_t tima_val = gbCore.mmu.directRead(TIMA_ADDR) + 1;

			if (tima_val == 0)
			{
				tima_val = gbCore.mmu.directRead(TMA_ADDR);
				requestInterrupt(Interrupt::Timer);
			}

			gbCore.mmu.directWrite(TIMA_ADDR, tima_val);
		}
	}
}