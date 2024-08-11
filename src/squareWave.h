#pragma once
#include <cstdint>

class squareWave
{
public:
	squareWave()
	{}

	struct regs
	{
		uint8_t NRx1 {};
		uint8_t NRx2 {};
		uint8_t NRx3 {};
		uint8_t NRx4 {};
	};

	regs regs{};

	struct state
	{
		uint8_t amplitude { };

		uint16_t frequencyTimer { };
		//uint16_t dutyCycles { };
		uint8_t dutyStep { };

		bool triggered{ false };
		uint8_t periodTimer { };
		uint8_t lengthTimer { };
	};

	state s{};

	inline void reset() { s = {}; }

	void trigger();
	void executeDuty();
	void executeLength();
	void executeEnvelope();

	inline void updateLengthTimer() { s.lengthTimer = 64 - (regs.NRx1 & 0x3F); }
//	inline void updatePeriod() { s.period = regs.NRx3 | ((regs.NRx4 & 0x07) << 8); }

	inline float getSample()
	{
		uint8_t dutyType = regs.NRx1 >> 6;
		return dutyCycles[dutyType][s.dutyStep] * (s.amplitude / 15.0) * s.triggered;
	}

	inline uint16_t getPeriod() { return regs.NRx3 | ((regs.NRx4 & 0x07) << 8); }

private:
	static constexpr uint8_t dutyCycles[4][8]
	{
		{0, 0, 0, 0, 0, 0, 0, 1}, // 00000001, 12.5%
		{1, 0, 0, 0, 0, 0, 0, 1}, // 10000001, 25%
		{1, 0, 0, 0, 0, 1, 1, 1}, // 10000111, 50%
		{0, 1, 1, 1, 1, 1, 1, 0}, // 01111110, 75%
	};
};