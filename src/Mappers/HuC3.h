#pragma once
#include "MBC.h"

struct HuC3State 
{
	uint8_t romBank { 0 };
	uint8_t ramBank { 0 };
	uint8_t selectedMode{ 0 };
};

class HuC3 : public MBC<HuC3State>
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
		if (addr >= 0xA000 && addr <= 0xBFFF)
		{
			switch (s.selectedMode)
			{
			case 0x00:
			case 0xA:
				return ram[(s.ramBank & (cartridge.ramBanks - 1)) * 0x2000 + (addr - 0xA000)];
			case 0xB:
				return 0xFF;
			case 0xC:
				return 0xFF;
			case 0xD:
				return 0xFF;
			case 0xE:
				return 0xFF;
			default:
				return 0xFF; // Any other value returns 0xFF. 
			}
		}

		return 0xFF;
	}

	void write(uint16_t addr, uint8_t val) override
	{
		if (addr <= 0x1FFF)
		{
			s.selectedMode = val;
		}
		else if (addr <= 0x3FFF)
		{
			s.romBank = val & 0x7F;
		}
		else if (addr <= 0x5FFF)
		{
			s.ramBank = val;
		}
		else if (addr >= 0xA000 && addr <= 0xBFFF)
		{
			switch (s.selectedMode)
			{
			case 0x00:
				break; // RAM is read-only, do nothing.
			case 0xA:
				ram[(s.ramBank & (cartridge.ramBanks - 1)) * 0x2000 + (addr - 0xA000)] = val;
			case 0xB:
				break;
			case 0xC:
				break;
			case 0xD:
				break;
			case 0xE:
				break;
			default:
				break; // Do nothing for any other value.
			}
		}
	}

private:
};