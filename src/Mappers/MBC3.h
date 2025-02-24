#pragma once
#include "MBC.h"

struct MBC3State : MBCstate
{
	bool rtcModeActive{ false };
	uint8_t ramBank{ 0 };
};

class MBC3 : public MBC<MBC3State>
{
public:
	MBC3(Cartridge& cartridge) : MBC(cartridge)
	{ }

	uint8_t read(uint16_t addr) const override;
	void write(uint16_t addr, uint8_t val) override;

	void saveBattery(std::ostream& st) const override;
	bool loadBattery(std::istream& st) override;

	void reset(bool resetBattery) override
	{
		lastRTCAccessCycles = 0;
		MBC::reset(resetBattery);
	}
private:
	void resetBatteryState() override
	{
		MBC::resetBatteryState();

		if (cartridge.hasTimer)
			cartridge.timer.reset();
	}

	mutable uint64_t lastRTCAccessCycles{ 0 };
	void updateRTC() const;
};