#include "MBC3.h"
#include "Cartridge.h"
#include "bitOps.h"

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
		if (!s.ramEnable) return 0xFF;

		if (s.rtcModeActive)
		{
			return RTC().s.latched ? RTC().s.latchedRegs.getReg(RTC().s.reg) : RTC().s.regs.getReg(RTC().s.reg);
		}
		else return ram[(s.ramBank % cartridge.ramBanks) * 0x2000 + (addr - 0xA000)];
	}

	return 0xFF;
}

void MBC3::write(uint16_t addr, uint8_t val)
{
	if (addr <= 0x1FFF)
	{
		s.ramEnable = (val & 0x0F) == 0x0A;
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
			s.rtcModeActive = false;
			s.ramBank = val;
		}
		else if (val <= 0x0C)
		{
			s.rtcModeActive = true; // to fix
			RTC().setReg(val);
		}
	}
	else if (addr <= 0x7FFF)
	{
		if (val == 0x01 && RTC().s.latchWrite == 0x00)
		{
			RTC().s.latched = !RTC().s.latched;

			if (RTC().s.latched)
				RTC().s.latchedRegs = RTC().s.regs;
		}

		RTC().s.latchWrite = val;
	}
	else if (addr <= 0xBFFF)
	{
		if (!s.ramEnable) return;

		if (s.rtcModeActive)
		{
			RTC().writeReg(val);
		}
		else ram[(s.ramBank % cartridge.ramBanks) * 0x2000 + (addr - 0xA000)] = val;
	}
}