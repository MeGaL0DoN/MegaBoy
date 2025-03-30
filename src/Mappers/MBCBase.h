#pragma once
#include <cstdint>
#include <iostream>
#include "RTC.h"

struct MBCBase
{
	virtual ~MBCBase() {}

	virtual uint8_t read(uint16_t addr) const = 0;
	virtual void write(uint16_t addr, uint8_t val) = 0;

	virtual void saveState(std::ostream& st) const = 0;
	virtual void loadState(std::istream& st) = 0;

	virtual void saveBattery(std::ostream& st) const = 0;
	virtual bool loadBattery(std::istream& st) = 0;

	virtual void reset(bool resetBattery) = 0;

	virtual uint16_t getCurrentRomBank() = 0;
	virtual RTC* getRTC() { return nullptr; }

	bool sramDirty { false };
};