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

	void saveBattery(std::ofstream& st) const override
	{
		MBC::saveBattery(st);

		if (cartridge.hasTimer)
			cartridge.timer.saveBattery(st);
	}

	void loadBattery(std::ifstream& st) override
	{
		MBC::loadBattery(st);

		if (cartridge.hasTimer)
		{
			lastRTCAccessCycles = 0;
			cartridge.timer.loadBattery(st);
		}
	}

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