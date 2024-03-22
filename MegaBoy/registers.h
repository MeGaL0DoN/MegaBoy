#pragma once
#include <cstdint>

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

enum class FlagType : uint8_t
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
		AF = 0;
		BC = 0;
		DE = 0;
		HL = 0;
	}

	constexpr void setFlag(FlagType flag, bool value)
	{
		if (value)
			F.val |= (1 << static_cast<uint8_t>(flag));
		else
			F.val &= ~(1 << static_cast<uint8_t>(flag));
	}
	constexpr void resetFlags()
	{
		F.val &= 0xF0;
	}
	constexpr bool getFlag(FlagType flag)
	{
		return F.val & (1 << static_cast<uint8_t>(flag));
	}
};