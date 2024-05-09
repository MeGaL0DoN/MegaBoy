#include "MBC1.h"
#include "Cartridge.h"

uint8_t MBC1::read(uint16_t addr) const
{
	if (addr <= 0x3FFF)
	{
		return rom[addr + s.lowROMOffset];
	}
	if (addr <= 0x7FFF)
	{
		return rom[(addr & 0x3FFF) + s.highROMOffset];
	}
	if (addr <= 0xBFFF)
	{
		if (!cartridge.hasRAM || !ramEnable)
			return 0xFF;

		return ram[s.RAMOffset + (addr - 0xA000)];
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
		s.bank1 = val & 0x1F;
		if (s.bank1 == 0) s.bank1 = 1;
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
		if (!cartridge.hasRAM || !ramEnable) return;
		ram[s.RAMOffset + (addr - 0xA000)] = val;
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

	s.highROMOffset = (((s.bank2 << 5) | s.bank1) % cartridge.romBanks) * 0x4000;
}

void MBC1::saveState(std::ofstream& st) const
{
	MBC::saveBattery(st);
	st.write(reinterpret_cast<const char*>(&s), sizeof(s));
}

void MBC1::loadState(std::ifstream& st)
{
	MBC::loadBattery(st);
	st.read(reinterpret_cast<char*>(&s), sizeof(s));
}