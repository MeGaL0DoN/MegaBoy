#pragma once
#include <array>
#include <cstdint>
#include "../Utils/bitOps.h"
#include "RTC.h"

struct Huc3RTCState
{
	uint8_t command{};
	uint8_t commandArg{};
	uint8_t commandResponse{};
	uint8_t accessAddr{};
};

class HuC3RTC : public RTC
{
public:
	void writeCommand(uint8_t val)
	{
		s.command = (val & 0x70) >> 4;
		s.commandArg = val & 0x0F;
	}
	uint8_t readCommandResponse() const
	{
		return (s.command << 4) | s.commandResponse;
	}

	void writeSemaphore(uint8_t val)
	{
		// When least significant bit is 0, command is executed.
		if (getBit(val, 0))
			return;

		switch (s.command)
		{
		case 0x0:
			if (accessingTimeRegs())
				updateTime();

			s.commandResponse = s.accessAddr <= 0x15 ? regs[s.accessAddr] : 0x00;
			break;
		case 0x1:
			if (accessingTimeRegs())
				updateTime();

			s.commandResponse = s.accessAddr <= 0x15 ? regs[s.accessAddr] : 0x00;
			s.accessAddr++;
			break;
		case 0x2:
			if (s.accessAddr < 0x8) // Values 0x8-1F are read-only
				regs[s.accessAddr] = s.commandArg;
			break;
		case 0x3:
			if (s.accessAddr < 0x8) 
				regs[s.accessAddr] = s.commandArg;

			s.accessAddr++;
			break;
		case 0x4:
			s.accessAddr = (s.accessAddr & 0xF0) | s.commandArg;
			break;
		case 0x5:
			s.accessAddr = (s.accessAddr & 0x0F) | (s.commandArg << 4);
			break;
		case 0x6:
		{
			// Extended command in argument.
			switch (s.commandArg)
			{
			case 0x0:
				updateTime();

				for (int i = 0; i < 3; i++)
					regs[i] = regs[0x10 + i];

				for (int i = 0; i < 3; i++)
					regs[0x3 + i] = regs[0x13 + i];

				break;
			case 0x1:
				if (regs[0x6] != 1 || getBit(regs[0x7], 0))
					break;

				for (int i = 0; i < 3; i++)
					regs[0x10 + i] = regs[i];

				for (int i = 0; i < 3; i++)
					regs[0x13 + i] = regs[0x3 + i];

				regs[0x6] = 0;
				lastUnixTime = getUnixTime();
				secondsCounter = 0;
				break;
			case 0x2: // Status command, response must be 0x1 for game to start.
				s.commandResponse = 0x1;
				break;
			}
			break;
		}
		}
	}
	uint8_t readSemaphore() const
	{
		// When bit 0 is set the RTC is ready to receive command, so just always returning it.
		return 0x1;
	}

	void saveBattery(std::ostream& st)
	{
		updateTime();

		const auto minutes { getMinuteCounter() }, days { getDayCounter() };
		const uint64_t zero { 0 };

		ST_WRITE(lastUnixTime);
		ST_WRITE(minutes);
		ST_WRITE(days);

		// Sameboy expects 5 bytes of alarm data in the end; alarm is not emulated so just writing 5 bytes of zeroes.
		st.write(reinterpret_cast<const char*>(&zero), 5);
	}
	bool loadBattery(std::istream& st)
	{
		if (FileUtils::remainingBytes(st) < 12)
			return false;

		uint16_t minutes, days;

		ST_READ(lastUnixTime);
		ST_READ(minutes);
		ST_READ(days);

		writeMinuteCounter(minutes);
		writeDayCounter(days);
		secondsCounter = 0;

		updateTime();
		return true;
	}

	void saveState(std::ostream& st)
	{
		updateTime();

		ST_WRITE(lastUnixTime);
		ST_WRITE(s);
		ST_WRITE_ARR(regs);
	}
	void loadState(std::istream& st)
	{
		ST_READ(lastUnixTime);
		ST_READ(s);
		ST_READ_ARR(regs);
		secondsCounter = 0;

		updateTime();
	}

	void reset()
	{
		s = {};
		regs = {};
		lastUnixTime = getUnixTime();
		secondsCounter = 0;
	}

private:
	Huc3RTCState s{};
	// Storing only the io registers related to time ($00-15), not emulating alarm and tone generator stuff for now.
	std::array<uint8_t, 0x16> regs{};
	uint64_t lastUnixTime { getUnixTime() };
	uint8_t secondsCounter { 0 };

	inline bool accessingTimeRegs() const { return s.accessAddr >= 0x10 && s.accessAddr <= 0x15; }

	inline uint16_t getMinuteCounter() const { return (regs[0x12] << 8) | (regs[0x11] << 4) | regs[0x10]; }
	inline uint16_t getDayCounter() const { return (regs[0x15] << 8) | (regs[0x14] << 4) | regs[0x13]; }

	void writeMinuteCounter(uint16_t minutes)
	{
		regs[0x10] = minutes & 0xF;
		regs[0x11] = (minutes >> 4) & 0xF;
		regs[0x12] = (minutes >> 8) & 0xF;
	}
	void writeDayCounter(uint16_t days)
	{
		regs[0x13] = days & 0xF;
		regs[0x14] = (days >> 4) & 0xF;
		regs[0x15] = (days >> 8) & 0xF;
	}

	void updateTime()
	{
		const uint64_t time { getUnixTime() };
		const uint64_t diff { time - lastUnixTime };

		uint16_t minutes { getMinuteCounter() };
		uint16_t days { getDayCounter() };

		days += (diff / 86400);
		minutes += (diff / 60) % 1440;
		secondsCounter += (diff % 60);

		if (secondsCounter >= 60)
		{
			minutes++;
			secondsCounter %= 60;
		}
		if (minutes >= 1440)
		{
			days++;
			minutes %= 1440;
		}

		writeMinuteCounter(minutes);
		writeDayCounter(days & 0xFFF);
		lastUnixTime = time;
	}
};