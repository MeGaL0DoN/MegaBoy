#pragma once
#include "MBC.h"
#include <array>

class MBC2 : public MBC
{
public:
	MBC2(Cartridge& cartridge);

	uint8_t read(uint16_t addr) const override;
	void write(uint16_t addr, uint8_t val) override;
private:
	bool ramEnable{ false };
	uint8_t romBank{ 1 };
};