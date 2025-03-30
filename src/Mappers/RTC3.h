#pragma once

#include <cstdint>
#include <iostream>
#include <chrono>
#include "RTC.h"
#include "../defines.h"

struct RTC3Regs
{
	uint8_t S{};
	uint8_t M{};
	uint8_t H{};
	uint8_t DL{};
	uint8_t DH{};

	inline uint8_t getReg(uint8_t val) const
	{
		switch (val)
		{
			case 0x08: return S & 0x3F;
			case 0x09: return M & 0x3F;
			case 0x0A: return H & 0x1F;
			case 0x0B: return DL;
			case 0x0C: return DH & 0xC1;
			default: UNREACHABLE();
		}
	}
};

struct RTC3State
{
	RTC3Regs regs{};
	RTC3Regs latchedRegs{};

	uint8_t reg{ 0 };
	uint8_t latchWrite{ 0xFF };
	bool latched{ false };
	int32_t cycles{ 0 };
};

class RTC3 : public RTC
{
public:
	RTC3State s{};

	static constexpr uint32_t CYCLES_PER_SECOND = 1048576 * 4;
	void addCycles(uint64_t cycles);

	constexpr void setReg(uint8_t reg) { s.reg = reg; }
	void writeReg(uint8_t val);

	bool loadBattery(std::istream& st);
	void saveBattery(std::ostream& st) const;

	inline void reset() 
	{
		s = {};
		lastUnixTime = getUnixTime(); 
	}

	inline void enableFastForward(int speedFactor) override
	{
		s.cycles *= speedFactor;
		slowDownFactor = speedFactor;
	}
	inline void disableFastForward() override 
	{
		s.cycles /= slowDownFactor;
		slowDownFactor = 1;
	}

private:
	uint64_t lastUnixTime{};
	int slowDownFactor { 1 };

	inline uint64_t getUnixTime() const
	{
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	void adjustRTC();
	void addDays(uint16_t days);
};