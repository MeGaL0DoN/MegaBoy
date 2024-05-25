#pragma once
#include "cstdint"

class APU
{
public:
	friend class MMU;

	APU() { initMiniAudio(); };

	inline void reset() { regs = {}; }

	static constexpr uint32_t SAMPLE_RATE = 44100;
private:
	struct apuRegs
	{
		// Square 1
		uint8_t NR10 { 0x80 };
		uint8_t NR11 { 0xBF };
		uint8_t NR12 { 0xF3 };
		uint8_t NR13 { 0xFF };
		uint8_t NR14 { 0xBF };

		// Square 2
		uint8_t NR21 { 0x3F };
		uint8_t NR22 { 0x00 };
		uint8_t NR23 { 0xFF };
		uint8_t NR24 { 0xBF };

		// Wave
		uint8_t NR30 { 0x7F };
		uint8_t NR31 { 0xFF };
		uint8_t NR32 { 0x9F };
		uint8_t NR33 { 0xFF };
		uint8_t NR34 { 0xBF };

		// Noise
		uint8_t NR41 { 0xFF };
		uint8_t NR42 { 0x00 };
		uint8_t NR43 { 0x00 };
		uint8_t NR44 { 0xBF };

		// Control/Status
		uint8_t NR50 { 0x77 };
		uint8_t NR51 { 0xF3 };
		uint8_t NR52 { 0xF1 };
	};

	apuRegs regs{};

	void initMiniAudio();
};