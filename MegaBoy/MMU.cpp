//#include <fstream>
#include <filesystem>
#include "MMU.h"
#include "GBCore.h"
#include <iostream>

MMU::MMU(GBCore& gbCore) : gbCore(gbCore) {}

void MMU::startDMATransfer()
{
	if (dmaTransfer)
	{
		dmaRestartRequest = true;
		return;
	}

	dmaTransfer = true;
	dmaCycles = 0;
	dmaSourceAddr = DMA * 0x100;

	if (dmaRestartRequest)
	{
		dmaDelayCycles = 1;
		dmaRestartRequest = false;
	}
	else dmaDelayCycles = 2;
}

void MMU::executeDMA()
{
	if (dmaTransfer)
	{
		if (dmaDelayCycles > 0)
			dmaDelayCycles--;
		else
		{
			gbCore.ppu.OAM[dmaCycles++] = read8<false>(dmaSourceAddr++);

			if (dmaRestartRequest)
			{
				dmaTransfer = false;
				startDMATransfer();
			}
			else if (dmaCycles >= DMA_CYCLES)
				dmaTransfer = false;
		}
	}
}

uint8_t writeCount{ 0 };

void MMU::write8(uint16_t addr, uint8_t val)
{
	if (addr == 0xFF46)
	{
		DMA = val;
		startDMATransfer();
		return;
	}
	else if (dmaInProgress() && (addr < 0xFF80 || addr > 0xFFFE)) // during DMA only HRAM can be accessed. TO FIX ON CGB
		return;

	if (addr <= 0x7FFF)
	{
		gbCore.cartridge.getMapper()->write(addr, val);
	}
	else if (addr <= 0x9FFF)
	{
		//if (gbCore.ppu.state == PPUMode::PixelTransfer)
		//{
		//	if (writeCount % 50 == 0) std::cout << "LY: " << +gbCore.ppu.LY << " CYCLES: " << gbCore.ppu.videoCycles << " LCDC: " << +gbCore.ppu.LCDC << " STAT: " << +gbCore.ppu.STAT << "\n";
		//	writeCount++;
		//}

		if (gbCore.ppu.state != PPUMode::PixelTransfer)
			gbCore.ppu.VRAM[addr - 0x8000] = val;
	}
	else if (addr <= 0xBFFF)
	{
		gbCore.cartridge.getMapper()->write(addr, val);
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
		if ((gbCore.ppu.state == PPUMode::HBlank || gbCore.ppu.state == PPUMode::VBlank) /*&& !dmaInProgress()*/)
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
		{
			// Allow writing only five upper bits to LCD status register.
			gbCore.ppu.STAT = (gbCore.ppu.STAT & 0x87) | (val & 0xF8);
			break;
		}
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

		case 0xFF10: 
			gbCore.apu.NR10 = val; break;
		case 0xFF11:
			gbCore.apu.NR11 = val; break;
		case 0xFF12: 
			gbCore.apu.NR12 = val; break;
		case 0xFF13:
			gbCore.apu.NR13 = val; break;
		case 0xFF14: 
			gbCore.apu.NR14 = val; break;
		case 0xFF16: 
			gbCore.apu.NR21 = val; break;
		case 0xFF17: 
			gbCore.apu.NR22 = val; break;
		case 0xFF18: 
			gbCore.apu.NR23 = val; break;
		case 0xFF19: 
			gbCore.apu.NR24 = val; break;
		case 0xFF1A: 
			gbCore.apu.NR30 = val; break;
		case 0xFF1B: 
			gbCore.apu.NR31 = val; break;
		case 0xFF1C: 
			gbCore.apu.NR32 = val; break;
		case 0xFF1D:
			gbCore.apu.NR33 = val; break;
		case 0xFF1E: 
			gbCore.apu.NR34 = val; break;
		case 0xFF20: 
			gbCore.apu.NR41 = val; break;
		case 0xFF21:
			gbCore.apu.NR42 = val; break;
		case 0xFF22: 
			gbCore.apu.NR43 = val; break;
		case 0xFF23: 
			gbCore.apu.NR44 = val; break;
		case 0xFF24: 
			gbCore.apu.NR50 = val; break;
		case 0xFF25:
			gbCore.apu.NR51 = val; break;
		case 0xFF26: 
			gbCore.apu.NR52 = val; break;
		}
	}
	else if (addr <= 0xFFFE)
	{
		HRAM[addr - 0xFF80] = val;
	}
	else
		gbCore.cpu.IE = val;
}

template uint8_t MMU::read8<true>(uint16_t) const;
template uint8_t MMU::read8<false>(uint16_t) const;

template <bool dmaBlocking>
uint8_t MMU::read8(uint16_t addr) const
{
	if constexpr (dmaBlocking)
	{
		if (dmaInProgress() && (addr < 0xFF80 || addr > 0xFFFE)) // during DMA only HRAM can be accessed. TO FIX ON CGB
			return 0xFF;
	}

	if (addr <= 0xFF && gbCore.cpu.executingBootROM)
	{
		return bootROM[addr];
	}
	if (addr <= 0x7FFF)
	{
		return gbCore.cartridge.getMapper()->read(addr);
	}
	if (addr <= 0x9FFF)
	{
		return gbCore.ppu.state == PPUMode::PixelTransfer ? 0xFF : gbCore.ppu.VRAM[addr - 0x8000];
	}
	if (addr <= 0xBFFF)
	{
		return gbCore.cartridge.getMapper()->read(addr);
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
		if ((gbCore.ppu.state == PPUMode::HBlank || gbCore.ppu.state == PPUMode::VBlank) /*&& !dmaInProgress()*/)
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
			return DMA;
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

		case 0xFF10: 
			return gbCore.apu.NR10;
		case 0xFF11: 
			return gbCore.apu.NR11;
		case 0xFF12: 
			return gbCore.apu.NR12;
		case 0xFF13: 
			return gbCore.apu.NR13;
		case 0xFF14: 
			return gbCore.apu.NR14;
		case 0xFF16: 
			return gbCore.apu.NR21;
		case 0xFF17: 
			return gbCore.apu.NR22;
		case 0xFF18: 
			return gbCore.apu.NR23;
		case 0xFF19: 
			return gbCore.apu.NR24;
		case 0xFF1A: 
			return gbCore.apu.NR30;
		case 0xFF1B:
			return gbCore.apu.NR31;
		case 0xFF1C: 
			return gbCore.apu.NR32;
		case 0xFF1D: 
			return gbCore.apu.NR33;
		case 0xFF1E: 
			return gbCore.apu.NR34;
		case 0xFF20: 
			return gbCore.apu.NR41;
		case 0xFF21: 
			return gbCore.apu.NR42;
		case 0xFF22:
			return gbCore.apu.NR43;
		case 0xFF23: 
			return gbCore.apu.NR44;
		case 0xFF24: 
			return gbCore.apu.NR50;
		case 0xFF25: 
			return gbCore.apu.NR51;
		case 0xFF26:
			return gbCore.apu.NR52;

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