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
		uint8_t amplitude{ 0 };
		uint16_t dutyCycles{ 0 };
		uint8_t dutyStep{ 0 };

		bool triggered{ false };
		uint8_t periodTimer{ 0 };
		uint8_t lengthTimer{ 0 };
	};

	state s{};

	inline void reset() { s = {}; }

	void trigger();
	void executeDuty();
	void executeLength();
	void executeEnvelope();

	inline void updateLengthTimer() { s.lengthTimer = 64 - (regs.NRx1 & 0x3F); }

	inline float getSample()
	{
		uint8_t dutyType = regs.NRx1 >> 6;
		return (dutyCycles[dutyType][s.dutyStep] * (s.amplitude / 15.0f));
	}
private:
	static constexpr uint8_t dutyCycles[4][8]
	{
		{0, 0, 0, 0, 0, 0, 0, 1}, // 00000001, 12.5%
		{1, 0, 0, 0, 0, 0, 0, 1}, // 10000001, 25%
		{1, 0, 0, 0, 0, 1, 1, 1}, // 10000111, 50%
		{0, 1, 1, 1, 1, 1, 1, 0}, // 01111110, 75%
	};
};