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
	uint8_t sweepTimer{};
	bool sweepEnabled{};
};

struct sweepWave : public squareWave<sweepWaveState, sweepWaveRegs>
{
	void reset()
	{
		squareWave::reset();
		s.enabled = true;

		regs.NR10 = 0x80;
		regs.NRx1 = 0xBF;
		regs.NRx2 = 0xF3;
	}

	void trigger() 
	{
		squareWave::trigger();

		const uint8_t sweepPeriod = (regs.NR10 >> 4) & 0b111;
		const uint8_t sweepShift = regs.NR10 & 0b111;

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
				const uint8_t sweepPeriod = (regs.NR10 >> 4) & 0b111;
				s.sweepTimer = sweepPeriod == 0 ? 8 : sweepPeriod;

				if (s.sweepEnabled && sweepPeriod != 0)
				{
					const uint16_t newFrequency = calculateFrequency();
					const uint8_t sweepShift = (regs.NR10 & 0b111);

					if (newFrequency < 2048 && sweepShift != 0)
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
		const uint8_t sweepShift = (regs.NR10 & 0b111);
		const bool isDecrementing = getBit(regs.NR10.load(), 3);
		uint16_t newFrequency = s.shadowFrequency >> sweepShift;

		if (isDecrementing)
			newFrequency = s.shadowFrequency - newFrequency;
		else
			newFrequency = s.shadowFrequency + newFrequency;

		if (newFrequency > 2047)
			s.enabled = false;

		return newFrequency;
	}
};