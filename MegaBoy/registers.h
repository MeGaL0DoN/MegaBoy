#pragma once
#include <cstdint>
#include "bitOps.h"

struct Register8
{
	uint8_t val;

	Register8() = default;
	explicit Register8(uint8_t v) : val(v)
	{};

	void operator =(uint8_t val)
	{
		this->val = val;
	}
};

union Register16 {
	uint16_t val;
	struct {
		Register8 low;
		Register8 high;
	};

	Register16() = default;
	explicit Register16(uint16_t v) : val(v)
	{};

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

	Register8& A;
	Register8& B;
	Register8& C;
	Register8& D;
	Register8& E;
	Register8& H;
	Register8& L;
	Register8& F;

	registerCollection() : A(AF.high), B(BC.high), C(BC.low), D(DE.high),
						   E(DE.low), H(HL.high), L(HL.low), F(AF.low)
	{
		resetRegisters();
	}

	void resetRegisters()
	{
		A = 0x01;
		B = 0x00;
		C = 0x13;
		D = 0x00;
		E = 0xD8;
		F = 0xB0;
		H = 0x01;
		L = 0x4D;
	}

	inline bool getFlag(FlagType flag)
	{
		return getBit(F.val, flag);
	}
	inline void setFlag(FlagType flag, bool value)
	{
		F = setBit(F.val, flag, value);
	}
	inline void resetFlags()
	{
		F.val &= 0x0F;
	}
};