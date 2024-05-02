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
		return rom[(romBank % cartridge.romBanks) * 0x4000 + (addr - 0x4000)];
	}
	if (addr <= 0xBFFF)
	{
		if (rtcModeActive) return rtcReg; // to fix
		else return ram[(ramBank % cartridge.ramBanks) * 0x2000 + (addr - 0xA000)];
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
		romBank = val & 0x7F;
		if (romBank == 0) romBank = 1;
	}
	else if (addr <= 0x5FFF)
	{
		if (val <= 0x03)
		{
			ramBank = val;
			rtcModeActive = false;
		}
		else if (val <= 0x0C)
			rtcModeActive = true; // to fix
	}
	else if (addr <= 0x7FFF)
	{
		//todo rtc latch.
	}
	else if (addr <= 0xBFFF)
	{
		if (rtcModeActive) rtcReg = val; // to fix
		else ram[(ramBank % cartridge.ramBanks) * 0x2000 + (addr - 0xA000)] = val;
	}
}