#pragma once
#include <cassert>
#include <cstdint>
#include <chrono>

struct RTCRegs
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
		}

		assert(false);
		return 0x00;
	}
};

struct RTCState
{
	mutable RTCRegs regs{};
	RTCRegs latchedRegs{};

	uint8_t reg{ 0 };
	uint8_t latchWrite{ 0xFF };
	bool latched{ false };
	int32_t cycles{ 0 };
};

class RTCTimer
{
public:
	RTCState s{};

	static constexpr uint32_t CYCLES_PER_SECOND = 1048576 * 4;
	void addRTCcycles(uint32_t cycles);
	void adjustRTC();

	constexpr void setReg(uint8_t reg) { s.reg = reg; }
	void writeReg(uint8_t val);

	void loadBattery(std::ifstream& st);
	void saveBattery(std::ofstream& st) const;

	inline void reset() { s = {}; lastUnixTime = unix_time(); }
private:
	void incrementDay();

	uint64_t lastUnixTime{};
	bool wasHalted { false };

	inline uint64_t unix_time() const
	{
		return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
};