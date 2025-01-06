#pragma once

#include <cstdint>
#include <atomic>

struct noiseWaveRegs
{
	std::atomic<uint8_t> NR41, NR42, NR43, NR44;
};

struct noiseWaveState
{
	bool enabled { true };
};

struct noiseWave
{
	inline void reset()
	{
		s = {};
	}

	inline bool dacEnabled() { return (regs.NR42 & 0xF8) != 0; }

	inline void trigger()
	{
		s.enabled = true;
	}

	inline void disable() { s.enabled = false; }

	inline void reloadLength() {}

	inline void execute()
	{

	}

	inline float getSample()
	{
		return 0.f;
	}

	noiseWaveRegs regs{};
	noiseWaveState s{};
};