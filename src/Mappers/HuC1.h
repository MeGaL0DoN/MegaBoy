#pragma once
#include "MBC.h"

struct HuC1State : MBCstate
{
	uint8_t ramBank { 0 };
};

class HuC1 : public MBC<HuC1State>
{
public:
	using MBC::MBC;

	uint8_t read(uint16_t addr) const override
	{
		if (addr <= 0x3FFF)
		{
			return rom[addr];
		}
		if (addr <= 0x7FFF)
		{
			return rom[(s.romBank & (cartridge.romBanks - 1)) * 0x4000 + (addr - 0x4000)];
		}
		if (addr <= 0xBFFF)
		{
			return s.ramEnable ? ram[(s.ramBank & (cartridge.ramBanks - 1)) * 0x2000 + (addr - 0xA000)] : 0xFF;
		}

		return 0xFF;
	}

	void write(uint16_t addr, uint8_t val) override
	{
		if (addr <= 0x1FFF)
		{
			s.ramEnable = (val & 0x0F) != 0x0E;
		}
		else if (addr <= 0x3FFF)
		{
			s.romBank = val;
			if (s.romBank == 0) s.romBank = 1;
		}
		else if (addr <= 0x5FFF)
		{
			s.ramBank = val;
		}
		else if (addr >= 0xA000 && addr <= 0xBFFF)
		{
			if (!s.ramEnable) return;
			sramDirty = true;
			ram[(s.ramBank & (cartridge.ramBanks - 1)) * 0x2000 + (addr - 0xA000)] = val;
		}
	}
};