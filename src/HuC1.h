#pragma once
#include "MBC.h"

struct HuC1State : MBCstate
{
	uint8_t ramBank{ 0 };
};

class HuC1 : public MBC<HuC1State>
{
public:
	using MBC::MBC;

	uint8_t read(uint16_t addr) const override;
	void write(uint16_t addr, uint8_t val) override;
};