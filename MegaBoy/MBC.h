#pragma once
#include <cstdint>
#include <vector>
#include <fstream>

class Cartridge;

class MBC
{
public:
	virtual ~MBC() {}
	MBC(Cartridge& cartridge);

	virtual uint8_t read(uint16_t addr) const = 0;
	virtual void write(uint16_t addr, uint8_t val) = 0;

	virtual void saveBattery(std::ofstream&) const;
	virtual void loadBattery(std::ifstream&);

	virtual void saveState(std::ofstream&) const {}
	virtual void loadState(std::ifstream&) {}

	virtual void reset(bool resetBattery)
	{
		ramEnable = false; 
		if (resetBattery) std::fill(ram.begin(), ram.end(), static_cast<uint8_t>(0));
	}

protected:
	const Cartridge& cartridge;

	const std::vector<uint8_t>& rom;
	std::vector<uint8_t> ram;
	bool ramEnable{ false };
};