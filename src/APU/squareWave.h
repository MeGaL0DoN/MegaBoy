#pragma once
#include <cstdint>
#include <array>
#include <atomic>

#include "../Utils/bitOps.h"

struct squareWaveRegs
{
	std::atomic<uint8_t> NRx1;
	std::atomic<uint8_t> NRx2;
	std::atomic<uint8_t> NRx3;
	std::atomic<uint8_t> NRx4;
};

struct squareWaveState
{
	uint8_t dutyStep {};
	uint8_t amplitude {};
	uint8_t lengthTimer {};
	uint8_t envelopePeriodTimer {};
	uint16_t freqPeriodTimer {};
	bool triggered {};
};

template<typename state = squareWaveState, typename regs = squareWaveRegs>
struct squareWave
{
	inline void reset() 
	{ 
		s = {}; 

		regs.NRx1 = 0x3F; 
		regs.NRx2 = 0x00;
		regs.NRx3 = 0xFF;
		regs.NRx4 = 0xBF;
	}

	inline uint16_t getFrequency()
	{
		return regs.NRx3 | ((regs.NRx4 & 0b111) << 8);
	}

	inline void trigger()
	{
		s.lengthTimer = 64 - (regs.NRx1 & 0b00111111);
		s.envelopePeriodTimer = regs.NRx2 & 0b111;
		s.amplitude = (regs.NRx2 >> 4) & 0b1111;
		s.triggered = true;

		s.freqPeriodTimer = 2048 - getFrequency();
		s.dutyStep = 0;
	}

	inline void executeEnvelope()
	{
		uint8_t period = regs.NRx2 & 0b111;
		if (period == 0) return; 

		if (s.envelopePeriodTimer > 0)
		{
			s.envelopePeriodTimer--;

			if (s.envelopePeriodTimer == 0)
			{
				s.envelopePeriodTimer = period;		
				bool increaseVol = regs.NRx2 & 0b1000;

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
		if (!getBit(regs.NRx4.load(), 6) || s.lengthTimer == 0) return;

		s.lengthTimer--;

		if (s.lengthTimer == 0)
			s.triggered = false;
	}

	inline void execute()
	{
		s.freqPeriodTimer--;

		if (s.freqPeriodTimer == 0)
		{
			s.freqPeriodTimer = 2048 - getFrequency();
			s.dutyStep = (s.dutyStep + 1) % 8;
		}
	}

	inline float getSample()
	{
		uint8_t dutyType = regs.NRx1 >> 6;
		return DUTY_TABLE[dutyType][s.dutyStep] * (s.amplitude / 15.f) * s.triggered;
	}

	static constexpr auto DUTY_TABLE = std::to_array<std::array<uint8_t, 8>>
	({
		{ 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 0, 0, 0, 0, 0, 0, 1, 1 },
		{ 0, 0, 0, 0, 1, 1, 1, 1 },
		{ 1, 1, 1, 1, 1, 1, 0, 0 }
	});

	state s{};
	regs regs{};
};