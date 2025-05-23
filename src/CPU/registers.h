#pragma once
#include <cstdint>
#include "../gbSystem.h"

struct Register8
{
	uint8_t val;

	void operator =(uint8_t val)
	{
		this->val = val;
	}
};

union Register16 
{
	uint16_t val;

	struct 
	{
		Register8 low;
		Register8 high;
	};

	void operator =(uint16_t val)
	{
		this->val = val;
	}
};

enum FlagType : uint8_t
{
	Zero = 7,
	Subtract = 6,
	HalfCarry = 5,
	Carry = 4
};

struct registerCollection
{
	Register16 AF{};
	Register16 BC{};
	Register16 DE{};
	Register16 HL{};

	registerCollection() { reset(); }
private:
	void reset()
	{
		switch (System::Current())
		{
		case GBSystem::DMG:
			{
				AF = 0x01B0;
				BC = 0x0013;
				DE = 0x00D8;
				HL = 0x014D;
				break;
			}
		case GBSystem::CGB:
			{
				AF = 0x1180;
				BC = 0x0000;
				DE = 0xFF56;
				HL = 0x000D;
				break;
			}
		case GBSystem::DMGCompatMode:
			{
				AF = 0x1180;
				BC = 0x0000;
				DE = 0x0008;
				HL = (BC.high.val == 0x43 || BC.high.val == 0x58) ? 0x991A : 0x007C;
				break;
			}
		}
	}
};