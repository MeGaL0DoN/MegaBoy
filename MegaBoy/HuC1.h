#pragma once
#include "MBC.h"

class HuC1 : public MBC
{
public:
	using MBC::MBC;

	uint8_t read(uint16_t addr) const override;
	void write(uint16_t addr, uint8_t val) override;

	virtual void saveState(std::ofstream& st) const override;
	virtual void loadState(std::ifstream& st) override;
private:
	uint8_t romBank{ 1 };
	uint8_t ramBank{ 0 };
};