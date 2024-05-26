#include "MMU.h"
#include "GBCore.h"
#include <iostream>

MMU::MMU(GBCore& gbCore) : gbCore(gbCore) {}

void MMU::saveState(std::ofstream& st)
{
	st.write(reinterpret_cast<char*>(&dma), sizeof(dma));
	st.write(reinterpret_cast<char*>(WRAM.data()), sizeof(WRAM));
	st.write(reinterpret_cast<char*>(HRAM.data()), sizeof(HRAM));
}

void MMU::loadState(std::ifstream& st)
{
	st.read(reinterpret_cast<char*>(&dma), sizeof(dma));
	st.read(reinterpret_cast<char*>(WRAM.data()), sizeof(WRAM));
	st.read(reinterpret_cast<char*>(HRAM.data()), sizeof(HRAM));
}

void MMU::startDMATransfer()
{
	if (dma.transfer)
	{
		dma.restartRequest = true;
		return;
	}

	dma.transfer = true;
	dma.cycles = 0;
	dma.sourceAddr = dma.reg * 0x100;

	if (dma.restartRequest)
	{
		dma.delayCycles = 1;
		dma.restartRequest = false;
	}
	else dma.delayCycles = 2;
}

void MMU::executeDMA()
{
	if (dma.transfer)
	{
		if (dma.delayCycles > 0)
			dma.delayCycles--;
		else
		{
			gbCore.ppu.OAM[dma.cycles++] = read8<false>(dma.sourceAddr++);

			if (dma.restartRequest)
			{
				dma.transfer = false;
				startDMATransfer();
			}
			else if (dma.cycles >= DMA_CYCLES)
				dma.transfer = false;
		}
	}
}

void MMU::write8(uint16_t addr, uint8_t val)
{
	if (addr == 0xFF46)
	{
		dma.reg = val;
		startDMATransfer();
		return;
	}
	else if (dmaInProgress() && (addr < 0xFF80 || addr > 0xFFFE)) // during DMA only HRAM can be accessed. // TODO: DISABLE ON GBC!
		return;

	if (addr <= 0x7FFF)
	{
		gbCore.cartridge.getMapper()->write(addr, val);
	}
	else if (addr <= 0x9FFF)
	{
		if (gbCore.ppu.canAccessVRAM)
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
		if (gbCore.ppu.canAccessOAM)
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
			gbCore.serial.s.serial_reg = val;
			break;
		case 0xFF02:
			gbCore.serial.writeSerialControl(val);
			break;
		case 0xFF04:
			gbCore.cpu.s.DIV_reg = 0;
			gbCore.cpu.s.DIV_COUNTER = 0;
			break;
		case 0xFF05:
			gbCore.cpu.s.TIMA_reg = val;
			break;
		case 0xFF06:
			gbCore.cpu.s.TMA_reg = val;
			break;
		case 0xFF07:
			gbCore.cpu.s.TAC_reg = val;
			break;
		case 0xFF0F:
			gbCore.cpu.s.IF = val | 0xE0;
			break;
		case 0xFF40:
			gbCore.ppu.regs.LCDC = val;
			if (!getBit(val, 7)) gbCore.ppu.disableLCD();
			break;
		case 0xFF41:
		{
			// Handle spurious STAT interrupts   // TODO: DISABLE ON GBC!
			gbCore.ppu.s.newStatVal = (gbCore.ppu.regs.STAT & 0x87) | (val & 0xF8);
			gbCore.ppu.regs.STAT = 0xFF;
			gbCore.ppu.s.statRegChanged = true;
			break;
		}
		case 0xFF42:
			gbCore.ppu.regs.SCY = val;
			break;
		case 0xFF43:
			gbCore.ppu.regs.SCX = val;
			break;
		case 0xFF44:
			// read only LY register
			break;
		case 0xFF45:
			gbCore.ppu.regs.LYC = val;
			break;
		case 0xFF47:
			gbCore.ppu.regs.BGP = val;
			gbCore.ppu.updatePalette(val, gbCore.ppu.BGpalette);
			break;
		case 0xFF48:
			gbCore.ppu.regs.OBP0 = val;
			gbCore.ppu.updatePalette(val, gbCore.ppu.OBP0palette);
			break;
		case 0xFF49:
			gbCore.ppu.regs.OBP1 = val;
			gbCore.ppu.updatePalette(val, gbCore.ppu.OBP1palette);
			break;
		case 0xFF4A:
			gbCore.ppu.regs.WY = val;
			break;
		case 0xFF4B:
			gbCore.ppu.regs.WX = val;
			break;

		case 0xFF10: 
			gbCore.apu.regs.NR10 = val; break;
		case 0xFF11:
			gbCore.apu.regs.NR11 = val; break;
		case 0xFF12: 
			gbCore.apu.regs.NR12 = val; break;
		case 0xFF13:
			gbCore.apu.regs.NR13 = val; break;
		case 0xFF14: 
			gbCore.apu.regs.NR14 = val; break;
		case 0xFF16: 
			gbCore.apu.regs.NR21 = val; break;
		case 0xFF17: 
			gbCore.apu.regs.NR22 = val; break;
		case 0xFF18: 
			gbCore.apu.regs.NR23 = val; break;
		case 0xFF19: 
			gbCore.apu.regs.NR24 = val; break;
		case 0xFF1A: 
			gbCore.apu.regs.NR30 = val; break;
		case 0xFF1B: 
			gbCore.apu.regs.NR31 = val; break;
		case 0xFF1C: 
			gbCore.apu.regs.NR32 = val; break;
		case 0xFF1D:
			gbCore.apu.regs.NR33 = val; break;
		case 0xFF1E: 
			gbCore.apu.regs.NR34 = val; break;
		case 0xFF20: 
			gbCore.apu.regs.NR41 = val; break;
		case 0xFF21:
			gbCore.apu.regs.NR42 = val; break;
		case 0xFF22: 
			gbCore.apu.regs.NR43 = val; break;
		case 0xFF23: 
			gbCore.apu.regs.NR44 = val; break;
		case 0xFF24: 
			gbCore.apu.regs.NR50 = val; break;
		case 0xFF25:
			gbCore.apu.regs.NR51 = val; break;
		case 0xFF26: 
			gbCore.apu.regs.NR52 = val; break;
		default:
			if (addr >= 0xFF30 && addr <= 0xFF3F)
				gbCore.apu.waveRAM[addr - 0xFF30] = val;
		}
	}
	else if (addr <= 0xFFFE)
	{
		HRAM[addr - 0xFF80] = val;
	}
	else
		gbCore.cpu.s.IE = val;
}

template uint8_t MMU::read8<true>(uint16_t) const;
template uint8_t MMU::read8<false>(uint16_t) const;

template <bool dmaBlocking>
uint8_t MMU::read8(uint16_t addr) const
{
	if constexpr (dmaBlocking)
	{
		if (dmaInProgress() && (addr < 0xFF80 || addr > 0xFFFE)) // during DMA only HRAM can be accessed. // TODO: DISABLE ON GBC!
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
		return gbCore.ppu.canAccessVRAM ? gbCore.ppu.VRAM[addr - 0x8000] : 0xFF;  
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
		return gbCore.ppu.canAccessOAM ? gbCore.ppu.OAM[addr - 0xFE00] : 0xFF;
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
			return gbCore.serial.s.serial_reg;
		case 0xFF02:
			return gbCore.serial.s.serial_control;
		case 0xFF04:
			return gbCore.cpu.s.DIV_reg;
		case 0xFF05:
			return gbCore.cpu.s.TIMA_reg;
		case 0xFF06:
			return gbCore.cpu.s.TMA_reg;
		case 0xFF07:
			return gbCore.cpu.s.TAC_reg;
		case 0xFF0F:
			return gbCore.cpu.s.IF;
		case 0xFF40:
			return gbCore.ppu.regs.LCDC;
		case 0xFF41:
			return gbCore.ppu.regs.STAT;
		case 0xFF42:
			return gbCore.ppu.regs.SCY;
		case 0xFF43:
			return gbCore.ppu.regs.SCX;
		case 0xFF44:
			return gbCore.ppu.s.LY;
		case 0xFF45:
			return gbCore.ppu.regs.LYC;
		case 0xFF46:
			return dma.reg;
		case 0xFF47:
			return gbCore.ppu.regs.BGP;
		case 0xFF48:
			return gbCore.ppu.regs.OBP0;
		case 0xFF49:
			return gbCore.ppu.regs.OBP1;
		case 0xFF4A:
			return gbCore.ppu.regs.WY;
		case 0xFF4B:
			return gbCore.ppu.regs.WX;

		case 0xFF10: 
			return gbCore.apu.regs.NR10;
		case 0xFF11: 
			return gbCore.apu.regs.NR11;
		case 0xFF12: 
			return gbCore.apu.regs.NR12;
		case 0xFF13: 
			return gbCore.apu.regs.NR13;
		case 0xFF14: 
			return gbCore.apu.regs.NR14;
		case 0xFF16: 
			return gbCore.apu.regs.NR21;
		case 0xFF17: 
			return gbCore.apu.regs.NR22;
		case 0xFF18: 
			return gbCore.apu.regs.NR23;
		case 0xFF19: 
			return gbCore.apu.regs.NR24;
		case 0xFF1A: 
			return gbCore.apu.regs.NR30;
		case 0xFF1B:
			return gbCore.apu.regs.NR31;
		case 0xFF1C: 
			return gbCore.apu.regs.NR32;
		case 0xFF1D: 
			return gbCore.apu.regs.NR33;
		case 0xFF1E: 
			return gbCore.apu.regs.NR34;
		case 0xFF20: 
			return gbCore.apu.regs.NR41;
		case 0xFF21: 
			return gbCore.apu.regs.NR42;
		case 0xFF22:
			return gbCore.apu.regs.NR43;
		case 0xFF23: 
			return gbCore.apu.regs.NR44;
		case 0xFF24: 
			return gbCore.apu.regs.NR50;
		case 0xFF25: 
			return gbCore.apu.regs.NR51;
		case 0xFF26:
			return gbCore.apu.regs.NR52;

		default:
			if (addr >= 0xFF30 && addr <= 0xFF3F)
				return gbCore.apu.waveRAM[addr - 0xFF30];

			return 0xFF;
		}
	}
	if (addr <= 0xFFFE)
	{
		return HRAM[addr - 0xFF80];
	}
	else
		return gbCore.cpu.s.IE;
}