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
		return rom[(romBank % cartridge.romBanks) * 0x4000 + (addr - 0x4000)];
	}
	if (addr <= 0xBFFF)
	{
		return ram[(ramBank % cartridge.ramBanks) * 0x2000 + (addr - 0xA000)];
	}

	return 0xFF;
}

void MBC5::write(uint16_t addr, uint8_t val)
{
	if (addr <= 0x1FFF)
	{
		ramEnable = (val & 0x0F) == 0x0A;
	}
	else if (addr <= 0x2FFF)
	{
		romBank = (romBank & 0xFF00) | val;
	}
	else if (addr <= 0x3FFF)
	{
		romBank = setBit(romBank, 9, val & 1);
	}
	else if (addr <= 0x5FFF)
	{
		ramBank = val & 0x0F;
	}
}