#pragma once
#include "MBC.h"

struct emptyState {};

class RomOnlyMBC : public MBC<emptyState>
{
	using MBC::MBC;

public:
	uint8_t read(uint16_t addr) const override
	{
		return addr <= 0x7FFF ? rom[addr] : 0xFF;
	}
	void write(uint16_t addr, uint8_t val) override
	{
		// 32 kb ROMs don't have external RAM
		(void)val; (void)addr;
	}
};