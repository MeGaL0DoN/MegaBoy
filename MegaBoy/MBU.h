#pragma once
#include <cstdint>
#include <string_view>

class MBU
{
public:
	uint8_t MEM[0xFFFF]{};

	void write8(uint16_t addr, uint8_t val);
    uint8_t& read8(uint16_t addr);

    inline void write16(uint16_t addr, uint16_t val)
	{
		write8(addr, val & 0xFF);
		write8(addr + 1, val >> 8);
	}
    inline uint16_t read16(uint16_t addr)
	{
		return static_cast<uint16_t>(read8(addr + 1) << 8) | read8(addr);
	}

	void loadROM(std::string_view path);

private:
};
