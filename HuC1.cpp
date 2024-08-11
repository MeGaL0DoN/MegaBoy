#include "HuC1.h"
#include "Cartridge.h"

uint8_t HuC1::read(uint16_t addr) const
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
		return s.ramEnable ? ram[(s.ramBank % cartridge.ramBanks) * 0x2000 + (addr - 0xA000)] : 0xFF;
	}

	return 0xFF;
}

void HuC1::write(uint16_t addr, uint8_t val)
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
		ram[(s.ramBank % cartridge.ramBanks) * 0x2000 + (addr - 0xA000)] = val;
	}
}