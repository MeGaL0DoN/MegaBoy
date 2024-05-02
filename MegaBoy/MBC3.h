#pragma once
#include "MBC.h"

class MBC3 : public MBC
{
public:
	MBC3(Cartridge& cartridge, bool hasTimer) : MBC(cartridge), hasTimer(hasTimer) {}

	uint8_t read(uint16_t addr) const override;
	void write(uint16_t addr, uint8_t val) override;
private:
	const bool hasTimer;

	bool ramEnable{ false };
	uint8_t romBank{1};

	bool rtcModeActive {false};
	uint8_t ramBank {0};
	uint8_t rtcReg{0}; // to fix
};