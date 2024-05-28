#include "RTCTimer.h"
#include "bitOps.h"
#include <fstream>

void RTCTimer::incrementDay()
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
}

void RTCTimer::addRTCcycles(int32_t cycles)
{
	if (getBit(s.regs.DH, 6)) // Timer disabled
	{
		wasHalted = true;
		return;
	}

	s.cycles += cycles;

	if (s.cycles >= CYCLES_PER_SECOND)
	{
		uint64_t currentTime = unix_time();

		if ((currentTime - lastUnixTime) > 1 && !wasHalted)
			adjustRTC(); // Auto-adjusting RTC. (for example: emulation paused unexpectedly).
		else
		{
			s.regs.S++;

			if (s.regs.S == 60)
			{
				s.regs.S = 0;
				s.regs.M++;

				if (s.regs.M == 60)
				{
					s.regs.M = 0;
					s.regs.H++;

					if (s.regs.H == 24)
					{
						s.regs.H = 0;
						incrementDay();
					}
				}
			}

			lastUnixTime = currentTime;
		}

		s.cycles = 0;
		wasHalted = false;
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

	for (uint16_t i = 0; i < daysToAdd; i++)
		incrementDay();

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
		s.regs.DH = val & 0xC1; break;
	}
}

void RTCTimer::saveBattery(std::ofstream& st) const
{
	auto WRITE_AS_INT = [&st](uint32_t var)
	{
		st.write(reinterpret_cast<char*>(&var), sizeof(var));
	};

	WRITE_AS_INT(s.regs.S);
	WRITE_AS_INT(s.regs.M);
	WRITE_AS_INT(s.regs.H);
	WRITE_AS_INT(s.regs.DL);
	WRITE_AS_INT(s.regs.DH);

	WRITE_AS_INT(s.latchedRegs.S);
	WRITE_AS_INT(s.latchedRegs.M);
	WRITE_AS_INT(s.latchedRegs.H);
	WRITE_AS_INT(s.latchedRegs.DL);
	WRITE_AS_INT(s.latchedRegs.DH);

	st.write(reinterpret_cast<const char*>(&lastUnixTime), sizeof(lastUnixTime));
}

void RTCTimer::loadBattery(std::ifstream& st)
{
	auto READ_AS_INT = [&st](uint8_t& var)
	{
		uint32_t var32;
		st.read(reinterpret_cast<char*>(&var32), sizeof(var32));
		var = static_cast<uint8_t>(var32);
	};

	READ_AS_INT(s.regs.S);
	READ_AS_INT(s.regs.M);
	READ_AS_INT(s.regs.H);
	READ_AS_INT(s.regs.DL);
	READ_AS_INT(s.regs.DH);

	READ_AS_INT(s.regs.S);
	READ_AS_INT(s.regs.M);
	READ_AS_INT(s.regs.H);
	READ_AS_INT(s.regs.DL);
	READ_AS_INT(s.regs.DH);

	lastUnixTime = 0;

	uint32_t pos = st.tellg();
	st.seekg(0, std::ios::end);

	uint8_t leftBytes = static_cast<uint32_t>(st.tellg()) - pos;
	st.seekg(pos, std::ios::beg);
	st.read(reinterpret_cast<char*>(&lastUnixTime), leftBytes <= sizeof(lastUnixTime) ? leftBytes : sizeof(lastUnixTime));

	adjustRTC();
}