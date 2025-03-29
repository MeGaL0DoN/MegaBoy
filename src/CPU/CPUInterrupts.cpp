#include "CPU.h"
#include "../Utils/bitOps.h"
#include "CPUInstructions.h"
#include "../GBCore.h"

void CPU::handleInterrupts()
{
	if (!pendingInterrupt()) [[likely]]
		return;

	if (s.IME)
	{
		addCycle();
		addCycle();
		write8(--s.SP.val, s.PC >> 8);
		addCycle();

		const uint8_t interrupt { pendingInterrupt() };

		write8(--s.SP.val, s.PC & 0xFF);

		if (interrupt) [[likely]]
		{
 			const uint8_t interrruptBit { static_cast<uint8_t>(std::countr_zero(interrupt)) };
			s.PC = 0x0040 + interrruptBit * 8;
			s.IF = resetBit(s.IF, interrruptBit);
		}
		else [[unlikely]]
			s.PC = 0x00;

		s.IME = false;
		s.shouldSetIME = false;
	}

	exitHalt();
}

void CPU::requestInterrupt(Interrupt interrupt)
{
	s.IF = setBit(s.IF, static_cast<uint8_t>(interrupt));
}

constexpr std::array<uint8_t, 4> TIMA_BITS { 9, 3, 5, 7 };

bool CPU::detectTimaOverflow()
{
	const bool timerEnabled = getBit(s.tacReg, 2);
	const uint8_t timerBit { TIMA_BITS[s.tacReg & 0b11] };
	const bool newDivBit { getBit(s.divCounter, timerBit) && timerEnabled };

	bool overflow { false };

	if (!newDivBit && s.oldDivBit)
		overflow = (++s.timaReg == 0);

	s.oldDivBit = newDivBit;
	return overflow;
}

void CPU::executeTimer()
{
	if (!s.stopState) [[likely]]
		s.divCounter += 4;

	s.timaOverflowed = false;

	if (s.timaOverflowDelay)
	{
		requestInterrupt(Interrupt::Timer);
		s.timaReg = s.tmaReg;
		s.timaOverflowDelay = false;
		s.timaOverflowed = true;
	}

	s.timaOverflowDelay = detectTimaOverflow();
}

// TAC reg writes cause immediate falling edge detection so interrupt can be requested immediately, without 1M cycle delay as usual.
void CPU::writeTacReg(uint8_t val)
{
	s.tacReg = val;

	if (detectTimaOverflow())
	{
		requestInterrupt(Interrupt::Timer);
		s.timaReg = s.tmaReg;
	}
}