#include "MBC1.h"
#include "Cartridge.h"

uint8_t MBC1::read(uint16_t addr) const
{
	if (addr <= 0x3FFF)
	{
		return rom[addr + lowROMOffset];
	}
	if (addr <= 0x7FFF)
	{
		return rom[(addr & 0x3FFF) + highROMOffset];
	}
	if (addr <= 0xBFFF)
	{
		if (!hasRAM || !ramEnable)
			return 0xFF;

		return ram[RAMOffset + (addr - 0xA000)];
	}

	return 0xFF;
}

void MBC1::write(uint16_t addr, uint8_t val)
{
	if (addr <= 0x1FFF)
	{
		ramEnable = ((val & 0xF) == 0xA);
	}
	else if (addr <= 0x3FFF)
	{
		bank1 = val & 0x1F;
		if (bank1 == 0) bank1 = 1;
		updateOffsets();
	}
	else if (addr <= 0x5FFF)
	{
		bank2 = val & 3;
		updateOffsets();
	}
	else if (addr <= 0x7FFF)
	{
		if (cartridge.romBanks <= 32 && cartridge.ramBanks <= 1) return;
		bankingMode = val & 0x1;
		updateOffsets();
	}
	else if (addr <= 0xBFFF)
	{
		if (!hasRAM || !ramEnable) return;
		ram[RAMOffset + (addr - 0xA000)] = val;
	}
}

void MBC1::updateOffsets()
{
	if (bankingMode == 0)
	{
		lowROMOffset = 0;
		RAMOffset = 0;
	}
	else
	{
		lowROMOffset = ((32 * bank2) % cartridge.romBanks) * 0x4000;
		if (hasRAM) RAMOffset = (bank2 % cartridge.ramBanks) * 0x2000;
	}

	highROMOffset = (((bank2 << 5) | bank1) % cartridge.romBanks) * 0x4000;
}