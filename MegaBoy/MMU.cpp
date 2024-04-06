#include <fstream>
#include "MMU.h"
#include "GBCore.h"

MMU::MMU(GBCore& gbCore) : gbCore(gbCore) {}

void MMU::write8(memoryAddress addr, uint8_t val)
{
	switch (addr)
	{
	case 0xFF00:
		// Allow writing only upper nibble to joypad register.
		MEM[0xFF00] = (MEM[0xFF00] & 0x0F) | (val & 0xF0);
		gbCore.input.modeChanged(MEM[0xFF00]);
		return;
	case 0xFF04:
		MEM[0xFF04] = 0;
		gbCore.cpu.DIV = 0;
		return;
	case 0xFF44:
		// read only LY register
		return;
	case 0xFF46:
		gbCore.ppu.OAMTransfer(val * 0x100);
		MEM[0xFF46] = val;
		return;
	case 0xFF47:
		gbCore.ppu.updatePalette(val, gbCore.ppu.BGpalette);
		MEM[0xFF47] = val;
		return;
	case 0xFF48:
		gbCore.ppu.updatePalette(val, gbCore.ppu.OBP0palette);
		MEM[0xFF48] = val;
		return;
	case 0xFF49:
		gbCore.ppu.updatePalette(val, gbCore.ppu.OBP1palette);
		MEM[0xFF49] = val;
		return;
	default:
		if (addr.inRange(0xFE0A, 0xFEFF))
			return;

		if (addr.inRange(0x0000, 0x7FFF))   
		{
			// ROM writes, TODO
			return;
		}

		if (addr.inRange(0xE000, 0xFDFF))
		{
			MEM[addr - 0x2000] = val;
			return;
		}

		if (addr.inRange(0x8000, 0x9FFF))
		{
			gbCore.ppu.VRAM[addr - 0x8000] = val;
			return;
		}

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
	switch (addr)
	{
	case 0xFF44:
		return gbCore.ppu.LY;
	case 0xFF0F:
		return MEM[0xFF0F] | 0xE0;

	default:
		if (addr.inRange(0xFEA0, 0xFEFF))
			return 0xFF;

		if (addr.inRange(0xE000, 0xFDFF))
			return MEM[addr - 0x2000];

		if (addr.inRange(0x8000, 0x9FFF))
			return gbCore.ppu.VRAM[addr - 0x8000];

		return MEM[addr];
	}
}

void MMU::resetMEM()
{
	std::memset(MEM, 0, sizeof(MEM));

	// reset input register
	MEM[0xFF00] = 0xCF; // Joypad

	// reset serial data transfer registers
	MEM[0xFF01] = 0x00; // SB
	MEM[0xFF02] = 0x7E; // SC

	// reset timer registers
	MEM[0xFF04] = 0xAB; // DIV
	MEM[0xFF05] = 0x00; // TIMA
	MEM[0xFF06] = 0x00; // TMA
	MEM[0xFF07] = 0x00; // TAC

	// reset interrupt flag registers
	MEM[0xFF0F] = 0xE1; // IF
	MEM[0xFFFF] = 0x00; // IE

	// reset sound registers
	MEM[0xFF10] = 0x80; // NR10
	MEM[0xFF11] = 0xBF; // NR11
	MEM[0xFF12] = 0xF3; // NR12
	MEM[0xFF14] = 0xBF; // NR14
	MEM[0xFF16] = 0x3F; // NR21
	MEM[0xFF17] = 0x00; // NR22
	MEM[0xFF19] = 0xBF; // NR24
	MEM[0xFF1A] = 0x7F; // NR30
	MEM[0xFF1B] = 0xFF; // NR31
	MEM[0xFF1C] = 0x9F; // NR32
	MEM[0xFF1E] = 0xBF; // NR33
	MEM[0xFF20] = 0xFF; // NR41
	MEM[0xFF21] = 0x00; // NR42
	MEM[0xFF22] = 0x00; // NR43
	MEM[0xFF23] = 0xBF; // NR30
	MEM[0xFF24] = 0x77; // NR50
	MEM[0xFF25] = 0xF3; // NR51
	MEM[0xFF26] = 0xF1; // NR52

	// reset PPU registers
	MEM[0xFF40] = 0x91; // LCDC
	MEM[0xFF41] = 0x85; // STAT
	MEM[0xFF42] = 0x00; // SCY
	MEM[0xFF43] = 0x00; // SCX
	MEM[0xFF44] = 0x00; // LY
	MEM[0xFF45] = 0x00; // LYC
	MEM[0xFF46] = 0x00; // DMA
	MEM[0xFF47] = 0xFC; // BGP
	MEM[0xFF48] = 0xFF; // OBP0
	MEM[0xFF49] = 0xFF; // OBP1
	MEM[0xFF4A] = 0x00; // WY
	MEM[0xFF4B] = 0x00; // WX
}


void MMU::loadROM(std::ifstream& ifs)
{
	resetMEM();
	gbCore.cpu.reset();
	gbCore.ppu.reset();
	gbCore.input.reset();

	std::ifstream::pos_type pos = ifs.tellg();
	ifs.seekg(0, std::ios::beg);
	ifs.read(reinterpret_cast<char*>(&MEM[0]), pos);
	ifs.close();

	ROMLoaded = true;
}