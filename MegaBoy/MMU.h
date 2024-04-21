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
	MMU(GBCore& gbCore);
	bool ROMLoaded{ false };

	void resetMEM();
	void write8(memoryAddress addr, uint8_t val);
    uint8_t read8(memoryAddress addr) const;

	constexpr void directWrite(uint16_t addr, uint8_t val) { MEM[addr] = val; }
	constexpr uint8_t directRead(uint16_t addr) const { return MEM[addr]; }

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
	uint8_t MEM[0xFFFF]{};
	uint8_t bootROM[256]{};
	GBCore& gbCore;

	void loadROM(std::ifstream& ifs);
};
