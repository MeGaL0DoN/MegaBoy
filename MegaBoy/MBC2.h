#pragma once
#include "MBC.h"
#include <array>

class MBC2 : public MBC
{
public:
	MBC2(Cartridge& cartridge);

	uint8_t read(uint16_t addr) const override;
	void write(uint16_t addr, uint8_t val) override;

	virtual void saveState(std::ofstream& st) const override;
	virtual void loadState(std::ifstream& st) override;
private:
	uint8_t romBank{ 1 };
};