#pragma once
#include <cstdint>
#include "../gbSystem.h"

struct Register8
{
	uint8_t val;

	Register8() = default;
	explicit Register8(uint8_t v) : val(v)
	{};

	void operator =(uint8_t _val)
	{
		this->val = _val;
	}
};

union Register16 
{
	uint16_t val;
	struct RegisterPair {
		Register8 low;
		Register8 high;
	} pair;

	Register16() = default;
	explicit Register16(uint16_t v) : val(v)
	{};

	void operator =(uint16_t _val)
	{
		this->val = _val;
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
		case GBSystem::GBC:
			{
				AF = 0x1180;
				BC = 0x0000;
				DE = 0xFF56;
				HL = 0x000D;
				break;
			}
		}
	}
};