#include "RTC3.h"
#include "../Utils/bitOps.h"
#include "../Utils/fileUtils.h"

void RTC3::addDays(uint16_t days)
{
	while (days--)
	{
		if (++s.regs.DL == 0)
		{
			const bool overflow = getBit(s.regs.DH, 0);

			if (overflow)
			{
				s.regs.DH = resetBit(s.regs.DH, 0);
				s.regs.DH = setBit(s.regs.DH, 7);
			}
			else
				s.regs.DH = setBit(s.regs.DH, 0);
		}
	}
}

void RTC3::addCycles(uint64_t cycles)
{
	if (getBit(s.regs.DH, 6)) // Halt
		return;

	s.cycles += cycles;
	const uint32_t targetCycles { CYCLES_PER_SECOND * slowDownFactor };

	if (s.cycles >= targetCycles)
	{
		const uint64_t currentTime { getUnixTime() };
		const int addedSeconds = s.cycles / targetCycles;

		if ((currentTime - lastUnixTime) > addedSeconds)
		{
			adjustRTC(); // Auto-adjusting RTC. (for example: emulation paused unexpectedly).
			s.cycles = 0;
		}
		else
		{
			s.cycles -= (addedSeconds * targetCycles);

			const bool secondsLegal { s.regs.S < 60 };
			const bool minutesLegal { s.regs.M < 60 };
			const bool hoursLegal { s.regs.H < 24 };

			s.regs.S += addedSeconds;

			if (secondsLegal && s.regs.S >= 60) 
			{
				const int extraMinutes { s.regs.S / 60 };
				s.regs.S %= 60;
				s.regs.M += extraMinutes;

				if (minutesLegal && s.regs.M >= 60)
				{
					const int extraHours { s.regs.M / 60 };
					s.regs.M %= 60;
					s.regs.H += extraHours;

					if (hoursLegal && s.regs.H >= 24)
					{
						const int extraDays { s.regs.H / 24 };
						s.regs.H %= 24;
						addDays(extraDays);
					}
				}
			}

			lastUnixTime = currentTime;
		}
	}
}

void RTC3::adjustRTC()
{
	const uint64_t time { getUnixTime() };
	const uint64_t diff { time - lastUnixTime };

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

void RTC3::writeReg(uint8_t val)
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
			lastUnixTime = getUnixTime();

		s.regs.DH = val & 0xC1; break;
	}
}

void RTC3::saveBattery(std::ostream& st) const
{
	const auto writeAs32 = [&st](uint32_t var)
	{
		st.write(reinterpret_cast<char*>(&var), sizeof(var));
	};

	writeAs32(s.regs.S);
	writeAs32(s.regs.M);
	writeAs32(s.regs.H);
	writeAs32(s.regs.DL);
	writeAs32(s.regs.DH);

	writeAs32(s.latchedRegs.S);
	writeAs32(s.latchedRegs.M);
	writeAs32(s.latchedRegs.H);
	writeAs32(s.latchedRegs.DL);
	writeAs32(s.latchedRegs.DH);

	st.write(reinterpret_cast<const char*>(&lastUnixTime), sizeof(lastUnixTime));
}

bool RTC3::loadBattery(std::istream& st)
{
	constexpr int MIN_RTC_SAVE_SIZE = 40;

	uint32_t remainingBytes { FileUtils::remainingBytes(st) };

	if (remainingBytes < MIN_RTC_SAVE_SIZE)
		return false;

	const auto readAs32 = [&st](uint8_t& var)
	{
		uint32_t var32;
		st.read(reinterpret_cast<char*>(&var32), sizeof(var32));
		var = static_cast<uint8_t>(var32);
	};

	readAs32(s.regs.S);
	readAs32(s.regs.M);
	readAs32(s.regs.H);
	readAs32(s.regs.DL);
	readAs32(s.regs.DH);

	readAs32(s.latchedRegs.S);
	readAs32(s.latchedRegs.M);
	readAs32(s.latchedRegs.H);
	readAs32(s.latchedRegs.DL);
	readAs32(s.latchedRegs.DH);

	remainingBytes -= MIN_RTC_SAVE_SIZE;

	if (remainingBytes >= 4)
	{
		lastUnixTime = 0;
		// Support both, 4 and 8 byte timestamps.
		st.read(reinterpret_cast<char*>(&lastUnixTime), remainingBytes <= sizeof(lastUnixTime) ? remainingBytes : sizeof(lastUnixTime)); 
		adjustRTC();
	}
	else
		lastUnixTime = getUnixTime();

	return true;
}