#pragma once
#include "MBC.h"

class EmptyMBC : public MBC
{
public:
	using MBC::MBC;

	constexpr uint8_t read(uint16_t addr) const override
	{
		(void)addr;
		return 0xFF;
	}
	constexpr void write(uint16_t addr, uint8_t val) override
	{
		(void)addr; (void)val;
		return;
	}
};