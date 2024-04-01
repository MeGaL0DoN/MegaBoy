#include <fstream>
#include "MMU.h"
#include "GBCore.h"

MMU::MMU(GBCore& gbCore) : gbCore(gbCore) {}

void MMU::write8(memoryAddress addr, uint8_t val)
{
	if (addr.inRange(0xFE0A, 0xFEFF))
		return;

	if (addr.inRange(0xE000, 0xFDFF))
	{
		MEM[addr - 0x2000] = val;
		return;
	}

	switch (addr)
	{
	case 0xFF00:
		// Allow writing only upper nibble to joypad register.
		MEM[0xFF00] = (MEM[0xFF00] & 0x0F) | (val & 0xF0);
		return;
	case 0xFF04:
		MEM[0xFF04] = 0;
		gbCore.cpu.DIV = 0;
		return;
	default:
		MEM[addr] = val;
		break;
	}

	if (read8(0xff02) == 0x81)
	{
		char c = read8(0xff01);
		printf("%c", c);
		MEM[0xff02] = 0x0;
	}
}
uint8_t MMU::read8(memoryAddress addr)
{
	if (addr.inRange(0xFEA0, 0xFEFF)) 
		return 0xFF;

	if (addr.inRange(0xE000, 0xFDFF)) 
		return MEM[addr - 0x2000];

	switch (addr)
	{
	default:
		return MEM[addr];
	}
}

void MMU::loadROM(std::string_view path)
{
	std::memset(MEM, 0, sizeof(MEM));

	// reset input register
	MEM[0xFF00] = 0xFF;

	std::ifstream ifs(path.data(), std::ios::binary | std::ios::ate);
	std::ifstream::pos_type pos = ifs.tellg();

	ifs.seekg(0, std::ios::beg);
	ifs.read(reinterpret_cast<char*>(&MEM[0]), pos);
	ifs.close();
}