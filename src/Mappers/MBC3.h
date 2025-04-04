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
	MBC3(Cartridge& cartridge, bool hasRTC) : MBC(cartridge)
	{
		if (hasRTC)
			RTC = RTC3{};
	}

	RTC* getRTC() override { return RTC.has_value() ? &RTC.value() : nullptr; }

	void saveBattery(std::ostream& st) const override
	{
		MBC::saveBattery(st);

		if (RTC.has_value())
		{
			updateRTC();
			RTC->saveBattery(st);
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

		if (RTC.has_value())
		{
			lastRTCAccessCycles = 0;
			RTC->reset();
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
				return RTC->s.latched ? RTC->s.latchedRegs.getReg(RTC->s.reg) : RTC->s.regs.getReg(RTC->s.reg);
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
			else if (val <= 0x0C && RTC.has_value())
			{
				s.rtcModeActive = true;
				RTC->setReg(val);
			}
		}
		else if (addr <= 0x7FFF)
		{
			if (!RTC.has_value()) 
				return;

			if (val == 0x01 && RTC->s.latchWrite == 0x00)
			{
				updateRTC();
				RTC->s.latched = !RTC->s.latched;

				if (RTC->s.latched)
				{
					RTC->s.latchedRegs = RTC->s.regs;
					sramDirty = true;
				}
			}

			RTC->s.latchWrite = val;
		}
		else if (addr <= 0xBFFF)
		{
			if (!s.ramEnable) 
				return;

			if (s.rtcModeActive)
			{
				updateRTC();
				RTC->writeReg(val);
			}
			else
				ram[(s.ramBank & (cartridge.ramBanks - 1)) * 0x2000 + (addr - 0xA000)] = val;

			sramDirty = true;
		}
	}

private:
	mutable uint64_t lastRTCAccessCycles { 0 };
	mutable std::optional<RTC3> RTC;

	void resetBatteryState() override
	{
		MBC::resetBatteryState();

		if (RTC.has_value())
			RTC->reset();
	}

	template <bool saveState>
	bool load(std::istream& st)
	{
		if constexpr (saveState)
			ST_READ(s);

		if (!MBC::loadBattery(st))
			return false;

		if (RTC.has_value())
		{
			lastRTCAccessCycles = cartridge.getGBCycles();
			RTC->load<saveState>(st);
		}

		return true;
	}

	void updateRTC() const
	{
		RTC->addCycles(cartridge.getGBCycles() - lastRTCAccessCycles);
		lastRTCAccessCycles = cartridge.getGBCycles();
	}
};