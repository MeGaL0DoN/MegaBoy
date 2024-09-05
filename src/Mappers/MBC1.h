#pragma once
#include "MBC.h"

struct MBC1state : MBCstate
{
	uint8_t bankingMode{ 0 };
	uint8_t bank2{ 0 };

	uint32_t lowROMOffset{ 0 };
	uint32_t highROMOffset{ 0x4000 };
	uint32_t RAMOffset{ 0 };
};

class MBC1 : public MBC<MBC1state>
{
public:
	using MBC::MBC;

	uint8_t read(uint16_t addr) const override;
	void write(uint16_t addr, uint8_t val) override;
private:
	void updateOffsets();
};