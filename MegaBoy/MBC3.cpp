#include "MBC3.h"
#include "Cartridge.h"

uint8_t MBC3::read(uint16_t addr) const
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
		if (s.rtcModeActive) return s.rtcReg; // to fix
		else return ram[(s.ramBank % cartridge.ramBanks) * 0x2000 + (addr - 0xA000)];
	}

	return 0xFF;
}

void MBC3::write(uint16_t addr, uint8_t val)
{
	if (addr <= 0x1FFF)
	{
		ramEnable = (val & 0x0F) == 0x0A;
	}
	else if (addr <= 0x3FFF)
	{
		s.romBank = val & 0x7F;
		if (s.romBank == 0) s.romBank = 1;
	}
	else if (addr <= 0x5FFF)
	{
		if (val <= 0x03)
		{
			s.ramBank = val;
			s.rtcModeActive = false;
		}
		else if (val <= 0x0C)
			s.rtcModeActive = true; // to fix
	}
	else if (addr <= 0x7FFF)
	{
		//todo rtc latch.
	}
	else if (addr <= 0xBFFF)
	{
		if (s.rtcModeActive) s.rtcReg = val; // to fix
		else ram[(s.ramBank % cartridge.ramBanks) * 0x2000 + (addr - 0xA000)] = val;
	}
}

void MBC3::saveState(std::ofstream& st) const
{
	MBC::saveBattery(st);
	st.write(reinterpret_cast<const char*>(&s), sizeof(s));
}

void MBC3::loadState(std::ifstream& st)
{
	MBC::loadBattery(st);
	st.read(reinterpret_cast<char*>(&s), sizeof(s));
}