#include "MBC5.h"
#include "Cartridge.h"
#include "bitOps.h"

uint8_t MBC5::read(uint16_t addr) const
{
	if (addr <= 0x3FFF)
	{
		return rom[addr];
	}
	if (addr <= 0x7FFF)
	{
		return rom[(s.romBank % cartridge.romBanks) * 0x4000 + (addr - 0x4000)];
	}
	if (addr <= 0xBFFF)
	{
		return ram[(s.ramBank % cartridge.ramBanks) * 0x2000 + (addr - 0xA000)];
	}

	return 0xFF;
}

void MBC5::write(uint16_t addr, uint8_t val)
{
	if (addr <= 0x1FFF)
	{
		s.ramEnable = (val & 0x0F) == 0x0A;
	}
	else if (addr <= 0x2FFF)
	{
		s.romBank = (s.romBank & 0xFF00) | val;
	}
	else if (addr <= 0x3FFF)
	{
		s.romBank = setBit(s.romBank, 9, val & 1);
	}
	else if (addr <= 0x5FFF)
	{
		s.ramBank = val & 0x0F;
	}
}