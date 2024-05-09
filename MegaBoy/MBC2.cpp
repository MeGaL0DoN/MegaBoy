#include "MBC2.h"
#include "Cartridge.h"

MBC2::MBC2(Cartridge& cartridge) : MBC(cartridge)
{
	ram.resize(512);
	cartridge.hasRAM = true;
}

uint8_t MBC2::read(uint16_t addr) const
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
		return ramEnable ? ram[addr & 0x1FF] | 0xF0 : 0xFF;
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
			romBank = val & 0x0F;
			if (romBank == 0) romBank = 1;
		}
		else
			ramEnable = (val & 0x0F) == 0x0A;
	}
	else if (addr >= 0xA000 && addr <= 0xBFFF)
	{
		if (!ramEnable) return;
		ram[addr & 0x1FF] = val;
	}
}

void MBC2::saveState(std::ofstream& st) const
{
	MBC::saveBattery(st);
	st.write(reinterpret_cast<const char*>(&romBank), sizeof(romBank));
}

void MBC2::loadState(std::ifstream& st)
{
	MBC::loadBattery(st);
	st.read(reinterpret_cast<char*>(&romBank), sizeof(romBank));
}