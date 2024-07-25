#pragma once
#include "MBC.h"

struct MBC5State
{
	uint16_t romBank : 9 { 1 };
	uint8_t ramBank : 4 { 0 };
	bool ramEnable : 1 { false };
};

class MBC5 : public MBC<MBC5State>
{
public:
	MBC5(Cartridge& cartridge, bool hasRumble) : MBC(cartridge), hasRumble(hasRumble) {}

	uint8_t read(uint16_t addr) const override;
	void write(uint16_t addr, uint8_t val) override;
private:
	const bool hasRumble;
};