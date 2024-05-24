#pragma once
#include "MBC.h"
#include <thread>

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
			cartridge.timer.loadBattery(st);
	}

	void resetBatteryState() override
	{
		if (cartridge.hasTimer)
			cartridge.timer.reset();
	}
private:
	constexpr RTCTimer& RTC() { return cartridge.timer; }
	constexpr const RTCTimer& RTC() const { return cartridge.timer; }
};