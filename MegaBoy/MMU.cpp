#include "MMU.h"
#include <fstream>

void MMU::write8(uint16_t addr, uint8_t val)
{
	MEM[addr] = val;

	if (read8(0xff02) == 0x81)
	{
		char c = read8(0xff01);
		printf("%c", c);
		MEM[0xff02] = 0x0;
	}
}
uint8_t MMU::read8(uint16_t addr)
{
	return MEM[addr];
}

void MMU::loadROM(std::string_view path)
{
	std::ifstream ifs(path.data(), std::ios::binary | std::ios::ate);
	std::ifstream::pos_type pos = ifs.tellg();

	ifs.seekg(0, std::ios::beg);
	ifs.read(reinterpret_cast<char*>(&MEM[0]), pos);
	ifs.close();
}