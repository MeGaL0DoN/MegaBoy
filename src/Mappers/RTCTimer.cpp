#include "RTCTimer.h"
#include "../Utils/bitOps.h"
#include "../Utils/fileUtils.h"

void RTCTimer::addDays(uint16_t days)
{
	while (days > 0)
	{
		s.regs.DL++;

		if (s.regs.DL == 0)
		{
			bool overflow = getBit(s.regs.DH, 0);

			if (overflow)
			{
				s.regs.DH = resetBit(s.regs.DH, 0);
				s.regs.DH = setBit(s.regs.DH, 7);
			}
			else
				s.regs.DH = setBit(s.regs.DH, 0);
		}

		days--;
	}
}

void RTCTimer::addRTCcycles(uint64_t cycles)
{
	if (getBit(s.regs.DH, 6)) // Halt
		return;

	s.cycles += cycles;
	const uint32_t TARGET_CYCLES = CYCLES_PER_SECOND * slowDownFactor;

	if (s.cycles >= TARGET_CYCLES)
	{
		uint64_t currentTime = unix_time();
		const int addedSeconds = s.cycles / TARGET_CYCLES;

		if ((currentTime - lastUnixTime) > addedSeconds)
		{
			adjustRTC(); // Auto-adjusting RTC. (for example: emulation paused unexpectedly).
			s.cycles = 0;
		}
		else
		{
			s.cycles -= (addedSeconds * TARGET_CYCLES);

			const bool secondsLegal = s.regs.S < 60;
			const bool minutesLegal = s.regs.M < 60;
			const bool hoursLegal = s.regs.H < 24;

			s.regs.S += addedSeconds;

			if (secondsLegal && s.regs.S >= 60) 
			{
				uint8_t extraMinutes = s.regs.S / 60;
				s.regs.S %= 60;
				s.regs.M += extraMinutes;

				if (minutesLegal && s.regs.M >= 60)
				{
					uint8_t extraHours = s.regs.M / 60;
					s.regs.M %= 60;
					s.regs.H += extraHours;

					if (hoursLegal && s.regs.H >= 24)
					{
						uint8_t extraDays = s.regs.H / 24;
						s.regs.H %= 24;
						addDays(extraDays);
					}
				}
			}

			lastUnixTime = currentTime;
		}
	}
}

void RTCTimer::adjustRTC()
{
	uint64_t time = unix_time();
	uint64_t diff = time - lastUnixTime;

	s.regs.S += diff % 60;
	s.regs.M += (diff / 60) % 60;
	s.regs.H += (diff / 3600) % 24;
	uint16_t daysToAdd = diff / 86400;

	if (s.regs.S >= 60)
	{
		s.regs.M += s.regs.S / 60;
		s.regs.S %= 60;
	}
	if (s.regs.M >= 60)
	{
		s.regs.H += s.regs.M / 60;
		s.regs.M %= 60;
	}
	if (s.regs.H >= 24)
	{
		daysToAdd += s.regs.H / 24;
		s.regs.H %= 24;
	}

	addDays(daysToAdd);

	lastUnixTime = time;
}

void RTCTimer::writeReg(uint8_t val)
{
	switch (s.reg)
	{
	case 0x08:
		s.regs.S = val & 0x3F;
		s.cycles = 0;
		break;
	case 0x09:
		s.regs.M = val & 0x3F; break;
	case 0x0A:
		s.regs.H = val & 0x1F; break;
	case 0x0B:
		s.regs.DL = val; break;
	case 0x0C:
		const bool wasHalted = getBit(s.regs.DH, 6);
		const bool isHalted = getBit(val, 6);

		if (wasHalted && !isHalted) // RTC is being enabled.
			lastUnixTime = unix_time();

		s.regs.DH = val & 0xC1; break;
	}
}

void RTCTimer::saveBattery(std::ostream& st) const
{
	auto WRITE_AS_32 = [&st](uint32_t var)
	{
		st.write(reinterpret_cast<char*>(&var), sizeof(var));
	};

	WRITE_AS_32(s.regs.S);
	WRITE_AS_32(s.regs.M);
	WRITE_AS_32(s.regs.H);
	WRITE_AS_32(s.regs.DL);
	WRITE_AS_32(s.regs.DH);

	WRITE_AS_32(s.latchedRegs.S);
	WRITE_AS_32(s.latchedRegs.M);
	WRITE_AS_32(s.latchedRegs.H);
	WRITE_AS_32(s.latchedRegs.DL);
	WRITE_AS_32(s.latchedRegs.DH);

	st.write(reinterpret_cast<const char*>(&lastUnixTime), sizeof(lastUnixTime));
}

bool RTCTimer::loadBattery(std::istream& st)
{
	constexpr int MIN_RTC_SAVE_SIZE = 40;

	uint32_t availableBytes = FileUtils::getAvailableBytes(st);

	if (availableBytes < MIN_RTC_SAVE_SIZE)
		return false;

	auto READ_AS_32 = [&st](uint8_t& var)
	{
		uint32_t var32;
		st.read(reinterpret_cast<char*>(&var32), sizeof(var32));
		var = static_cast<uint8_t>(var32);
	};

	READ_AS_32(s.regs.S);
	READ_AS_32(s.regs.M);
	READ_AS_32(s.regs.H);
	READ_AS_32(s.regs.DL);
	READ_AS_32(s.regs.DH);

	READ_AS_32(s.latchedRegs.S);
	READ_AS_32(s.latchedRegs.M);
	READ_AS_32(s.latchedRegs.H);
	READ_AS_32(s.latchedRegs.DL);
	READ_AS_32(s.latchedRegs.DH);

	availableBytes -= MIN_RTC_SAVE_SIZE;

	if (availableBytes >= 4)
	{
		lastUnixTime = 0;
		st.read(reinterpret_cast<char*>(&lastUnixTime), availableBytes <= sizeof(lastUnixTime) ? availableBytes : sizeof(lastUnixTime)); // Support both, 4 and 8 byte timestamps.
		adjustRTC();
	}
	else
		lastUnixTime = unix_time();

	return true;
}