#pragma once
#include <cstdint>

enum class FlagType
{
	Zero = 7,
	Subtract = 6,
	HalfCarry = 5,
	Carry = 4
};

struct reg16
{
	uint8_t& reg1;
	uint8_t& reg2;
};
struct reg16View
{
	uint8_t reg1;
	uint8_t reg2;
};

struct registerCollection
{
	uint8_t A;
	uint8_t B;
	uint8_t C;
	uint8_t D;
	uint8_t E;
	uint8_t H;
	uint8_t L;
	uint8_t F;

	constexpr uint16_t get16Bit(reg16View reg)
	{
		return (reg.reg2 << 8) | reg.reg1;
	}
	constexpr void set16Bit(reg16 reg, uint16_t val)
	{
		reg.reg1 = val & 0xFF; 
		reg.reg2 = val >> 8;
	}
	constexpr void setFlag(FlagType flag, bool value)
	{
		if (value)
			F |=  static_cast<uint8_t>(flag); 
		else
			F &= ~static_cast<uint8_t>(flag); 
	}
};