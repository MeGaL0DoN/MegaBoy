#include "MBC1.h"

uint8_t MBC1::read(uint16_t addr) const
{
	if (addr <= 0x3FFF)
	{
		return rom[(addr + s.lowROMOffset) % rom.size()];
	}
	if (addr <= 0x7FFF)
	{
		return rom[((addr & 0x3FFF) + s.highROMOffset) % rom.size()];
	}
	if (addr <= 0xBFFF)
	{
		if (!cartridge.hasRAM || !s.ramEnable) return 0xFF;
		return ram[(s.RAMOffset + (addr - 0xA000)) % ram.size()];
	}

	return 0xFF;
}

void MBC1::write(uint16_t addr, uint8_t val)
{
	if (addr <= 0x1FFF)
	{
		s.ramEnable = ((val & 0xF) == 0xA);
	}
	else if (addr <= 0x3FFF)
	{
		s.romBank = val & 0x1F;
		if (s.romBank == 0) s.romBank = 1;
		updateOffsets();
	}
	else if (addr <= 0x5FFF)
	{
		s.bank2 = val & 3;
		updateOffsets();
	}
	else if (addr <= 0x7FFF)
	{
		if (cartridge.romBanks <= 32 && cartridge.ramBanks <= 1) return;
		s.bankingMode = val & 0x1;
		updateOffsets();
	}
	else if (addr <= 0xBFFF)
	{
		if (!cartridge.hasRAM || !s.ramEnable) return;
		ram[(s.RAMOffset + (addr - 0xA000)) % ram.size()] = val;
	}
}

void MBC1::updateOffsets()
{
	if (s.bankingMode == 0)
	{
		s.lowROMOffset = 0;
		s.RAMOffset = 0;
	}
	else
	{
		s.lowROMOffset = ((32 * s.bank2) % cartridge.romBanks) * 0x4000;
		if (cartridge.hasRAM) s.RAMOffset = (s.bank2 % cartridge.ramBanks) * 0x2000;
	}

	s.highROMOffset = (((s.bank2 << 5) | s.romBank) % cartridge.romBanks) * 0x4000;
}