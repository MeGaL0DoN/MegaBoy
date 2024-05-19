#pragma once
#include "MBC.h"

class MBC5 : public MBC
{
public:
	MBC5(Cartridge& cartridge, bool hasRumble) : MBC(cartridge), hasRumble(hasRumble) {}

	uint8_t read(uint16_t addr) const override;
	void write(uint16_t addr, uint8_t val) override;

	virtual void saveState(std::ofstream& st) const override;
	virtual void loadState(std::ifstream& st) override;

	virtual void reset(bool resetBattery) override { MBC::reset(resetBattery); romBank = 1; ramBank = 0; }
private:
	const bool hasRumble;

	uint16_t romBank { 1 };
	uint8_t ramBank { 0 };
};