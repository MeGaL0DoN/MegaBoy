#pragma once

#include <cstdint>
#include <atomic>

struct noiseWaveRegs
{
	std::atomic<uint8_t> NR41, NR42, NR43, NR44;
};

struct noiseWaveState
{
	uint16_t freqPeriodTimer { }, LFSR { };
	uint8_t amplitude { }, envelopePeriodTimer { }, lengthTimer { };
	bool enabled { };
};

struct noiseWave
{
	inline void reset()
	{
		s = {};

		regs.NR41 = 0xFF;
		regs.NR42 = 0x00;
		regs.NR43 = 0x00;
		regs.NR44 = 0xBF;
	}

	inline bool dacEnabled() const { return (regs.NR42 & 0xF8) != 0; }

	inline uint8_t getDivisor() const
	{
		const uint8_t divisorCode = regs.NR43 & 0x7;
		return divisorCode == 0 ? 8 : divisorCode << 4;
	}

	inline uint16_t getPeriodTimer() const
	{
		const uint8_t shiftAmount = (regs.NR43 & 0xF0) >> 4;
		return getDivisor() << shiftAmount;
	}

	inline void disable() { s.enabled = false; }

	inline void trigger()
	{
		s.envelopePeriodTimer = regs.NR42 & 0b111;
		s.amplitude = (regs.NR42 >> 4) & 0b1111;
		s.lengthTimer = s.lengthTimer == 0 ? 64 : s.lengthTimer;
		s.LFSR = 0x7FFF;
		s.enabled = dacEnabled();
	}

	inline void executeEnvelope()
	{
		const uint8_t period = regs.NR42 & 0b111;
		if (period == 0) return;

		if (s.envelopePeriodTimer > 0)
		{
			s.envelopePeriodTimer--;

			if (s.envelopePeriodTimer == 0)
			{
				s.envelopePeriodTimer = period;
				const bool increaseVol = regs.NR42 & 0b1000;

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

	inline void reloadLength() { s.lengthTimer = 64 - (regs.NR41 & 0b00111111); }

	inline void executeLength()
	{
		if (!getBit(regs.NR44.load(), 6) || s.lengthTimer == 0)
			return;

		s.lengthTimer--;

		if (s.lengthTimer == 0)
			s.enabled = false;
	}

	inline void execute()
	{
		if (s.freqPeriodTimer == 0)
		{
			s.freqPeriodTimer = getPeriodTimer();
			const uint8_t xorResult = (s.LFSR & 0x1) ^ ((s.LFSR & 0x2) >> 1);
			s.LFSR = (s.LFSR >> 1) | (xorResult << 14);

			const bool smallWidthMode = getBit(regs.NR43.load(), 3);

			if (smallWidthMode)
				s.LFSR = setBit(s.LFSR, 6, static_cast<bool>(xorResult));
		}

		s.freqPeriodTimer--;
	}

	inline uint8_t getSample() const
	{
		const uint8_t baseAmplitude = ~s.LFSR & 0x01;
		return baseAmplitude * s.amplitude * s.enabled;
	}

	noiseWaveState s{};
	noiseWaveRegs regs{};
};