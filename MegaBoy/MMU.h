#pragma once
#include <fstream>
#include <cstdint>
#include <array>

class GBCore;
class Cartridge;

class MMU
{
public:
	friend class Cartridge;
	MMU(GBCore& gbCore);

	void write8(uint16_t addr, uint8_t val);
    uint8_t read8(uint16_t addr) const;

private:
	std::array<uint8_t, 8192> WRAM{};
	std::array<uint8_t, 127> HRAM{};
	std::array<uint8_t, 256> bootROM{};

	GBCore& gbCore;
};
