#include "CPU.h"
#include "../Utils/bitOps.h"
#include "CPUInstructions.h"
#include "../GBCore.h"

void CPU::handleInterrupts()
{
	if (s.IME && s.interruptLatch) [[unlikely]]
	{
		addCycle();
		addCycle();
		write8(--s.SP.val, s.PC >> 8);
		addCycle();

		const uint8_t interrupt { pendingInterrupt() };

		write8(--s.SP.val, s.PC & 0xFF);

		s.IME = false;
		s.shouldSetIME = false;
		exitHalt();

		if (interrupt) [[likely]]
		{
 			const uint8_t interrruptBit { static_cast<uint8_t>(std::countr_zero(interrupt)) };
			s.PC = 0x0040 + interrruptBit * 8;
			s.IF = resetBit(s.IF, interrruptBit);
		}
		else
			s.PC = 0x00;
	}
	else if (pendingInterrupt()) [[unlikely]]
		exitHalt();
}

void CPU::requestInterrupt(Interrupt interrupt)
{
	s.IF = setBit(s.IF, static_cast<uint8_t>(interrupt));
}

constexpr std::array<uint16_t, 4> TIMAcycles = { 256, 4, 16, 64 };

void CPU::updateTimer()
{
	if (!s.stopState)
	{
		s.DIV_COUNTER++;

		if (s.DIV_COUNTER >= 64)
		{
			s.DIV_COUNTER -= 64;
			s.DIV_reg++;
		}
	}

	if (!getBit(s.TAC_reg, 2))
		return;

	s.TIMA_COUNTER++;
	const uint16_t currentTIMAspeed { TIMAcycles[s.TAC_reg & 0x03] };

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