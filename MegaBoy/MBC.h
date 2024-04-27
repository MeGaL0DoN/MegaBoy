#pragma once
#include <cstdint>

class MBC
{
public:
	MBC(uint8_t* data) : data(data) {}

	virtual uint8_t read(uint16_t addr) const = 0;
	virtual void write(uint16_t addr, uint8_t val) = 0;

protected:
	uint8_t* data;
};