#include "CPU.h"
#include "bitOps.h"
#include "instructionsEngine.h"
extern InstructionsEngine instructions;

constexpr uint8_t interruptSources[5] = { 0x40, 0x48, 0x50, 0x58, 0x60 };

uint8_t CPU::handleInterrupts()
{
	uint8_t IE = mmu.directRead(0xFFFF);
	uint8_t IF = mmu.directRead(0xFF0F);

	if (IE & IF & 0x1F)
	{
		if (IME)
		{
			for (int i = 0; i < sizeof(interruptSources); i++)
			{
				if (getBit(IE, i) && getBit(IF, i))
				{
					halted = false;
					instructions.PUSH(PC);
					PC = interruptSources[i];
					mmu.directWrite(0xFF0F, resetBit(IF, i));
					IME = false;
					return 5; // m cycles processing interrupt
				}
			}
		}
		else if (halted)
		{
			// TODO halt bug
			halted = false;
			halt_bug = true;
		}
	}

	return 0;
}

void CPU::requestInterrupt(Interrupt interrupt)
{
	uint8_t IF = mmu.directRead(0xFF0F);
	mmu.directWrite(0xFF0F, setBit(IF, static_cast<uint8_t>(interrupt)));
}

constexpr uint16_t DIV_ADDR = 0xFF04;
constexpr uint16_t TIMA_ADDR = 0xFF05;
constexpr uint16_t TMA_ADDR = 0xFF06;
constexpr uint16_t TAC_ADDR = 0xFF07;

constexpr uint16_t TIMAcycles[4] = { 256, 4, 16, 64 };

void CPU::updateTimer(uint8_t cycles)
{
	DIV += cycles;
	if (DIV >= 64)
	{
		DIV -= 64;
		mmu.directWrite(DIV_ADDR, mmu.directRead(DIV_ADDR) + 1);
	}

	uint8_t TAC = mmu.directRead(TAC_ADDR);
	if (getBit(TAC, 2))
	{
		TIMA += cycles;
		uint16_t currentTIMAspeed = TIMAcycles[TAC & 0x03];

		while (TIMA >= currentTIMAspeed) 
		{
			TIMA -= currentTIMAspeed;
			uint8_t tima_val = mmu.directRead(TIMA_ADDR) + 1;

			if (tima_val == 0)
			{
				tima_val = mmu.directRead(TMA_ADDR);
				requestInterrupt(Interrupt::Timer);
			}

			mmu.directWrite(TIMA_ADDR, tima_val);
		}
	}
}