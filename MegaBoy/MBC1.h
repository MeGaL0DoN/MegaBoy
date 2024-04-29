#pragma once
#include "MBC.h"

class MBC1 : public MBC
{
public:
	using MBC::MBC;

	uint8_t read(uint16_t addr) const override;
	void write(uint16_t addr, uint8_t val) override;
private:
	bool ramEnable{false};

	uint8_t bankingMode { 0};
	uint8_t bank1 {1};
	uint8_t bank2 {0};

	uint32_t lowROMOffset {0};
	uint32_t highROMOffset {0x4000};
	uint32_t RAMOffset {0};

	void updateOffsets();
};