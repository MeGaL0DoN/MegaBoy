#pragma once
#include <cstdint>
#include <fstream>

struct MBCBase
{
	virtual ~MBCBase() {}

	virtual uint8_t read(uint16_t addr) const = 0;
	virtual void write(uint16_t addr, uint8_t val) = 0;

	virtual void saveState(std::ofstream& st) const = 0;
	virtual void loadState(std::ifstream& st) = 0;

	virtual void reset(bool resetBattery) = 0;
};