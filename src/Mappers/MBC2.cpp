#include "MBC2.h"

MBC2::MBC2(Cartridge& cartridge) : MBC(cartridge)
{
	ram.resize(512);
}

uint8_t MBC2::read(uint16_t addr) const
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
		return s.ramEnable ? ram[addr & 0x1FF] | 0xF0 : 0xFF;
	}

	return 0xFF;
}

void MBC2::write(uint16_t addr, uint8_t val)
{
	if (addr <= 0x3FFF)
	{
		bool romBankBit = (addr >> 8) & 1;

		if (romBankBit)
		{
			s.romBank = val & 0x0F;
			if (s.romBank == 0) s.romBank = 1;
		}
		else
			s.ramEnable = (val & 0x0F) == 0x0A;
	}
	else if (addr >= 0xA000 && addr <= 0xBFFF)
	{
		if (!s.ramEnable) return;
		sramDirty = true;
		ram[addr & 0x1FF] = val;
	}
}