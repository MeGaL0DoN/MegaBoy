#include "MMU.h"
#include "GBCore.h"
#include <iostream>

MMU::MMU(GBCore& gbCore) : gbCore(gbCore) {}

void MMU::saveState(std::ofstream& st)
{
	st.write(reinterpret_cast<char*>(&s), sizeof(s));

	uint16_t WRAMSize = System::Current() == GBSystem::GBC ? 0x8000 : 0x2000;
	st.write(reinterpret_cast<char*>(WRAM_BANKS.data()), WRAMSize);

	st.write(reinterpret_cast<char*>(HRAM.data()), sizeof(HRAM));
}

void MMU::loadState(std::ifstream& st)
{
	st.read(reinterpret_cast<char*>(&s), sizeof(s));

	uint16_t WRAMSize = System::Current() == GBSystem::GBC ? 0x8000 : 0x2000;
	st.read(reinterpret_cast<char*>(WRAM_BANKS.data()), WRAMSize);

	st.read(reinterpret_cast<char*>(HRAM.data()), sizeof(HRAM));
}

void MMU::startDMATransfer()
{
	if (s.dma.transfer)
	{
		s.dma.restartRequest = true;
		return;
	}

	s.dma.transfer = true;
	s.dma.cycles = 0;
	s.dma.sourceAddr = s.dma.reg * 0x100;

	if (s.dma.restartRequest)
	{
		s.dma.delayCycles = 1;
		s.dma.restartRequest = false;
	}
	else s.dma.delayCycles = 2;
}

void MMU::executeDMA()
{
	if (s.dma.transfer)
	{
		if (s.dma.delayCycles > 0)
			s.dma.delayCycles--;
		else
		{
			gbCore.ppu.OAM[s.dma.cycles++] = read8<false>(s.dma.sourceAddr++);

			if (s.dma.restartRequest)
			{
				s.dma.transfer = false;
				startDMATransfer();
			}
			else if (s.dma.cycles >= DMA_CYCLES)
				s.dma.transfer = false;
		}
	}
}

void MMU::executeGHDMA()
{
	s.hdma.cycles += (gbCore.cpu.doubleSpeed() ? 2 : 4);

	if (s.hdma.cycles >= GHDMA_BLOCK_CYCLES)
	{
		s.hdma.cycles -= GHDMA_BLOCK_CYCLES;
		s.hdma.transferLength -= 0x10;

		for (int i = 0; i < 0x10; i++)
			gbCore.ppu.VRAM[s.hdma.currentDestAddr++] = read8<false>(s.hdma.currentSourceAddr++);

		if (s.hdma.transferLength == 0)
		{
			s.hdma.status = GHDMAStatus::None;
			s.hdma.transferLength = 0x7F;
		}
	}
}

void MMU::write8(uint16_t addr, uint8_t val)
{
	if (addr == 0xFF46)
	{
		s.dma.reg = val;
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
	else if (addr <= 0xCFFF)
	{
		WRAM_BANKS[addr - 0xC000] = val;
	}
	else if (addr <= 0xDFFF)
	{
		WRAM_BANKS[0x1000 * s.wramBank + addr - 0xD000] = val;
	}
	else if (addr <= 0xFDFF)
	{
		WRAM_BANKS[addr - 0xE000] = val;
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
			s.newStatVal = (gbCore.ppu.regs.STAT & 0x87) | (val & 0xF8);
			gbCore.ppu.regs.STAT = 0xFF;
			s.statRegChanged = true;
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

		case 0xFF50: // BANK register used by boot ROMs
			gbCore.cpu.executingBootROM = false;
			break;

		case 0xFF4F:
			if (System::Current() == GBSystem::GBC)
				gbCore.ppu.setVRAMBank(val);
			break;
		case 0xFF68:
			gbCore.ppu.gbcRegs.BCPS.write(val);
			break;
		case 0xFF69:
			gbCore.ppu.writePaletteRAM(gbCore.ppu.BGpaletteRAM, gbCore.ppu.gbcRegs.BCPS, val);
			break;
		case 0xFF6A:
			gbCore.ppu.gbcRegs.OCPS.write(val);
			break;
		case 0xFF6B:
			gbCore.ppu.writePaletteRAM(gbCore.ppu.OBPpaletteRAM, gbCore.ppu.gbcRegs.OCPS, val);
			break;
		case 0xFF70:
			if (System::Current() == GBSystem::GBC)
			{
				s.wramBank = val & 0x7;
				if (s.wramBank == 0) s.wramBank = 1;
			}
			break;

		case 0xFF4D:
			if (System::Current() == GBSystem::GBC)
			{
				if (gbCore.cpu.s.GBCdoubleSpeed != (val & 1))
					gbCore.cpu.s.prepareSpeedSwitch = true;
			}
			break;

		case 0xFF51:
			s.hdma.sourceAddr = (s.hdma.sourceAddr & 0x00FF) | (val << 8);
			break;
		case 0xFF52:
			s.hdma.sourceAddr = (s.hdma.sourceAddr & 0xFF00) | val; 
			break;
		case 0xFF53:
			s.hdma.destAddr = (s.hdma.destAddr & 0x00FF) | (val << 8);
			break;
		case 0xFF54:
			s.hdma.destAddr = (s.hdma.destAddr & 0xFF00) | val;
			break;
		case 0xFF55:
			if (System::Current() == GBSystem::GBC)
			{
				if (s.hdma.status != GHDMAStatus::None)
				{
					if (!getBit(val, 7))
						s.hdma.status = GHDMAStatus::None;
				}
				else
				{
					s.hdma.currentSourceAddr = s.hdma.sourceAddr & 0xFFF0;
					s.hdma.currentDestAddr = s.hdma.destAddr & 0x1FF0; // ignore 3 upper bits too, because destination is always in VRAM

					s.hdma.transferLength = ((val & 0x7F) + 1) * 0x10;
					s.hdma.status = getBit(val, 7) ? GHDMAStatus::HDMA : GHDMAStatus::GDMA;
					s.hdma.cycles = 0;
				}				
			}
			break;

		//case 0xFF51:
		//case 0xFF52:
		//case 0xFF53:
		//case 0xFF54:
		//case 0xFF55:
		//	std::cout << "HDMA WRITE! \n";
		//	break;

		case 0xFF10: 
			gbCore.apu.regs.NR10 = val; break;
		case 0xFF11:
			gbCore.apu.channel1.regs.NRx1 = val; 
			gbCore.apu.channel1.updateLengthTimer();
			break;
		case 0xFF12: 
			gbCore.apu.channel1.regs.NRx2 = val; 
			break;
		case 0xFF13:
			gbCore.apu.channel1.regs.NRx3 = val; 
			break;
		case 0xFF14: 
			gbCore.apu.channel1.regs.NRx4 = val; 
			if (getBit(val, 7))
			{
				gbCore.apu.channel1.trigger();
				gbCore.apu.triggerSweep();
			}
			break;
		case 0xFF16: 
			gbCore.apu.channel2.regs.NRx1 = val; 
			gbCore.apu.channel2.updateLengthTimer();
			break;
		case 0xFF17: 
			gbCore.apu.channel2.regs.NRx2 = val; 
			break;
		case 0xFF18: 
			gbCore.apu.channel2.regs.NRx3 = val;
			break;
		case 0xFF19: 
			gbCore.apu.channel2.regs.NRx4 = val; 
			if (getBit(val, 7)) gbCore.apu.channel2.trigger();
			break;
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
	if (addr == 0xFF46)
		return s.dma.reg;

	if constexpr (dmaBlocking)
	{
		if (dmaInProgress() && (addr < 0xFF80 || addr > 0xFFFE)) // during DMA only HRAM can be accessed. // TODO: DISABLE ON GBC!
			return 0xFF;
	}

	if (gbCore.cpu.executingBootROM)
	{
		if (addr < 0x100)
			return base_bootROM[addr];
		else if (System::Current() == GBSystem::GBC && (addr >= 0x200 && addr <= 0x8FF))
			return GBCbootROM[addr - 0x200];
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
	if (addr <= 0xCFFF)
	{
		return WRAM_BANKS[addr - 0xC000];
	}
	if (addr <= 0xDFFF)
	{
		return WRAM_BANKS[s.wramBank * 0x1000 + addr - 0xD000];
	}
	if (addr <= 0xFDFF)
	{
		return WRAM_BANKS[addr - 0xE000];
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

		// TODO : CONSTEXPR GBC MODE
		case 0xFF4F:
			return System::Current() == GBSystem::GBC ? gbCore.ppu.gbcRegs.VBK : 0xFF;
		case 0xFF68:
			return System::Current() == GBSystem::GBC ? gbCore.ppu.gbcRegs.BCPS.read() : 0xFF;
		case 0xFF69:
			return System::Current() == GBSystem::GBC ? gbCore.ppu.BGpaletteRAM[gbCore.ppu.gbcRegs.BCPS.value] : 0xFF;
		case 0xFF6A:
			return System::Current() == GBSystem::GBC ? gbCore.ppu.gbcRegs.OCPS.read() : 0xFF;
		case 0xFF6B:
			return System::Current() == GBSystem::GBC ? gbCore.ppu.OBPpaletteRAM[gbCore.ppu.gbcRegs.OCPS.value] : 0xFF;
		case 0xFF70:
			return System::Current() == GBSystem::GBC ? s.wramBank : 0xFF;

		case 0xFF4D:
			return System::Current() == GBSystem::GBC ? (0x7E | (gbCore.cpu.s.GBCdoubleSpeed << 7) | gbCore.cpu.s.prepareSpeedSwitch) : 0xFF;

		case 0xFF51:
			return System::Current() == GBSystem::GBC ? s.hdma.sourceAddr >> 8 : 0xFF;
		case 0xFF52:
			return System::Current() == GBSystem::GBC ? s.hdma.sourceAddr & 0xFF : 0xFF;
		case 0xFF53:
			return System::Current() == GBSystem::GBC ? s.hdma.destAddr >> 8 : 0xFF;
		case 0xFF54:
			return System::Current() == GBSystem::GBC ? s.hdma.destAddr & 0xFF : 0xFF;
		case 0xFF55:
			return System::Current() == GBSystem::GBC ? (((s.hdma.status == GHDMAStatus::None) << 7) | ((s.hdma.transferLength - 1) / 0x10)) : 0xFF;

		case 0xFF10: 
			return gbCore.apu.regs.NR10;
		case 0xFF11: 
			return gbCore.apu.channel1.regs.NRx1;
		case 0xFF12: 
			return gbCore.apu.channel1.regs.NRx2;
		case 0xFF13: 
			return gbCore.apu.channel1.regs.NRx3;
		case 0xFF14: 
			return gbCore.apu.channel1.regs.NRx4;
		case 0xFF16: 
			return gbCore.apu.channel2.regs.NRx1;
		case 0xFF17: 
			return gbCore.apu.channel2.regs.NRx2;
		case 0xFF18: 
			return gbCore.apu.channel2.regs.NRx3;
		case 0xFF19: 
			return gbCore.apu.channel2.regs.NRx4;
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