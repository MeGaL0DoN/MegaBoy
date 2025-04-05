#pragma once
#include "MBC.h"
#include "HuC3RTC.h"

struct HuC3State 
{
	uint8_t romBank { 0 };
	uint8_t ramBank { 0 };
	uint8_t selectedMode { 0 };
};

class HuC3 : public MBC<HuC3State>
{
public:
	using MBC::MBC;

	RTC* getRTC() override { return &rtc; }

	void saveBattery(std::ostream& st) const override
	{
		MBC::saveBattery(st);
		rtc.saveBattery(st);
	}
	bool loadBattery(std::istream& st) override
	{
		if (!MBC::loadBattery(st))
			return false;

		rtc.loadBattery(st);
		return true;
	}

	void saveState(std::ostream& st) const override
	{
		ST_WRITE(s);
		MBC::saveBattery(st);
		rtc.saveState(st);
	}
	void loadState(std::istream& st) override
	{
		ST_READ(s);
		MBC::loadBattery(st);
		rtc.loadState(st);
	}

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
				return 0xFF; // RTC command write register (write-only).
			case 0xC:
				return rtc.readCommandResponse();
			case 0xD:
				return rtc.readSemaphore();
			case 0xE:
				// I think 0xC0 is no signal for IR same as in HuC1, but not sure.
				return 0xC0;
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
			s.selectedMode = val & 0x0F;
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
				break; // read-only RAM mode, do nothing.
			case 0xA:
				ram[(s.ramBank & (cartridge.ramBanks - 1)) * 0x2000 + (addr - 0xA000)] = val;
				break;
			case 0xB:
				rtc.writeCommand(val);
				break;
			case 0xC:
				break; // RTC command response register (read-only).
			case 0xD:
				rtc.writeSemaphore(val);
				break;
			case 0xE:
				break; // IR, ignore writes.
			default:
				break; // Do nothing for any other value.
			}

			sramDirty = true;
		}
	}

private:
	mutable HuC3RTC rtc{};

	void resetBatteryState() override
	{
		MBC::resetBatteryState();
		rtc.reset();
	}
};