#pragma once
#include "MBC.h"

struct MBC1state : MBCstate
{
	uint8_t bankingMode{ 0 };
	uint8_t bank2{ 0 };

	uint32_t lowROMOffset{ 0 };
	uint32_t highROMOffset{ 0x4000 };
	uint32_t RAMOffset{ 0 };
};

class MBC1 : public MBC<MBC1state>
{
public:
	using MBC::MBC;

	uint8_t read(uint16_t addr) const override
	{
		if (addr <= 0x3FFF)
		{
			return rom[(addr + s.lowROMOffset) & (rom.size() - 1)];
		}
		if (addr <= 0x7FFF)
		{
			return rom[((addr & 0x3FFF) + s.highROMOffset) & (rom.size() - 1)];
		}
		if (addr <= 0xBFFF)
		{
			if (!cartridge.hasRAM || !s.ramEnable) return 0xFF;
			return ram[(s.RAMOffset + (addr - 0xA000)) & (ram.size() - 1)];
		}

		return 0xFF;
	}

	void write(uint16_t addr, uint8_t val) override
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
			sramDirty = true;
			ram[(s.RAMOffset + (addr - 0xA000)) & (ram.size() - 1)] = val;
		}
	}

private:
	void updateOffsets()
	{
		if (s.bankingMode == 0)
		{
			s.lowROMOffset = 0;
			s.RAMOffset = 0;
		}
		else
		{
			s.lowROMOffset = ((32 * s.bank2) & (cartridge.romBanks - 1)) * 0x4000;

			if (cartridge.hasRAM)
				s.RAMOffset = (s.bank2 & (cartridge.ramBanks - 1)) * 0x2000;
		}

		s.highROMOffset = (((s.bank2 << 5) | s.romBank) & (cartridge.romBanks - 1)) * 0x4000;
	}
};