#include "MMU.h"
#include "GBCore.h"
#include "defines.h"

MMU::MMU(GBCore& gbCore) : gbCore(gbCore) {}

void MMU::updateFunctionPointers()
{
	if (System::Current() == GBSystem::DMG)
	{
		read_func = &MMU::read8<GBSystem::DMG, true>;
		write_func = &MMU::write8<GBSystem::DMG>;
		dma_nonblocking_read = &MMU::read8<GBSystem::DMG, false>;
	}
	else if (System::Current() == GBSystem::GBC)
	{
		read_func = &MMU::read8<GBSystem::GBC, false>;
		write_func = &MMU::write8<GBSystem::GBC>;
		dma_nonblocking_read = read_func;
	}
}

void MMU::saveState(std::ofstream& st) const
{
	ST_WRITE(s);

	if (System::Current() == GBSystem::GBC)
		ST_WRITE(gbc);

	const uint16_t WRAMSize = System::Current() == GBSystem::GBC ? 0x8000 : 0x2000;

	st.write(reinterpret_cast<const char*>(WRAM_BANKS.data()), WRAMSize);
	ST_WRITE_ARR(HRAM);
}

void MMU::loadState(std::ifstream& st)
{
	ST_READ(s);

	if (System::Current() == GBSystem::GBC)
		ST_READ(gbc);

	uint16_t WRAMSize = System::Current() == GBSystem::GBC ? 0x8000 : 0x2000;

	st.read(reinterpret_cast<char*>(WRAM_BANKS.data()), WRAMSize);
	ST_READ_ARR(HRAM);
}

void MMU::execute()
{
	if (gbc.ghdma.active) [[unlikely]]
		executeGHDMA();
	else if (s.dma.transfer) [[unlikely]]
		executeDMA();

	if (s.statRegChanged) [[unlikely]]
	{
		gbCore.ppu->regs.STAT = s.newStatVal;
		s.statRegChanged = false;
	}
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
	if (s.dma.delayCycles > 0)
		s.dma.delayCycles--;
	else
	{
		gbCore.ppu->OAM[s.dma.cycles++] = (this->*dma_nonblocking_read)(s.dma.sourceAddr++);

		if (s.dma.restartRequest)
		{
			s.dma.transfer = false;
			startDMATransfer();
		}
		else if (s.dma.cycles >= DMA_CYCLES)
			s.dma.transfer = false;
	}
}

void MMU::executeGHDMA()
{
	gbc.ghdma.cycles += gbCore.cpu.TcyclesPerM();

	if (gbc.ghdma.cycles >= GHDMA_BLOCK_CYCLES)
	{
		gbc.ghdma.cycles -= GHDMA_BLOCK_CYCLES;
		gbc.ghdma.transferLength -= 0x10;

		for (int i = 0; i < 0x10; i++)
			gbCore.ppu->VRAM[gbc.ghdma.currentDestAddr++] = (this->*read_func)(gbc.ghdma.currentSourceAddr++);

		if (gbc.ghdma.transferLength == 0)
		{
			gbc.ghdma.status = GHDMAStatus::None;
			gbc.ghdma.active = false;
		}
	}
}

template void MMU::write8<GBSystem::DMG>(uint16_t, uint8_t);
template void MMU::write8<GBSystem::GBC>(uint16_t, uint8_t);

template <GBSystem sys>
void MMU::write8(uint16_t addr, uint8_t val)
{
	if constexpr (sys == GBSystem::DMG)
	{
		if (addr == 0xFF46)
		{
			s.dma.reg = val;
			startDMATransfer();
			return;
		}
		else if (dmaInProgress() && (addr < 0xFF80 || addr > 0xFFFE)) // during DMA only HRAM can be accessed. (only on DMG)
			return;
	}

	if (addr <= 0x7FFF)
	{
		gbCore.cartridge.getMapper()->write(addr, val);
	}
	else if (addr <= 0x9FFF)
	{
		if (gbCore.ppu->canAccessVRAM)
			gbCore.ppu->VRAM[addr - 0x8000] = val;
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
		if constexpr (sys == GBSystem::GBC)
			WRAM_BANKS[gbc.wramBank * 0x1000 + addr - 0xD000] = val;
		else
			WRAM_BANKS[addr - 0xC000] = val;
	}
	else if (addr <= 0xFDFF)
	{
		WRAM_BANKS[addr - 0xE000] = val;
	}
	else if (addr <= 0xFE9F)
	{
		if (gbCore.ppu->canAccessOAM)
			gbCore.ppu->OAM[addr - 0xFE00] = val;
	}
	else if (addr <= 0xFEFF)
	{
		// prohibited area
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
			gbCore.ppu->setLCDEnable(getBit(val, 7));
			gbCore.ppu->regs.LCDC = val;
			break;
		case 0xFF41:
		{
			const uint8_t maskedSTAT = (gbCore.ppu->regs.STAT & 0x87) | (val & 0xF8);

			// spurious STAT interrupts (DMG only)
			if constexpr (sys == GBSystem::DMG)
			{
				if (gbCore.ppu->s.state == PPUMode::VBlank || gbCore.ppu->s.LY == gbCore.ppu->regs.LYC)
				{
					s.newStatVal = maskedSTAT;
					gbCore.ppu->regs.STAT = 0xFF;
					s.statRegChanged = true;
					break;
				}
			}

			gbCore.ppu->regs.STAT = maskedSTAT;
			break;
		}
		case 0xFF42:
			gbCore.ppu->regs.SCY = val;
			break;
		case 0xFF43:
			gbCore.ppu->regs.SCX = val;
			break;
		case 0xFF44:
			// read only LY register
			break;
		case 0xFF45:
			gbCore.ppu->regs.LYC = val;
			break;
		case 0xFF46:
			if constexpr (sys == GBSystem::GBC)
			{
				s.dma.reg = val;
				startDMATransfer();
			}
			break;
		case 0xFF47:
			gbCore.ppu->regs.BGP = val;
			PPU::updatePalette(val, gbCore.ppu->BGpalette);
			break;
		case 0xFF48:
			gbCore.ppu->regs.OBP0 = val;
			PPU::updatePalette(val, gbCore.ppu->OBP0palette);
			break;
		case 0xFF49:
			gbCore.ppu->regs.OBP1 = val;
			PPU::updatePalette(val, gbCore.ppu->OBP1palette);
			break;
		case 0xFF4A:
			gbCore.ppu->regs.WY = val;
			break;
		case 0xFF4B:
			gbCore.ppu->regs.WX = val;
			break;
		case 0xFF50: // BANK register used by boot ROMs
			gbCore.cpu.executingBootROM = false;
			break;

		case 0xFF4D:
			if constexpr (sys == GBSystem::GBC)
			{
				if (gbCore.cpu.s.GBCdoubleSpeed != (val & 1))
					gbCore.cpu.s.prepareSpeedSwitch = true;
			}
			break;
		case 0xFF4F:
			if constexpr (sys == GBSystem::GBC)
				gbCore.ppu->setVRAMBank(val);
			break;
		case 0xFF68:
			if constexpr (sys == GBSystem::GBC)
				gbCore.ppu->gbcRegs.BCPS.writeReg(val);
			break;
		case 0xFF69:
			if constexpr (sys == GBSystem::GBC)
				gbCore.ppu->gbcRegs.BCPS.writePaletteRAM(val);
			break;
		case 0xFF6A:
			if constexpr (sys == GBSystem::GBC)
				gbCore.ppu->gbcRegs.OCPS.writeReg(val);
			break;
		case 0xFF6B:
			if constexpr (sys == GBSystem::GBC)
				gbCore.ppu->gbcRegs.OCPS.writePaletteRAM(val);
			break;
		case 0xFF70:
			if constexpr (sys == GBSystem::GBC)
			{
				gbc.wramBank = val & 0x7;
				if (gbc.wramBank == 0) gbc.wramBank = 1;
			}
			break;
		case 0xFF51:
			if constexpr (sys == GBSystem::GBC)
				gbc.ghdma.sourceAddr = (gbc.ghdma.sourceAddr & 0x00FF) | (val << 8);
			break;
		case 0xFF52:
			if constexpr (sys == GBSystem::GBC)
				gbc.ghdma.sourceAddr = (gbc.ghdma.sourceAddr & 0xFF00) | val;
			break;
		case 0xFF53:
			if constexpr (sys == GBSystem::GBC)
				gbc.ghdma.destAddr = (gbc.ghdma.destAddr & 0x00FF) | (val << 8);
			break;
		case 0xFF54:
			if constexpr (sys == GBSystem::GBC)
				gbc.ghdma.destAddr = (gbc.ghdma.destAddr & 0xFF00) | val;
			break;
		case 0xFF55:
			if constexpr (sys == GBSystem::GBC)
			{
				if (gbc.ghdma.status != GHDMAStatus::None)
				{
					if (!getBit(val, 7))
						gbc.ghdma.status = GHDMAStatus::None;
				}
				else
				{
					gbc.ghdma.currentSourceAddr = gbc.ghdma.sourceAddr & 0xFFF0;
					gbc.ghdma.currentDestAddr = gbc.ghdma.destAddr & 0x1FF0; // ignore 3 upper bits too, because destination is always in VRAM

					gbc.ghdma.transferLength = ((val & 0x7F) + 1) * 0x10;
					gbc.ghdma.cycles = 0;

					if (getBit(val, 7))
					{
						gbc.ghdma.status = GHDMAStatus::HDMA;
						gbc.ghdma.active = false;
					}
					else
					{
						gbc.ghdma.status = GHDMAStatus::GDMA;
						gbc.ghdma.active = true;
					}
				}
			}
			break;
		case 0xFF72:
			if constexpr (sys == GBSystem::GBC)
				gbc.FF72 = val;
			break;
		case 0xFF73:
			if constexpr (sys == GBSystem::GBC)
				gbc.FF73 = val;
			break;
		case 0xFF74:
			if constexpr (sys == GBSystem::GBC)
				gbc.FF74 = val;
			break;
		case 0xFF75:
			if constexpr (sys == GBSystem::GBC)
				gbc.FF75 = val | 0x8F;
			break;

		case 0xFF10: 
			gbCore.apu.channel1.NR10 = val; 
			break;
		case 0xFF11:
			gbCore.apu.channel1.NRx1 = val; 
			break;
		case 0xFF12: 
			gbCore.apu.channel1.NRx2 = val; 
			break;
		case 0xFF13:
			gbCore.apu.channel1.NRx3 = val; 
			break;
		case 0xFF14: 
			gbCore.apu.channel1.NRx4 = val; 
			if (getBit(val, 7)) gbCore.apu.channel1.trigger();
			break;
		case 0xFF16: 
			gbCore.apu.channel2.NRx1 = val; 
			break;
		case 0xFF17:
			gbCore.apu.channel2.NRx2 = val; 
			break;
		case 0xFF18: 
			gbCore.apu.channel2.NRx3 = val;
			break;
		case 0xFF19: 
			gbCore.apu.channel2.NRx4 = val; 
			if (getBit(val, 7)) gbCore.apu.channel2.trigger();
			break;

		case 0xFF24: ////////
			gbCore.apu.NR50 = val; 
			break;

		//default:
		//	if (addr >= 0xFF30 && addr <= 0xFF3F)
		//		gbCore.apu.waveRAM[addr - 0xFF30] = val;
		}
	}
	else if (addr <= 0xFFFE)
	{
		HRAM[addr - 0xFF80] = val;
	}
	else
		gbCore.cpu.s.IE = val;
}

template uint8_t MMU::read8<GBSystem::DMG, true>(uint16_t) const;
template uint8_t MMU::read8<GBSystem::DMG, false>(uint16_t) const;

template uint8_t MMU::read8<GBSystem::GBC, true>(uint16_t) const;
template uint8_t MMU::read8<GBSystem::GBC, false>(uint16_t) const;

template <GBSystem sys, bool dmaBlocking>
uint8_t MMU::read8(uint16_t addr) const
{
	if constexpr (dmaBlocking)
	{
		if (addr == 0xFF46)
			return s.dma.reg;

		if (dmaInProgress() && (addr < 0xFF80 || addr > 0xFFFE))
			return 0xFF;
	}

	if (gbCore.cpu.executingBootROM)
	{
		if (addr < 0x100)
			return base_bootROM[addr];

		if constexpr (sys == GBSystem::GBC)
		{
			if (addr >= 0x200 && addr <= 0x8FF)
				return GBCbootROM[addr - 0x200];
		}
	}

	if (addr <= 0x7FFF)
	{
		return gbCore.cartridge.getMapper()->read(addr);
	}
	if (addr <= 0x9FFF)
	{
		return gbCore.ppu->canAccessVRAM ? gbCore.ppu->VRAM[addr - 0x8000] : 0xFF;
	}
	if (addr <= 0xBFFF)
	{
		return gbCore.cartridge.getMapper()->read(addr);
	}
	if constexpr (sys == GBSystem::DMG)
	{
		if (addr <= 0xDFFF)
			return WRAM_BANKS[addr - 0xC000];
	}
	else
	{
		if (addr <= 0xCFFF)
			return WRAM_BANKS[addr - 0xC000];
		if (addr <= 0xDFFF)
			return WRAM_BANKS[gbc.wramBank * 0x1000 + addr - 0xD000];
	}
	if (addr <= 0xFDFF)
	{
		return WRAM_BANKS[addr - 0xE000];
	}
	if (addr <= 0xFE9F)
	{
		return gbCore.ppu->canAccessOAM ? gbCore.ppu->OAM[addr - 0xFE00] : 0xFF;
	}
	if (addr <= 0xFEFF)
	{
		// prohibited area

		if constexpr (sys == GBSystem::DMG)
			return gbCore.ppu->canAccessOAM ? 0x00 : 0xFF;
		else
			return 0xFF;
	}
	if (addr <= 0xFF7F)
	{
		switch (addr)
		{
		case 0xFF00:
			return gbCore.input.readJoypadReg();
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
			return gbCore.ppu->regs.LCDC;
		case 0xFF41:
			return gbCore.ppu->regs.STAT;
		case 0xFF42:
			return gbCore.ppu->regs.SCY;
		case 0xFF43:
			return gbCore.ppu->regs.SCX;
		case 0xFF44:
			return gbCore.ppu->s.LY;
		case 0xFF45:
			return gbCore.ppu->regs.LYC;
		case 0xFF46:
			return s.dma.reg;
		case 0xFF47:
			return gbCore.ppu->regs.BGP;
		case 0xFF48:
			return gbCore.ppu->regs.OBP0;
		case 0xFF49:
			return gbCore.ppu->regs.OBP1;
		case 0xFF4A:
			return gbCore.ppu->regs.WY;
		case 0xFF4B:
			return gbCore.ppu->regs.WX;

		case 0xFF4D:
			if constexpr (sys == GBSystem::GBC)
				return 0x7E | (static_cast<uint8_t>(gbCore.cpu.s.GBCdoubleSpeed) << 7) | static_cast<uint8_t>(gbCore.cpu.s.prepareSpeedSwitch);
			else
				return 0xFF;
		case 0xFF4F:
			if constexpr (sys == GBSystem::GBC)
				return gbCore.ppu->gbcRegs.VBK;
			else 
				return 0xFF;
		case 0xFF68:
			if constexpr (sys == GBSystem::GBC)
				return gbCore.ppu->gbcRegs.BCPS.readReg();
			else
				return 0xFF;
		case 0xFF69:
			if constexpr (sys == GBSystem::GBC)
				return gbCore.ppu->gbcRegs.BCPS.readPaletteRAM();
			else
				return 0xFF;
		case 0xFF6A:
			if constexpr (sys == GBSystem::GBC)
				return gbCore.ppu->gbcRegs.OCPS.readReg();
			else
				return 0xFF;
		case 0xFF6B:
			if constexpr (sys == GBSystem::GBC)
				return gbCore.ppu->gbcRegs.OCPS.readPaletteRAM();
			else
				return 0xFF;
		case 0xFF70:
			if constexpr (sys == GBSystem::GBC)
				return gbc.wramBank;
			else
				return 0xFF;
		case 0xFF51:
			if constexpr (sys == GBSystem::GBC)
				return gbc.ghdma.sourceAddr >> 8;
			else
				return 0xFF;
		case 0xFF52:
			if constexpr (sys == GBSystem::GBC)
				return gbc.ghdma.sourceAddr & 0xFF;
			else
				return 0xFF;
		case 0xFF53:
			if constexpr (sys == GBSystem::GBC)
				return gbc.ghdma.destAddr >> 8;
			else
				return 0xFF;
		case 0xFF54:
			if constexpr (sys == GBSystem::GBC)
				return gbc.ghdma.destAddr & 0xFF;
			else
				return 0xFF;
		case 0xFF55:
			if constexpr (sys == GBSystem::GBC)
			{
				const uint8_t lengthVal = (gbc.ghdma.transferLength - 1) / 0x10;
				return ((gbc.ghdma.status == GHDMAStatus::None) << 7) | lengthVal;
			}
			else
				return 0xFF;
		case 0xFF6C: // OPRI, bit 0 is always off in cgb mode?
			if constexpr (sys == GBSystem::GBC)
				return 0xFE;
			else
				return 0xFF;
		case 0xFF72:
			if constexpr (sys == GBSystem::GBC)
				return gbc.FF72;
			else
				return 0xFF;
		case 0xFF73:
			if constexpr (sys == GBSystem::GBC)
				return gbc.FF73;
			else
				return 0xFF;
		case 0xFF74:
			if constexpr (sys == GBSystem::GBC)
				return gbc.FF74;
			else
				return 0xFF;
		case 0xFF75:
			if constexpr (sys == GBSystem::GBC)
				return gbc.FF75;
			else
				return 0xFF;

		case 0xFF10: 
			return gbCore.apu.channel1.NR10;
		case 0xFF11: 
			return gbCore.apu.channel1.NRx1;
		case 0xFF12: 
			return gbCore.apu.channel1.NRx2;
		case 0xFF13: 
			return gbCore.apu.channel1.NRx3;
		case 0xFF14: 
			return gbCore.apu.channel1.NRx4;
		case 0xFF16: 
			return gbCore.apu.channel2.NRx1;
		case 0xFF17: 
			return gbCore.apu.channel2.NRx2;
		case 0xFF18: 
			return gbCore.apu.channel2.NRx3;
		case 0xFF19: 
			return gbCore.apu.channel2.NRx4;

			
		//case 0xFF1A: 
		//	return gbCore.apu.regs.NR30;
		//case 0xFF1B:
		//	return gbCore.apu.regs.NR31;
		//case 0xFF1C: 
		//	return gbCore.apu.regs.NR32;
		//case 0xFF1D: 
		//	return gbCore.apu.regs.NR33;
		//case 0xFF1E: 
		//	return gbCore.apu.regs.NR34;
		//case 0xFF20: 
		//	return gbCore.apu.regs.NR41;
		//case 0xFF21: 
		//	return gbCore.apu.regs.NR42;
		//case 0xFF22:
		//	return gbCore.apu.regs.NR43;
		//case 0xFF23: 
		//	return gbCore.apu.regs.NR44;
		case 0xFF24: 
			return gbCore.apu.NR50;// gbCore.apu.regs.NR50;
		//case 0xFF25: 
		//	return gbCore.apu.regs.NR51;
		//case 0xFF26:
		//	return gbCore.apu.regs.NR52;

		default:
			//if (addr >= 0xFF30 && addr <= 0xFF3F)
			//	return gbCore.apu.waveRAM[addr - 0xFF30];

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