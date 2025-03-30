#pragma once
#include "MBC.h"

struct HuC3State 
{
};

class HuC3 : public MBC<HuC3State>
{
public:
	using MBC::MBC;

	uint8_t read(uint16_t addr) const override
	{
		return 0xFF;
	}

	void write(uint16_t addr, uint8_t val) override
	{

	}

private:
};