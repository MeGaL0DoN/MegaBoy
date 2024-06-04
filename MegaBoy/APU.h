#pragma once
#include <cstdint>
#include <array>
#include <atomic>
#include <memory>

class APU
{
public:
	friend class MMU;

	APU();
	~APU();

	void execute();
	inline void reset() { regs = {}; waveRAM = {}; }

	static constexpr uint32_t CPU_FREQUENCY = 1053360;
	static constexpr uint32_t SAMPLE_RATE = 44100;
	static constexpr uint32_t CYCLES_PER_SAMPLE = CPU_FREQUENCY / SAMPLE_RATE;

	int16_t sample;

	//static constexpr uint32_t BUFFER_SIZE = SAMPLE_RATE;
	//std::array<int16_t, BUFFER_SIZE> sampleBuffer {};

	//std::atomic<size_t> writeIndex{ 0 };
	//std::atomic<size_t> readIndex{ 0 };
private:
	void initMiniAudio();

	typedef class ma_device ma_device;
	std::unique_ptr<ma_device> soundDevice;

	struct apuRegs
	{
		// Square 1
		uint8_t NR10{ 0x80 };
		uint8_t NR11{ 0xBF };
		uint8_t NR12{ 0xF3 };
		uint8_t NR13{ 0xFF };
		uint8_t NR14{ 0xBF };

		// Square 2
		uint8_t NR21{ 0x3F };
		uint8_t NR22{ 0x00 };
		uint8_t NR23{ 0xFF };
		uint8_t NR24{ 0xBF };

		// Wave
		uint8_t NR30{ 0x7F };
		uint8_t NR31{ 0xFF };
		uint8_t NR32{ 0x9F };
		uint8_t NR33{ 0xFF };
		uint8_t NR34{ 0xBF };

		// Noise
		uint8_t NR41{ 0xFF };
		uint8_t NR42{ 0x00 };
		uint8_t NR43{ 0x00 };
		uint8_t NR44{ 0xBF };

		// Control/Status
		uint8_t NR50{ 0x77 };
		uint8_t NR51{ 0xF3 };
		uint8_t NR52{ 0xF1 };
	};

	apuRegs regs{};
	std::array<uint8_t, 16> waveRAM{};

	uint32_t cycles{ 0 };
	uint32_t channel2Cycles { 0 };
	uint8_t dutyStep2{ 0 };

	static constexpr uint8_t dutyCycles[4][8]
	{
		{0, 0, 0, 0, 0, 0, 0, 1}, // 00000001, 12.5%
		{1, 0, 0, 0, 0, 0, 0, 1}, // 10000001, 25%
		{1, 0, 0, 0, 0, 1, 1, 1}, // 10000111, 50%
		{0, 1, 1, 1, 1, 1, 1, 0}, // 01111110, 75%
	};
};