#pragma once
#include "MBC.h"
#include <iostream>

class NoMBC : public MBC
{
	using MBC::MBC;

public:
	constexpr uint8_t read(uint16_t addr) const override
	{
		return addr <= 0x7FFF ? data[addr] : 0xFF;
	}
	constexpr void write(uint16_t addr, uint8_t val) override
	{
		// 32 kb ROMs don't have external RAM
		return;
	}
};