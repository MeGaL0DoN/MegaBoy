#pragma once
#include <cstdint>
#include <vector>

class Cartridge;

class MBC
{
public:
	MBC(Cartridge& cartridge);

	virtual uint8_t read(uint16_t addr) const = 0;
	virtual void write(uint16_t addr, uint8_t val) = 0;

protected:
	const Cartridge& cartridge;
	const std::vector<uint8_t>& rom;
	std::vector<uint8_t> ram;
};