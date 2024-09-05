#pragma once
#include <cstdint>
#include <array>

#include "../Utils/bitOps.h"

struct squareWave
{
	uint8_t NRx1{};
	uint8_t NRx2{};
	uint8_t NRx3{};
	uint8_t NRx4{};

	inline void reset() { s = {}; }

	inline uint16_t getFrequency()
	{
		return NRx3 | ((NRx4 & 0b111) << 8);
	}

	inline void trigger()
	{
		s.lengthTimer = 64 - (NRx1 & 0b00111111);
		s.envelopePeriodTimer = NRx2 & 0b111;
		s.amplitude = (NRx2 >> 4) & 0b1111;
		s.triggered = true;

		s.freqPeriodTimer = 2048 - getFrequency();
		s.dutyStep = 0;
	}

	inline void executeEnvelope()
	{
		uint8_t period = NRx2 & 0b111;
		if (!period) return; 

		if (s.envelopePeriodTimer > 0)
		{
			s.envelopePeriodTimer--;

			if (s.envelopePeriodTimer == 0)
			{
				s.envelopePeriodTimer = period;		
				bool increaseVol = NRx2 & 0b1000;

				if (increaseVol)
				{
					if (s.amplitude < 0xF)
						s.amplitude++;
				}
				else
				{
					if (s.amplitude > 0)
						s.amplitude--;
				}
			}
		}
	}

	inline void executeLength()
	{
		if (!getBit(NRx4, 6) || s.lengthTimer == 0) return;

		s.lengthTimer--;

		if (s.lengthTimer == 0)
			s.triggered = false;
	}

	inline void execute()
	{
		if (s.freqPeriodTimer == 0)
		{
			s.freqPeriodTimer = 2048 - getFrequency();
			s.dutyStep = (s.dutyStep + 1) % 8;
		}

		s.freqPeriodTimer--;
	}

	inline float getSample()
	{
		uint8_t dutyType = NRx1 >> 6;
		return dutyTable[dutyType][s.dutyStep] * (s.amplitude / 15.0) * s.triggered;
	}
protected:
	static constexpr std::array<std::array<uint8_t, 8>, 4> dutyTable
	{ {
		{{ 0, 0, 0, 0, 0, 0, 0, 1 }},
		{{ 0, 0, 0, 0, 0, 0, 1, 1 }},
		{{ 0, 0, 0, 0, 1, 1, 1, 1 }},
		{{ 1, 1, 1, 1, 1, 1, 0, 0 }}
	} };

	struct state
	{
		uint8_t amplitude{};
		uint8_t lengthTimer{};
		uint8_t envelopePeriodTimer{};
		uint16_t freqPeriodTimer{};

		bool triggered{};
		uint8_t dutyStep{};
	};

	state s{};
};