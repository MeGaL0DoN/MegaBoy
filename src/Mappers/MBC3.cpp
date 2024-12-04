#include "MBC3.h"
#include "../GBCore.h"
#include "../Utils/bitOps.h"

extern GBCore gbCore;
#define RTC cartridge.timer

void MBC3::updateRTC() const
{
	RTC.addRTCcycles(gbCore.totalCycles() - lastRTCAccessCycles);
	lastRTCAccessCycles = gbCore.totalCycles();
}

bool MBC3::loadBattery(std::istream& st) 
{
	if (!MBC::loadBattery(st))
		return false;

	if (cartridge.hasTimer)
	{
		lastRTCAccessCycles = gbCore.totalCycles();
		cartridge.timer.loadBattery(st);
	}

	return true;
}
void MBC3::saveBattery(std::ostream & st) const 
{
	MBC::saveBattery(st);

	if (cartridge.hasTimer)
	{
		updateRTC();
		cartridge.timer.saveBattery(st);
	}
}

uint8_t MBC3::read(uint16_t addr) const
{
	if (addr <= 0x3FFF)
	{
		return rom[addr];
	}
	if (addr <= 0x7FFF)
	{
		return rom[(s.romBank & (cartridge.romBanks - 1)) * 0x4000 + (addr - 0x4000)];
	}
	if (addr <= 0xBFFF)
	{
		if (!s.ramEnable) return 0xFF;

		if (s.rtcModeActive)
		{
			updateRTC();
			return RTC.s.latched ? RTC.s.latchedRegs.getReg(RTC.s.reg) : RTC.s.regs.getReg(RTC.s.reg);
		}		
		else 
			return ram[(s.ramBank & (cartridge.ramBanks - 1)) * 0x2000 + (addr - 0xA000)];
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
			s.rtcModeActive = true;
			RTC.setReg(val);
		}
	}
	else if (addr <= 0x7FFF)
	{
		if (val == 0x01 && RTC.s.latchWrite == 0x00)
		{
			updateRTC();
			RTC.s.latched = !RTC.s.latched;

			if (RTC.s.latched)
				RTC.s.latchedRegs = RTC.s.regs;
		}

		RTC.s.latchWrite = val;
	}
	else if (addr <= 0xBFFF)
	{
		if (!s.ramEnable) return;
		sramDirty = true;

		if (s.rtcModeActive)
		{
			updateRTC();
			RTC.writeReg(val);
		}
		else 
			ram[(s.ramBank & (cartridge.ramBanks - 1)) * 0x2000 + (addr - 0xA000)] = val;
	}
}