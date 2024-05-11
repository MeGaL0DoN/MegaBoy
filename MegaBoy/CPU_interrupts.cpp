#include "CPU.h"
#include "bitOps.h"
#include "instructionsEngine.h"
#include "GBCore.h"
extern InstructionsEngine instructions;

bool CPU::interruptsPending()
{
	return s.IE & s.IF & 0x1F;
}

uint8_t CPU::handleInterrupts()
{
	if (s.IME && s.interruptLatch)
	{
		instructions.PUSH(s.PC);
		addCycles(2); // PUSH adds 3, need 5 in total.

		s.IME = false;
		s.halted = false;
		uint8_t interrupt = s.IE & s.IF;

		if (interrupt)
		{
			uint8_t interrruptBit = std::countr_zero(interrupt);
			s.PC = 0x0040 + interrruptBit * 8;
			s.IF = resetBit(s.IF, interrruptBit);
		}
		else
			s.PC = 0x00;

		return 5;
	}
	else if (interruptsPending())
	{
		// halt bug
		s.halted = false;
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