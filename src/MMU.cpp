#include "MMU.h"
#include "GBCore.h"
#include "defines.h"

MMU::MMU(GBCore& gb) : gb(gb) {}

void MMU::updateFunctionPointers()
{
	if (System::Current() == GBSystem::DMG)
	{
		read_func = &MMU::read8<GBSystem::DMG>;
		write_func = &MMU::write8<GBSystem::DMG>;
	}
	else if (System::Current() == GBSystem::GBC)
	{
		read_func = &MMU::read8<GBSystem::GBC>;
		write_func = &MMU::write8<GBSystem::GBC>;
	}
}

void MMU::saveState(std::ostream& st) const
{
	ST_WRITE(s);

	if (System::Current() == GBSystem::GBC)
		ST_WRITE(gbc);

	const uint16_t WRAMSize = System::Current() == GBSystem::GBC ? 0x8000 : 0x2000;

	st.write(reinterpret_cast<const char*>(WRAM_BANKS.data()), WRAMSize);
	ST_WRITE_ARR(HRAM);
}

void MMU::loadState(std::istream& st)
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
	if (gbc.ghdma.active && !gb.cpu.s.halted) [[unlikely]]
		executeGHDMA();
	else if (s.dma.transfer) [[unlikely]]
		executeDMA();

	if (s.statRegChanged) [[unlikely]]
	{
		gb.ppu->regs.STAT = s.newStatVal;
		s.statRegChanged = false;
	}
}

void MMU::startDMATransfer()
{
	if (s.dma.transfer)
	{
		s.dma.restartRequest = true;
		s.dma.delayCycles = 1;
		return;
	}

	s.dma.transfer = true;
	s.dma.cycles = 0;
	s.dma.sourceAddr = s.dma.reg * 0x100;

	if (s.dma.restartRequest)
	{
		s.dma.delayCycles = 0;
		s.dma.restartRequest = false;
	}
	else
		s.dma.delayCycles = 2;
}

void MMU::executeDMA()
{
	if (s.dma.delayCycles > 0 && !s.dma.restartRequest)
		s.dma.delayCycles--;
	else
	{
		gb.ppu->OAM[s.dma.cycles++] = (this->*read_func)(s.dma.sourceAddr++);

		if (s.dma.restartRequest && s.dma.delayCycles-- == 0)
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
	gbc.ghdma.cycles += gb.cpu.TcyclesPerM();

	if (gbc.ghdma.cycles >= GHDMA_BLOCK_CYCLES)
	{
		gbc.ghdma.cycles -= GHDMA_BLOCK_CYCLES;
		gbc.ghdma.transferLength -= 0x10;

		for (int i = 0; i < 0x10; i++)
			gb.ppu->VRAM[gbc.ghdma.currentDestAddr++] = (this->*read_func)(gbc.ghdma.currentSourceAddr++);

		if (gbc.ghdma.transferLength == 0)
		{
			gbc.ghdma.status = GHDMAStatus::None;
			gbc.ghdma.active = false;
		}
		else if (gbc.ghdma.status == GHDMAStatus::HDMA)
			gbc.ghdma.active = false;
	}
}

template void MMU::write8<GBSystem::DMG>(uint16_t, uint8_t);
template void MMU::write8<GBSystem::GBC>(uint16_t, uint8_t);

template <GBSystem sys>
void MMU::write8(uint16_t addr, uint8_t val)
{
	if (addr <= 0x7FFF)
	{
		gb.cartridge.getMapper()->write(addr, val);
	}
	else if (addr <= 0x9FFF)
	{
		gb.ppu->VRAM[addr - 0x8000] = val;
	}
	else if (addr <= 0xBFFF)
	{
		gb.cartridge.getMapper()->write(addr, val);
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
		if (gb.ppu->canAccessOAM && !dmaInProgress())
			gb.ppu->OAM[addr - 0xFE00] = val;
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
			gb.joypad.writeInputReg(val);
			break;
		case 0xFF01:
			gb.serial.s.serial_reg = val;
			break;
		case 0xFF02:
			gb.serial.writeSerialControl(val);
			break;
		case 0xFF04:
			gb.cpu.s.DIV_reg = 0;
			gb.cpu.s.DIV_COUNTER = 0;
			break;
		case 0xFF05:
			gb.cpu.s.TIMA_reg = val;
			break;
		case 0xFF06:
			gb.cpu.s.TMA_reg = val;
			break;
		case 0xFF07:
			gb.cpu.s.TAC_reg = val;
			break;
		case 0xFF0F:
			gb.cpu.s.IF = val | 0xE0;
			break;
		case 0xFF40:
			gb.ppu->setLCDEnable(getBit(val, 7));
			gb.ppu->regs.LCDC = val;
			break;
		case 0xFF41:
		{
			const uint8_t maskedSTAT = (gb.ppu->regs.STAT & 0x87) | (val & 0xF8);

			// spurious STAT interrupts (DMG only)
			if constexpr (sys == GBSystem::DMG)
			{
				if (gb.ppu->s.state == PPUMode::VBlank || gb.ppu->s.LY == gb.ppu->regs.LYC)
				{
					s.newStatVal = maskedSTAT;
					gb.ppu->regs.STAT = 0xFF;
					s.statRegChanged = true;
					break;
				}
			}

			gb.ppu->regs.STAT = maskedSTAT;
			break;
		}
		case 0xFF42:
			gb.ppu->regs.SCY = val;
			break;
		case 0xFF43:
			gb.ppu->regs.SCX = val;
			break;
		case 0xFF44:
			// read only LY register
			break;
		case 0xFF45:
			gb.ppu->regs.LYC = val;
			break;
		case 0xFF46:
			s.dma.reg = val;
			startDMATransfer();
			break;
		case 0xFF47:
			gb.ppu->regs.BGP = val;
			PPU::updatePalette(val, gb.ppu->BGpalette);
			break;
		case 0xFF48:
			gb.ppu->regs.OBP0 = val;
			PPU::updatePalette(val, gb.ppu->OBP0palette);
			break;
		case 0xFF49:
			gb.ppu->regs.OBP1 = val;
			PPU::updatePalette(val, gb.ppu->OBP1palette);
			break;
		case 0xFF4A:
			gb.ppu->regs.WY = val;
			break;
		case 0xFF4B:
			gb.ppu->regs.WX = val;
			break;
		case 0xFF50: // BANK register used by boot ROMs
			gb.cpu.executingBootROM = false;
			break;

		case 0xFF4D:
			if constexpr (sys == GBSystem::GBC)
			{
				if (gb.cpu.s.GBCdoubleSpeed != (val & 1))
					gb.cpu.s.prepareSpeedSwitch = true;
			}
			break;
		case 0xFF4F:
			if constexpr (sys == GBSystem::GBC)
				gb.ppu->setVRAMBank(val);
			break;
		case 0xFF68:
			if constexpr (sys == GBSystem::GBC)
				gb.ppu->gbcRegs.BCPS.writeReg(val);
			break;
		case 0xFF69:
			if constexpr (sys == GBSystem::GBC)
				gb.ppu->gbcRegs.BCPS.writePaletteRAM(val);
			break;
		case 0xFF6A:
			if constexpr (sys == GBSystem::GBC)
				gb.ppu->gbcRegs.OCPS.writeReg(val);
			break;
		case 0xFF6B:
			if constexpr (sys == GBSystem::GBC)
				gb.ppu->gbcRegs.OCPS.writePaletteRAM(val);
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
						gbc.ghdma.active = gb.ppu->s.state == PPUMode::HBlank;
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
			gb.apu.channel1.regs.NR10 = val; 
			break;
		case 0xFF11:
			gb.apu.channel1.regs.NRx1 = val; 
			gb.apu.channel1.reloadLength();
			break;
		case 0xFF12: 
			gb.apu.channel1.regs.NRx2 = val; 
			if (!gb.apu.channel1.dacEnabled()) gb.apu.channel1.disable();
			break;
		case 0xFF13:
			gb.apu.channel1.regs.NRx3 = val; 
			break;
		case 0xFF14: 
			gb.apu.channel1.regs.NRx4 = val; 
			if (getBit(val, 7)) gb.apu.channel1.trigger();
			break;
		case 0xFF16: 
			gb.apu.channel2.regs.NRx1 = val; 
			gb.apu.channel2.reloadLength();
			break;
		case 0xFF17:
			gb.apu.channel2.regs.NRx2 = val; 
			if (!gb.apu.channel2.dacEnabled()) gb.apu.channel2.disable();
			break;
		case 0xFF18: 
			gb.apu.channel2.regs.NRx3 = val;
			break;
		case 0xFF19: 
			gb.apu.channel2.regs.NRx4 = val; 
			if (getBit(val, 7)) gb.apu.channel2.trigger();
			break;
		case 0xFF1A:
			gb.apu.channel3.regs.NR30 = val;
			if (!gb.apu.channel3.dacEnabled()) gb.apu.channel3.disable();
			break;
		case 0xFF1B:
			gb.apu.channel3.regs.NR31 = val;
			gb.apu.channel3.reloadLength();
			break;
		case 0xFF1C:
			gb.apu.channel3.regs.NR32 = val;
			break;
		case 0xFF1D:
			gb.apu.channel3.regs.NR33 = val;
			break;
		case 0xFF1E:
			gb.apu.channel3.regs.NR34 = val;
			if (getBit(val, 7)) gb.apu.channel3.trigger();
			break;
		case 0xFF20:
			gb.apu.channel4.regs.NR41 = val;
			gb.apu.channel4.reloadLength();
			break;
		case 0xFF21:
			gb.apu.channel4.regs.NR42 = val;
			if (!gb.apu.channel4.dacEnabled()) gb.apu.channel4.disable();
			break;
		case 0xFF22:
			gb.apu.channel4.regs.NR43 = val;
			break;
		case 0xFF23:
			gb.apu.channel4.regs.NR44 = val;
			if (getBit(val, 7)) gb.apu.channel4.trigger();
			break;
		case 0xFF24: 
			gb.apu.regs.NR50 = val; 
			break;
		case 0xFF25:
			gb.apu.regs.NR51 = val;
			break;
		case 0xFF26:
		{
			const bool apuEnable = getBit(val, 7);
			if (!apuEnable) gb.apu.reset();
			gb.apu.regs.apuEnable = apuEnable;
			break;
		}

		default:
			if (addr >= 0xFF30 && addr <= 0xFF3F)
				gb.apu.channel3.waveRAM[addr - 0xFF30] = val;
		}
	}
	else if (addr <= 0xFFFE)
	{
		HRAM[addr - 0xFF80] = val;
	}
	else
		gb.cpu.s.IE = val;
}

template uint8_t MMU::read8<GBSystem::DMG>(uint16_t) const;
template uint8_t MMU::read8<GBSystem::GBC>(uint16_t) const;

template <GBSystem sys>
uint8_t MMU::read8(uint16_t addr) const
{
	if (gb.cpu.executingBootROM)
	{
		if (addr < 0x100)
			return baseBootROM[addr];

		if constexpr (sys == GBSystem::GBC)
		{
			if (addr >= 0x200 && addr <= 0x8FF)
				return cgbBootROM[addr - 0x200];
		}
	}

	if (addr <= 0x7FFF)
	{
		uint8_t val = gb.cartridge.getMapper()->read(addr);

		for (const auto& genie : gb.gameGenies)
		{
			if (addr == genie.addr && val == genie.oldData && genie.enable)
				return genie.newData;
		}

		return val;
	}
	if (addr <= 0x9FFF)
	{
		return gb.ppu->canAccessVRAM ? gb.ppu->VRAM[addr - 0x8000] : 0xFF;
	}
	if (addr <= 0xBFFF)
	{
		return gb.cartridge.getMapper()->read(addr);
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
		return gb.ppu->canAccessOAM && !dmaInProgress() ? gb.ppu->OAM[addr - 0xFE00] : 0xFF;
	}
	if (addr <= 0xFEFF)
	{
		// prohibited area

		if constexpr (sys == GBSystem::DMG)
			return gb.ppu->canAccessOAM ? 0x00 : 0xFF;
		else
			return 0xFF;
	}
	if (addr <= 0xFF7F)
	{
		switch (addr)
		{
		case 0xFF00:
			return gb.joypad.readInputReg();
		case 0xFF01:
			return gb.serial.s.serial_reg;
		case 0xFF02:
			return gb.serial.s.serial_control;
		case 0xFF04:
			return gb.cpu.s.DIV_reg;
		case 0xFF05:
			return gb.cpu.s.TIMA_reg;
		case 0xFF06:
			return gb.cpu.s.TMA_reg;
		case 0xFF07:
			return gb.cpu.s.TAC_reg;
		case 0xFF0F:
			return gb.cpu.s.IF;
		case 0xFF40:
			return gb.ppu->regs.LCDC;
		case 0xFF41:
			return gb.ppu->regs.STAT;
		case 0xFF42:
			return gb.ppu->regs.SCY;
		case 0xFF43:
			return gb.ppu->regs.SCX;
		case 0xFF44:
			return gb.ppu->s.LY;
		case 0xFF45:
			return gb.ppu->regs.LYC;
		case 0xFF46:
			return s.dma.reg;
		case 0xFF47:
			return gb.ppu->regs.BGP;
		case 0xFF48:
			return gb.ppu->regs.OBP0;
		case 0xFF49:
			return gb.ppu->regs.OBP1;
		case 0xFF4A:
			return gb.ppu->regs.WY;
		case 0xFF4B:
			return gb.ppu->regs.WX;

		case 0xFF4D:
			if constexpr (sys == GBSystem::GBC)
				return 0x7E | (static_cast<uint8_t>(gb.cpu.s.GBCdoubleSpeed) << 7) | static_cast<uint8_t>(gb.cpu.s.prepareSpeedSwitch);
			else
				return 0xFF;
		case 0xFF4F:
			if constexpr (sys == GBSystem::GBC)
				return gb.ppu->gbcRegs.VBK;
			else 
				return 0xFF;
		case 0xFF68:
			if constexpr (sys == GBSystem::GBC)
				return gb.ppu->gbcRegs.BCPS.readReg();
			else
				return 0xFF;
		case 0xFF69:
			if constexpr (sys == GBSystem::GBC)
				return gb.ppu->gbcRegs.BCPS.readPaletteRAM();
			else
				return 0xFF;
		case 0xFF6A:
			if constexpr (sys == GBSystem::GBC)
				return gb.ppu->gbcRegs.OCPS.readReg();
			else
				return 0xFF;
		case 0xFF6B:
			if constexpr (sys == GBSystem::GBC)
				return gb.ppu->gbcRegs.OCPS.readPaletteRAM();
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
			return gb.apu.channel1.regs.NR10 | 0x80;
		case 0xFF11: 
			return gb.apu.channel1.regs.NRx1 | 0x3F;
		case 0xFF12: 
			return gb.apu.channel1.regs.NRx2; // | 0x00;
		case 0xFF13: 
			return 0xFF; // gb.apu.channel1.regs.NRx3 | 0xFF;
		case 0xFF14: 
			return gb.apu.channel1.regs.NRx4 | 0xBF;
		case 0xFF16: 
			return gb.apu.channel2.regs.NRx1 | 0x3F;
		case 0xFF17: 
			return gb.apu.channel2.regs.NRx2; // | 0x00;
		case 0xFF18: 
			return 0xFF; // gb.apu.channel2.regs.NRx3 | 0xFF;
		case 0xFF19: 
			return gb.apu.channel2.regs.NRx4 | 0xBF;		
		case 0xFF1A: 
			return gb.apu.channel3.regs.NR30 | 0x7F;
		case 0xFF1B:
			return 0xFF; // gb.apu.channel3.regs.NR31 | 0xFF;
		case 0xFF1C: 
			return gb.apu.channel3.regs.NR32 | 0x9F;
		case 0xFF1D: 
			return 0xFF; // gb.apu.channel3.regs.NR33 | 0xFF;
		case 0xFF1E: 
			return gb.apu.channel3.regs.NR34 | 0xBF;
		case 0xFF20: 
			return 0xFF; // gb.apu.channel4.regs.NR41 | 0xFF;
		case 0xFF21: 
			return gb.apu.channel4.regs.NR42; // | 0x00;
		case 0xFF22:
			return gb.apu.channel4.regs.NR43; // | 0x00;
		case 0xFF23: 
			return gb.apu.channel4.regs.NR44 | 0xBF;
		case 0xFF24: 
			return gb.apu.regs.NR50;
		case 0xFF25: 
			return gb.apu.regs.NR51;
		case 0xFF26:
			return gb.apu.getNR52();

		default:
			if (addr >= 0xFF30 && addr <= 0xFF3F)
				return gb.apu.channel3.waveRAM[addr - 0xFF30];

			return 0xFF;
		}
	}
	if (addr <= 0xFFFE)
	{
		return HRAM[addr - 0xFF80];
	}
	else
		return gb.cpu.s.IE;
}