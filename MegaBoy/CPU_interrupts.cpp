#include "CPU.h"
#include "bitOps.h"
#include "instructionsEngine.h"
#include "GBCore.h"
extern InstructionsEngine instructions;

constexpr std::array<uint8_t, 5> interruptSources = { 0x40, 0x48, 0x50, 0x58, 0x60 };

bool CPU::interruptsPending()
{
	return IE & IF & 0x1F;
}

uint8_t currentSTAT;

uint8_t CPU::handleInterrupts()
{
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
					IF = resetBit(IF, i);
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
	IF = setBit(IF, static_cast<uint8_t>(interrupt));
}

constexpr std::array<uint16_t, 4> TIMAcycles = { 256, 4, 16, 64 };

void CPU::updateTimer()
{
	DIV_COUNTER++;
	if (DIV_COUNTER >= 64)
	{
		DIV_COUNTER -= 64;
		DIV_reg++;
	}

	if (getBit(TAC_reg, 2))
	{
		TIMA_COUNTER++;
		uint16_t currentTIMAspeed = TIMAcycles[TAC_reg & 0x03];

		while (TIMA_COUNTER >= currentTIMAspeed) 
		{
			TIMA_COUNTER -= currentTIMAspeed;
			TIMA_reg++;

			if (TIMA_reg == 0)
			{
				TIMA_reg = TMA_reg;
				requestInterrupt(Interrupt::Timer);
			}
		}
	}
}