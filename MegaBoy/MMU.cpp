#include <fstream>
#include <filesystem>
#include "MMU.h"
#include "GBCore.h"

MMU::MMU(GBCore& gbCore) : gbCore(gbCore) {}

void MMU::write8(uint16_t addr, uint8_t val)
{
	if (addr <= 0x7FFF)
	{
		gbCore.cartridge.mapper.get()->write(addr, val);
	}
	else if (addr <= 0x9FFF)
	{
		if (gbCore.ppu.state != PPUMode::PixelTransfer)
			gbCore.ppu.VRAM[addr - 0x8000] = val;
	}
	else if (addr <= 0xBFFF)
	{
		gbCore.cartridge.mapper.get()->write(addr, val);
	}
	else if (addr <= 0xDFFF)
	{
		WRAM[addr - 0xC000] = val;
	}
	else if (addr <= 0xFDFF)
	{
		WRAM[addr - 0xE000] = val;
	}
	else if (addr <= 0xFE9F)
	{
		if ((gbCore.ppu.state == PPUMode::HBlank || gbCore.ppu.state == PPUMode::VBlank) && !gbCore.ppu.dmaTransfer)
			gbCore.ppu.OAM[addr - 0xFE00] = val;
	}
	else if (addr <= 0xFEFF)
	{
		// TODO: prohibited area;
		return;
	}
	else if (addr <= 0xFF7F)
	{
		switch (addr)
		{
		case 0xFF00:
			gbCore.input.setJoypadReg(val);
			break;
		case 0xFF01:
			gbCore.serial.serial_reg = val;
			break;
		case 0xFF02:
			gbCore.serial.writeSerialControl(val);
			break;
		case 0xFF04:
			gbCore.cpu.DIV_reg = 0;
			gbCore.cpu.DIV_COUNTER = 0;
			break;
		case 0xFF05:
			gbCore.cpu.TIMA_reg = val;
			break;
		case 0xFF06:
			gbCore.cpu.TMA_reg = val;
			break;
		case 0xFF07:
			gbCore.cpu.TAC_reg = val;
			break;
		case 0xFF0F:
			gbCore.cpu.IF = val | 0xE0;
			break;
		case 0xFF40:
			gbCore.ppu.LCDC = val;
			if (!getBit(val, 7)) gbCore.ppu.disableLCD();
			break;
		case 0xFF41:
			// Allow writing only five upper bits to LCD status register.
			gbCore.ppu.STAT = (gbCore.ppu.STAT & 0x87) | (val & 0xF8);
			break;
		case 0xFF42:
			gbCore.ppu.SCY = val;
			break;
		case 0xFF43:
			gbCore.ppu.SCX = val;
			break;
		case 0xFF44:
			// read only LY register
			break;
		case 0xFF45:
			gbCore.ppu.LYC = val;
			break;
		case 0xFF46:
			gbCore.ppu.DMA = val;
			gbCore.ppu.startDMATransfer();
			break;
		case 0xFF47:
			gbCore.ppu.BGP = val;
			gbCore.ppu.updatePalette(val, gbCore.ppu.BGpalette);
			break;
		case 0xFF48:
			gbCore.ppu.OBP0 = val;
			gbCore.ppu.updatePalette(val, gbCore.ppu.OBP0palette);
			break;
		case 0xFF49:
			gbCore.ppu.OBP1 = val;
			gbCore.ppu.updatePalette(val, gbCore.ppu.OBP1palette);
			break;
		case 0xFF4A:
			gbCore.ppu.WY = val;
			break;
		case 0xFF4B:
			gbCore.ppu.WX = val;
			break;
		}
	}
	else if (addr <= 0xFFFE)
	{
		HRAM[addr - 0xFF80] = val;
	}
	else
		gbCore.cpu.IE = val;
}

uint8_t MMU::read8(uint16_t addr) const
{
	if (addr <= 0xFF && gbCore.cpu.executingBootROM)
	{
		return bootROM[addr];
	}
	if (addr <= 0x7FFF)
	{
		return gbCore.cartridge.mapper.get()->read(addr);
	}
	if (addr <= 0x9FFF)
	{
		return gbCore.ppu.state == PPUMode::PixelTransfer ? 0xFF : gbCore.ppu.VRAM[addr - 0x8000];
	}
	if (addr <= 0xBFFF)
	{
		return gbCore.cartridge.mapper.get()->read(addr);
	}
	if (addr <= 0xDFFF)
	{
		return WRAM[addr - 0xC000];
	}
	if (addr <= 0xFDFF)
	{
		return WRAM[addr - 0xE000];
	}
	if (addr <= 0xFE9F)
	{
		if ((gbCore.ppu.state == PPUMode::HBlank || gbCore.ppu.state == PPUMode::VBlank) && !gbCore.ppu.dmaTransfer)
			return gbCore.ppu.OAM[addr - 0xFE00];

		return 0xFF;
	}
	if (addr <= 0xFEFF)
	{
		// TODO: prohibited area;
		return 0xFF;
	}
	if (addr <= 0xFF7F)
	{
		switch (addr)
		{
		case 0xFF00:
			return gbCore.input.getJoypadReg();
		case 0xFF01:
			return gbCore.serial.serial_reg;
		case 0xFF02:
			return gbCore.serial.serial_control;
		case 0xFF04:
			return gbCore.cpu.DIV_reg;
		case 0xFF05:
			return gbCore.cpu.TIMA_reg;
		case 0xFF06:
			return gbCore.cpu.TMA_reg;
		case 0xFF07:
			return gbCore.cpu.TAC_reg;
		case 0xFF0F:
			return gbCore.cpu.IF;
		case 0xFF40:
			return gbCore.ppu.LCDC;
		case 0xFF41:
			return gbCore.ppu.STAT;
		case 0xFF42:
			return gbCore.ppu.SCY;
		case 0xFF43:
			return gbCore.ppu.SCX;
		case 0xFF44:
			return gbCore.ppu.LY;
		case 0xFF45:
			return gbCore.ppu.LYC;
		case 0xFF46:
			return gbCore.ppu.DMA;
		case 0xFF47:
			return gbCore.ppu.BGP;
		case 0xFF48:
			return gbCore.ppu.OBP0;
		case 0xFF49:
			return gbCore.ppu.OBP1;
		case 0xFF4A:
			return gbCore.ppu.WY;
		case 0xFF4B:
			return gbCore.ppu.WX;
		default:
			return 0xFF;
		}
	}
	if (addr <= 0xFFFE)
	{
		return HRAM[addr - 0xFF80];
	}
	else
		return gbCore.cpu.IE;
}