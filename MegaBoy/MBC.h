#pragma once
#include <cstdint>
#include <vector>

class Cartridge;

class MBC
{
public:
	MBC(const Cartridge& cartridge, bool hasRAM = false, bool hasBattery = false);

	virtual uint8_t read(uint16_t addr) const = 0;
	virtual void write(uint16_t addr, uint8_t val) = 0;

protected:
	const bool hasRAM;
	const bool hasBattery;

	const Cartridge& cartridge;
	const std::vector<uint8_t>& rom;
	std::vector<uint8_t> ram;
};