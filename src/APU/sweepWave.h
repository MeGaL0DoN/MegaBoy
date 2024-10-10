#pragma once
#include "squareWave.h"
#include "../Utils/bitOps.h"

struct sweepWave : public squareWave
{
	uint8_t NR10 {};

	//state s{};

	void reset()
	{
		squareWave::reset();
	}

	void trigger() 
	{
		squareWave::trigger();

		uint8_t sweepPeriod = (NR10 >> 4) & 0b111;
		uint8_t sweepShift = NR10 & 0b111;

		shadowFrequency = getFrequency();

		sweepTimer = sweepPeriod == 0 ? 8 : sweepPeriod;
		sweepEnabled = sweepPeriod != 0 || sweepShift != 0;

		if (sweepShift != 0)
			calculateFrequency();
	}

	void executeSweep()
	{
		if (sweepTimer > 0)
		{
			sweepTimer--;

			if (sweepTimer == 0)
			{
				uint8_t sweepPeriod = (NR10 >> 4) & 0b111;
				sweepTimer = sweepPeriod == 0 ? 8 : sweepPeriod;

				if (sweepEnabled && sweepPeriod > 0)
				{
					uint16_t newFrequency = calculateFrequency();

					if (newFrequency < 2048 && (NR10 & 0b111) > 0)
					{
						shadowFrequency = newFrequency;
						NRx3 = newFrequency & 0xFF;
						NRx4 = (NRx4 & 0b11111000) | (newFrequency >> 8); //|= ((newFrequency & 0x700) >> 8); //

						calculateFrequency();
					}
				}
			}
		}
	}

	uint16_t calculateFrequency()
	{
		uint16_t newFrequency = shadowFrequency >> (NR10 & 0b111);

		if (getBit(NR10, 3))
			newFrequency = shadowFrequency - newFrequency;
		else
			newFrequency = shadowFrequency + newFrequency;

		if (newFrequency > 2047)
			s.triggered = false;

		return newFrequency;
	}

private:
	uint16_t shadowFrequency{};
	bool sweepEnabled{};
	uint8_t sweepTimer{};
};