#pragma once

#include <array>
#include <cstdint>
#include <atomic>

#include "../defines.h"

struct customWaveRegs
{
	std::atomic<uint8_t> NR30, NR31, NR32, NR33, NR34;
};

struct customWaveState
{
	uint8_t sampleInd{};
	uint16_t periodTimer{}, lengthTimer{};
	bool enabled{};
};

struct customWave
{
	inline void reset() 
	{
		s = {}; 
		waveRAM = {};

		regs.NR30 = 0x7F;
		regs.NR31 = 0xFF;
		regs.NR32 = 0x9F;
		regs.NR33 = 0xFF;
		regs.NR34 = 0xBF;
	}

	inline uint16_t getFrequency()
	{
		return regs.NR33 | ((regs.NR34 & 0b111) << 8);
	}

	inline uint8_t getVolumeShift()
	{
		switch (regs.NR32 & 0b01100000)
		{
		case 0b00 << 5: return 4;
		case 0b01 << 5: return 0;
		case 0b10 << 5: return 1;
		case 0b11 << 5: return 2;
		default: UNREACHABLE();
		}
	}

	inline void trigger()
	{
		if (!getBit(regs.NR30.load(), 7))
			return; // DAC is disabled.

		s.periodTimer = (2048 - getFrequency()) >> 1;
		s.lengthTimer = 256 - regs.NR31;
		s.enabled = true;
	}

	inline void disable() { s.enabled = false; }

	inline void executeLength()
	{
		if (!getBit(regs.NR34.load(), 6) || s.lengthTimer == 0) return;

		s.lengthTimer--;

		if (s.lengthTimer == 0)
			s.enabled = false;
	}

	inline void execute()
	{
		s.periodTimer--;

		if (s.periodTimer == 0)
		{
			s.periodTimer = (2048 - getFrequency()) >> 1;
			s.sampleInd = (s.sampleInd + 1) & 31;
		}
	}

	inline float getSample()
	{
		uint8_t sample = (s.sampleInd & 1) == 0 ? (waveRAM[s.sampleInd >> 1] >> 4) : (waveRAM[s.sampleInd >> 1] & 0xF);
		return ((sample >> getVolumeShift()) / 15.f) * s.enabled;
	}

	customWaveState s{};
	customWaveRegs regs{};
	std::array<uint8_t, 16> waveRAM{};
};