#pragma once
#include <cstdint>
#include <array>
#include <atomic>

#include "../Utils/bitOps.h"

struct squareWaveRegs
{
	std::atomic<uint8_t> NRx1, NRx2, NRx3, NRx4;
};

struct squareWaveState
{
	uint16_t freqPeriodTimer {};
	uint8_t dutyStep {}, amplitude {}, lengthTimer {}, envelopePeriodTimer {};
	bool enabled {};
};

template<typename state = squareWaveState, typename r = squareWaveRegs>
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

	inline bool dacEnabled() { return (regs.NRx2 & 0xF8) != 0; }

	inline uint16_t getFrequency()
	{
		return regs.NRx3 | ((regs.NRx4 & 0b111) << 8);
	}

	inline void disable() { s.enabled = false; }

	inline void trigger()
	{
		s.envelopePeriodTimer = regs.NRx2 & 0b111;
		s.dutyStep = 0;
		s.amplitude = (regs.NRx2 >> 4) & 0b1111;
		s.lengthTimer = s.lengthTimer == 0 ? 64 : s.lengthTimer;
		s.enabled = dacEnabled();
	}

	inline void executeEnvelope()
	{
		const uint8_t period = regs.NRx2 & 0b111;
		if (period == 0) return; 

		if (s.envelopePeriodTimer > 0)
		{
			s.envelopePeriodTimer--;

			if (s.envelopePeriodTimer == 0)
			{
				s.envelopePeriodTimer = period;		
				const bool increaseVol = regs.NRx2 & 0b1000;

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

	inline void reloadLength() { s.lengthTimer = 64 - (regs.NRx1 & 0b00111111); }

	inline void executeLength()
	{
		if (!getBit(regs.NRx4.load(), 6) || s.lengthTimer == 0) 
			return;

		s.lengthTimer--;

		if (s.lengthTimer == 0)
			s.enabled = false;
	}

	inline void execute()
	{
		if (s.freqPeriodTimer == 0)
		{
			s.freqPeriodTimer = 2048 - getFrequency();
			s.dutyStep = (s.dutyStep + 1) & 7;
		}

		s.freqPeriodTimer--;
	}

	inline uint8_t getSample()
	{
		const uint8_t dutyType = regs.NRx1 >> 6;
		return DUTY_TABLE[dutyType][s.dutyStep] * s.amplitude * s.enabled;
	}

	static constexpr auto DUTY_TABLE = std::to_array<std::array<uint8_t, 8>>
	({
		{ 0, 0, 0, 0, 0, 0, 0, 1 },
		{ 0, 0, 0, 0, 0, 0, 1, 1 },
		{ 0, 0, 0, 0, 1, 1, 1, 1 },
		{ 1, 1, 1, 1, 1, 1, 0, 0 }
	});

	state s{};
	r regs{};
};