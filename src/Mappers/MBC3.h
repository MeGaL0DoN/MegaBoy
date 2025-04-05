#pragma once
#include <optional>
#include "RTC3.h"
#include "MBC.h"

struct MBC3State : MBCstate
{
	bool rtcModeActive { false };
	uint8_t ramBank { 0 };
};

class MBC3 : public MBC<MBC3State>
{
public:
	MBC3(Cartridge& cartridge, bool hasrtc) : MBC(cartridge)
	{
		if (hasrtc)
			rtc = RTC3{};
	}

	RTC* getRTC() override { return rtc.has_value() ? &rtc.value() : nullptr; }

	void saveBattery(std::ostream& st) const override
	{
		MBC::saveBattery(st);

		if (rtc.has_value())
		{
			updateRTC();
			rtc->saveBattery(st);
		}
	}
	bool loadBattery(std::istream& st) override
	{
		return load<false>(st);
	}
	void loadState(std::istream& st) override
	{
		load<true>(st);
	}

	void reset(bool resetBattery) override
	{
		MBC::reset(resetBattery);

		if (rtc.has_value())
		{
			lastRTCAccessCycles = 0;
			rtc->reset();
		}
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
		if (addr <= 0xBFFF)
		{
			if (!s.ramEnable)
				return 0xFF;

			if (s.rtcModeActive)
			{
				updateRTC();
				return rtc->s.latched ? rtc->s.latchedRegs.getReg(rtc->s.reg) : rtc->s.regs.getReg(rtc->s.reg);
			}
			else
				return ram[(s.ramBank & (cartridge.ramBanks - 1)) * 0x2000 + (addr - 0xA000)];
		}

		return 0xFF;
	}

	void write(uint16_t addr, uint8_t val) override
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
			else if (val <= 0x0C && rtc.has_value())
			{
				s.rtcModeActive = true;
				rtc->setReg(val);
			}
		}
		else if (addr <= 0x7FFF)
		{
			if (!rtc.has_value()) 
				return;

			if (val == 0x01 && rtc->s.latchWrite == 0x00)
			{
				updateRTC();
				rtc->s.latched = !rtc->s.latched;

				if (rtc->s.latched)
				{
					rtc->s.latchedRegs = rtc->s.regs;
					sramDirty = true;
				}
			}

			rtc->s.latchWrite = val;
		}
		else if (addr <= 0xBFFF)
		{
			if (!s.ramEnable) 
				return;

			if (s.rtcModeActive)
			{
				updateRTC();
				rtc->writeReg(val);
			}
			else
				ram[(s.ramBank & (cartridge.ramBanks - 1)) * 0x2000 + (addr - 0xA000)] = val;

			sramDirty = true;
		}
	}

private:
	mutable uint64_t lastRTCAccessCycles { 0 };
	mutable std::optional<RTC3> rtc;

	void resetBatteryState() override
	{
		MBC::resetBatteryState();

		if (rtc.has_value())
			rtc->reset();
	}

	template <bool saveState>
	bool load(std::istream& st)
	{
		if constexpr (saveState)
			ST_READ(s);

		if (!MBC::loadBattery(st))
			return false;

		if (rtc.has_value())
		{
			lastRTCAccessCycles = cartridge.getGBCycles();
			rtc->load<saveState>(st);
		}

		return true;
	}

	void updateRTC() const
	{
		rtc->addCycles(cartridge.getGBCycles() - lastRTCAccessCycles);
		lastRTCAccessCycles = cartridge.getGBCycles();
	}
};