#pragma once
#include "squareWave.h"
#include "../Utils/bitOps.h"

struct sweepWaveRegs : squareWaveRegs
{
	std::atomic<uint8_t> NR10;
};

struct sweepWaveState : squareWaveState
{
	uint16_t shadowFrequency{};
	bool sweepEnabled{};
	uint8_t sweepTimer{};
};

struct sweepWave : public squareWave<sweepWaveState, sweepWaveRegs>
{
	void reset()
	{
		squareWave::reset();

		regs.NR10 = 0x80;
		regs.NRx1 = 0xBF;
		regs.NRx2 = 0xF3;
	}

	void trigger() 
	{
		squareWave::trigger();

		uint8_t sweepPeriod = (regs.NR10 >> 4) & 0b111;
		uint8_t sweepShift = regs.NR10 & 0b111;

		s.shadowFrequency = getFrequency();

		s.sweepTimer = sweepPeriod == 0 ? 8 : sweepPeriod;
		s.sweepEnabled = sweepPeriod != 0 || sweepShift != 0;

		if (sweepShift != 0)
			calculateFrequency();
	}

	void executeSweep()
	{
		if (s.sweepTimer > 0)
		{
			s.sweepTimer--;

			if (s.sweepTimer == 0)
			{
				uint8_t sweepPeriod = (regs.NR10 >> 4) & 0b111;
				s.sweepTimer = sweepPeriod == 0 ? 8 : sweepPeriod;

				if (s.sweepEnabled && sweepPeriod > 0)
				{
					uint16_t newFrequency = calculateFrequency();

					if (newFrequency < 2048 && (regs.NR10 & 0b111) > 0)
					{
						s.shadowFrequency = newFrequency;
						regs.NRx3 = newFrequency & 0xFF;
						regs.NRx4 = (regs.NRx4 & 0b11111000) | (newFrequency >> 8); 

						calculateFrequency();
					}
				}
			}
		}
	}

	uint16_t calculateFrequency()
	{
		uint16_t newFrequency = s.shadowFrequency >> (regs.NR10 & 0b111);
		const bool isDecrementing = getBit(regs.NR10.load(), 3);

		if (isDecrementing)
			newFrequency = s.shadowFrequency - newFrequency;
		else
			newFrequency = s.shadowFrequency + newFrequency;

		if (newFrequency > 2047)
			s.triggered = false;

		return newFrequency;
	}
};