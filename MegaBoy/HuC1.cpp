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
		return rom[(romBank % cartridge.romBanks) * 0x4000 + (addr - 0x4000)];
	}
	if (addr <= 0xBFFF)
	{
		return ramEnable ? ram[(ramBank % cartridge.ramBanks) * 0x2000 + (addr - 0xA000)] : 0xFF;
	}

	return 0xFF;
}

void HuC1::write(uint16_t addr, uint8_t val)
{
	if (addr <= 0x1FFF)
	{
		ramEnable = (val & 0x0F) != 0x0E;
	}
	else if (addr <= 0x3FFF)
	{
		romBank = val;
		if (romBank == 0) romBank = 1;
	}
	else if (addr <= 0x5FFF)
	{
		ramBank = val;
	}
	else if (addr >= 0xA000 && addr <= 0xBFFF)
	{
		if (!ramEnable) return;
		ram[(ramBank % cartridge.ramBanks) * 0x2000 + (addr - 0xA000)] = val;
	}
}

void HuC1::saveState(std::ofstream& st) const
{
	MBC::saveBattery(st);
	st.write(reinterpret_cast<const char*>(&romBank), sizeof(romBank));
	st.write(reinterpret_cast<const char*>(&ramBank), sizeof(ramBank));
}

void HuC1::loadState(std::ifstream& st)
{
	MBC::loadBattery(st);
	st.read(reinterpret_cast<char*>(&romBank), sizeof(romBank));
	st.read(reinterpret_cast<char*>(&ramBank), sizeof(ramBank));
}