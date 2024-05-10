#pragma once
#include <cstdint>
#include <vector>
#include <fstream>

class Cartridge;

class MBC
{
public:
	MBC(Cartridge& cartridge);

	virtual uint8_t read(uint16_t addr) const = 0;
	virtual void write(uint16_t addr, uint8_t val) = 0;

	virtual void saveBattery(std::ofstream& st) const;
	virtual void loadBattery(std::ifstream& st);

	virtual void saveState(std::ofstream& st) const {}
	virtual void loadState(std::ifstream& st) {}

	virtual void reset() { ramEnable = false; std::fill(ram.begin(), ram.end(), 0); }

protected:
	const Cartridge& cartridge;

	const std::vector<uint8_t>& rom;
	bool ramEnable{ false };
	std::vector<uint8_t> ram;
};