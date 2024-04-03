#pragma once
#include <fstream>
#include <cstdint>

struct memoryAddress
{
	uint16_t val;
	memoryAddress(uint16_t val) : val(val)
	{ }

	operator uint16_t() const { return val; }

	constexpr bool inRange(uint16_t start, uint16_t end)
	{
		return val >= start && val <= end;
	}
};

class GBCore;

class MMU
{
public:
	MMU(GBCore& cpu);

	void write8(memoryAddress addr, uint8_t val);
    uint8_t read8(memoryAddress addr);

    inline void write16(memoryAddress addr, uint16_t val)
	{
		write8(addr, val & 0xFF);
		write8(addr + 1, val >> 8);
	}
    inline uint16_t read16(memoryAddress addr)
	{
		return static_cast<uint16_t>(read8(addr + 1) << 8) | read8(addr);
	}

	constexpr void directWrite(uint16_t addr, uint8_t val) { MEM[addr] = val; }
	constexpr uint8_t directRead(uint16_t addr) { return MEM[addr]; }

	//inline uint16_t directRead16(memoryAddress addr)
	//{
	//	return static_cast<uint16_t>(directRead(addr + 1) << 8) | directRead(addr);
	//}

	void resetMEM();

	inline void loadROM(const wchar_t* path)
	{
		std::ifstream ifs(path, std::ios::binary | std::ios::ate);
		loadROM(ifs);
	}
	inline void loadROM(const char* path)
	{
		std::ifstream ifs(path, std::ios::binary | std::ios::ate);
		loadROM(ifs);
	}

private:
	uint8_t MEM[0xFFFF + 1]{};
	GBCore& gbCore;

	void loadROM(std::ifstream& ifs);
};
