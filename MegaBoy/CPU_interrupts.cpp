#include "CPU.h"
#include "bitOps.h"
#include "instructionsEngine.h"
#include "GBCore.h"
extern InstructionsEngine instructions;

constexpr std::array<uint8_t, 5> interruptSources = { 0x40, 0x48, 0x50, 0x58, 0x60 };

bool CPU::interruptsPending()
{
	return s.IE & s.IF & 0x1F;
}

uint8_t CPU::handleInterrupts()
{
	if (interruptsPending())
	{
		if (s.IME)
		{
			for (int i = 0; i < sizeof(interruptSources); i++)
			{
				if (getBit(s.IE, i) && getBit(s.IF, i))
				{
					cycles = 0;
					s.halted = false;
					instructions.PUSH(s.PC);
					s.PC = interruptSources[i];
					s.IF = resetBit(s.IF, i);
					addCycles(2); // PUSH adds 3, need 5 in total.
					s.IME = false;
					return cycles;
				}
			}
		}
		else
		{
			// halt bug
			s.halted = false;
		}
	}

	return 0;
}

void CPU::requestInterrupt(Interrupt interrupt)
{
	s.IF = setBit(s.IF, static_cast<uint8_t>(interrupt));
}

constexpr std::array<uint16_t, 4> TIMAcycles = { 256, 4, 16, 64 };

void CPU::updateTimer()
{
	s.DIV_COUNTER++;
	if (s.DIV_COUNTER >= 64)
	{
		s.DIV_COUNTER -= 64;
		s.DIV_reg++;
	}

	if (getBit(s.TAC_reg, 2))
	{
		s.TIMA_COUNTER++;
		uint16_t currentTIMAspeed = TIMAcycles[s.TAC_reg & 0x03];

		while (s.TIMA_COUNTER >= currentTIMAspeed) 
		{
			s.TIMA_COUNTER -= currentTIMAspeed;
			s.TIMA_reg++;

			if (s.TIMA_reg == 0)
			{
				s.TIMA_reg = s.TMA_reg;
				requestInterrupt(Interrupt::Timer);
			}
		}
	}
}