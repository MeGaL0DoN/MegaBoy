#include "MMU.h"
#include "GBCore.h"
#include "defines.h"
#include "Utils/rngOps.h"

MMU::MMU(GBCore& gb) : gb(gb) { updateSystem(); }

void MMU::updateSystem()
{
	switch (System::Current())
	{
	case GBSystem::DMG:		
		readFunc = &MMU::read8<GBSystem::DMG>;
		writeFunc = &MMU::write8<GBSystem::DMG>;
		break;
	case GBSystem::CGB:
		readFunc = &MMU::read8<GBSystem::CGB>;
		writeFunc = &MMU::write8<GBSystem::CGB>;
		break;
	case GBSystem::DMGCompatMode:
		readFunc = &MMU::read8<GBSystem::DMGCompatMode>;
		writeFunc = &MMU::write8<GBSystem::DMGCompatMode>;
		break;
	}
}

void MMU::reset()
{
	s = {};
	gbc = {};
	dmgCompatSwitch = false;

	for (int i = 0; i < 0x2000; i++)
		WRAM_BANKS[i] = RngOps::gen8bit();

	if (System::Current() == GBSystem::CGB)
	{
		// WRAM Bank 2 is zeroed instead.
		for (int i = 0x2000; i < 0x3000; i++)
			WRAM_BANKS[i] = 0;

		for (int i = 0x3000; i < 0x8000; i++)
			WRAM_BANKS[i] = RngOps::gen8bit();
	}

	for (uint8_t& i : HRAM)
		i = RngOps::gen8bit();
}

void MMU::saveState(std::ostream& st) const
{
	ST_WRITE(s);

	if (System::IsCGBDevice(System::Current())) // Some registers in gbc struct still usable in DMG compat mode.
		ST_WRITE(gbc); 

	const int WRAMSize { System::Current() == GBSystem::CGB ? 0x8000 : 0x2000 };

	st.write(reinterpret_cast<const char*>(WRAM_BANKS.data()), WRAMSize);
	ST_WRITE_ARR(HRAM);
}

void MMU::loadState(std::istream& st)
{
	ST_READ(s);

	// It's used to index array, so clamp it to 0 so it doesn't crash the emulator if state is invalid.
	s.dma.cycles = s.dma.cycles >= sizeof(PPU::OAM) ? 0 : s.dma.cycles;

	if (System::IsCGBDevice(System::Current()))
		ST_READ(gbc);

	const int WRAMSize { System::Current() == GBSystem::CGB ? 0x8000 : 0x2000 };

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
	s.dma.sourceAddr = s.dma.reg >= 0xFE ? (0xDE00 + ((s.dma.reg - 0xFE) * 0x100)) : s.dma.reg * 0x100;

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
		gb.ppu->OAM[s.dma.cycles++] = (this->*readFunc)(s.dma.sourceAddr++);

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
		gbc.ghdma.transferLength--;

		// Upper 3 bits of dest address are masked in place, actual address is not modified.
		for (int i = 0; i < 0x10; i++)
			gb.ppu->VRAM[(gbc.ghdma.destAddr++) & 0x1FFF] = (this->*readFunc)(gbc.ghdma.sourceAddr++);

		// Since it's actually (transferLength - 1), transfer is over once it underflows to FF.
		// Also when dest address overflows.
		if (gbc.ghdma.transferLength == 0xFF || gbc.ghdma.destAddr == 0x0000) 
		{
			gbc.ghdma.status = GHDMAStatus::None;
			gbc.ghdma.active = false;
		}
		else if (gbc.ghdma.status == GHDMAStatus::HDMA)
			gbc.ghdma.active = false;
	}
}

template void MMU::write8<GBSystem::DMG>(uint16_t, uint8_t);
template void MMU::write8<GBSystem::CGB>(uint16_t, uint8_t);
template void MMU::write8<GBSystem::DMGCompatMode>(uint16_t, uint8_t);

template <GBSystem sys>
void MMU::write8(uint16_t addr, uint8_t val)
{
	if (addr <= 0x7FFF)
	{
		gb.cartridge.getMapper()->write(addr, val);
	}
	else if (addr <= 0x9FFF)
	{
		if (gb.ppu->canWriteVRAM())
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
		if constexpr (sys == GBSystem::CGB)
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
		if (gb.ppu->canWriteOAM() && !dmaInProgress())
			gb.ppu->OAM[addr - 0xFE00] = val;
	}
	else if (addr <= 0xFF7F)
	{
		switch (addr)
		{
		case 0xFF00:
			gb.joypad.writeInputReg(val);
			break;
		case 0xFF01:
			gb.serial.s.serialReg = val;
			break;
		case 0xFF02:
			gb.serial.writeSerialControl(val);
			break;
		case 0xFF04:
			gb.cpu.s.divCounter = 0;
			break;
		case 0xFF05:
			if (!gb.cpu.s.timaOverflowed)
			{
				gb.cpu.s.timaReg = val;
				gb.cpu.s.timaOverflowDelay = false;
			}
			break;
		case 0xFF06:
			gb.cpu.s.tmaReg = val;

			if (gb.cpu.s.timaOverflowed)
				gb.cpu.s.timaReg = val;

			break;
		case 0xFF07:
			gb.cpu.writeTacReg(val);
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

			// spurious STAT interrupts (DMG only, doesn't happen on DMG compat mode on CGB)
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
			PPU::updatePalette(val, gb.ppu->BGP);
			break;
		case 0xFF48:
			gb.ppu->regs.OBP0 = val;
			PPU::updatePalette(val, gb.ppu->OBP0);
			break;
		case 0xFF49:
			gb.ppu->regs.OBP1 = val;
			PPU::updatePalette(val, gb.ppu->OBP1);
			break;
		case 0xFF4A:
			gb.ppu->regs.WY = val;
			break;
		case 0xFF4B:
			gb.ppu->regs.WX = val;
			break;
		case 0xFF50: // BANK register used to unmap boot ROM.
			if (!isBootROMMapped)
				return;

			isBootROMMapped = false;

			if (bootRomExitEvent != nullptr)
				bootRomExitEvent();

			break;
		case 0xFF4C: // KEY0
			if constexpr (sys == GBSystem::CGB)
			{
				if (isBootROMMapped && getBit(val, 2))
					dmgCompatSwitch = true;
			}
			break;
		case 0xFF4D:
			if constexpr (sys == GBSystem::CGB)
				gb.cpu.s.prepareSpeedSwitch = getBit(val, 0);
			break;
		case 0xFF4F:
			if constexpr (sys == GBSystem::CGB)
				gb.ppu->setVRAMBank(val);
			break;
		case 0xFF68:
			if constexpr (sys == GBSystem::CGB)
				gb.ppu->gbcRegs.BCPS.writeReg(val);
			break;
		case 0xFF69:
			if constexpr (sys == GBSystem::CGB)
				gb.ppu->gbcRegs.BCPS.writePaletteRAM(val);
			break;
		case 0xFF6A:
			if constexpr (sys == GBSystem::CGB)
				gb.ppu->gbcRegs.OCPS.writeReg(val);
			break;
		case 0xFF6B:
			if constexpr (sys == GBSystem::CGB)
				gb.ppu->gbcRegs.OCPS.writePaletteRAM(val);
			break;
		case 0xFF70:
			if constexpr (sys == GBSystem::CGB)
			{
				gbc.wramBank = val & 0x7;
				if (gbc.wramBank == 0) gbc.wramBank = 1;
			}
			break;
		case 0xFF51:
			if constexpr (sys == GBSystem::CGB)
				gbc.ghdma.sourceAddr = (gbc.ghdma.sourceAddr & 0x00FF) | (val << 8);
			break;
		case 0xFF52:
			if constexpr (sys == GBSystem::CGB)
				gbc.ghdma.sourceAddr = (gbc.ghdma.sourceAddr & 0xFF00) | (val & 0xF0);
			break;
		case 0xFF53:
			if constexpr (sys == GBSystem::CGB)
				gbc.ghdma.destAddr = (gbc.ghdma.destAddr & 0x00FF) | (val << 8); 
			break;
		case 0xFF54:
			if constexpr (sys == GBSystem::CGB)
				gbc.ghdma.destAddr = (gbc.ghdma.destAddr & 0xFF00) | (val & 0xF0);
			break;
		case 0xFF55:
			if constexpr (sys == GBSystem::CGB)
			{
				gbc.ghdma.transferLength = val & 0x7F;

				if (gbc.ghdma.status != GHDMAStatus::None)
				{
					if (!getBit(val, 7))
						gbc.ghdma.status = GHDMAStatus::None;
				}
				else
				{
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
		case 0xFF56:
			if constexpr (sys == GBSystem::CGB)
				gbc.FF56 = val;
			break;
		case 0xFF72:
			if constexpr (System::IsCGBDevice(sys)) // Undocumented CGB registers, still usable in DMG compat mode.
				gbc.FF72 = val;
			break;
		case 0xFF73:
			if constexpr (System::IsCGBDevice(sys))
				gbc.FF73 = val;
			break;
		case 0xFF74:
			if constexpr (sys == GBSystem::CGB) // This one is not writable in DMG compat mode.
				gbc.FF74 = val;
			break;
		case 0xFF75:
			if constexpr (System::IsCGBDevice(sys))
				gbc.FF75 = val;
			break;
		default:
			// Audio registers are not writable when APU is disabled.
			if (gb.apu.enabled())
			{
				switch (addr)
				{
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
				}
			}

			if (addr == 0xFF26)
			{
				const bool apuEnable = getBit(val, 7);

				if (gb.apu.enabled() && !apuEnable)
					gb.apu.powerOff();

				gb.apu.regs.apuEnable = apuEnable;
				break;
			}

			if (addr >= 0xFF30 && addr <= 0xFF3F)
				gb.apu.channel3.waveRAM[addr - 0xFF30] = val;

			break;
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
template uint8_t MMU::read8<GBSystem::CGB>(uint16_t) const;
template uint8_t MMU::read8<GBSystem::DMGCompatMode>(uint16_t) const;

template <GBSystem sys>
uint8_t MMU::read8(uint16_t addr) const
{
	if (addr <= 0x7FFF)
	{
		if (isBootROMMapped) [[unlikely]]
		{
			if (addr < 0x100)
				return baseBootROM[addr];

			if constexpr (System::IsCGBDevice(sys))
			{
				if (addr >= 0x200 && addr <= 0x8FF)
					return cgbBootROM[addr - 0x200];
			}
		}

		const uint8_t val { gb.cartridge.getMapper()->read(addr) };

		for (const auto& genie : gb.gameGenies)
		{
			if (addr == genie.addr && val == genie.oldData && genie.enable)
				return genie.newData;
		}

		return val;
	}
	if (addr <= 0x9FFF)
	{
		return gb.ppu->canReadVRAM() ? gb.ppu->VRAM[addr - 0x8000] : 0xFF;
	}
	if (addr <= 0xBFFF)
	{
		return gb.cartridge.getMapper()->read(addr);
	}
	if constexpr (sys == GBSystem::CGB)
	{
		if (addr <= 0xCFFF)
			return WRAM_BANKS[addr - 0xC000];
		if (addr <= 0xDFFF)
			return WRAM_BANKS[gbc.wramBank * 0x1000 + addr - 0xD000];
	}
	else if (addr <= 0xDFFF)
		return WRAM_BANKS[addr - 0xC000];

	if (addr <= 0xFDFF)
	{
		return WRAM_BANKS[addr - 0xE000];
	}
	if (addr <= 0xFE9F)
	{
		return gb.ppu->canReadOAM() && !dmaInProgress() ? gb.ppu->OAM[addr - 0xFE00] : 0xFF;
	}
	if (addr <= 0xFEFF)
	{
		// prohibited area

		if constexpr (sys == GBSystem::DMG) // Doesn't apply to DMG compat mode I think.
			return gb.ppu->canReadOAM() ? 0x00 : 0xFF;
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
			return gb.serial.s.serialReg;
		case 0xFF02:
			return gb.serial.readSerialControl();
		case 0xFF04:
			return gb.cpu.s.divCounter >> 8; 
		case 0xFF05:
			return gb.cpu.s.timaReg;
		case 0xFF06:
			return gb.cpu.s.tmaReg;
		case 0xFF07:
			return gb.cpu.s.tacReg | 0b11111000;
		case 0xFF0F:
			return gb.cpu.s.IF;
		case 0xFF40:
			return gb.ppu->regs.LCDC;
		case 0xFF41:
			return gb.ppu->readSTAT();
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
			if constexpr (sys == GBSystem::CGB)
				return 0x7E | (static_cast<uint8_t>(gb.cpu.s.cgbDoubleSpeed) << 7) | static_cast<uint8_t>(gb.cpu.s.prepareSpeedSwitch);
			else
				return 0xFF;
		case 0xFF4F:
			if constexpr (sys == GBSystem::CGB)
				return gb.ppu->gbcRegs.VBK;
			else if constexpr (sys == GBSystem::DMGCompatMode)
				return 0xFE; // Bit 0 is always read as set.
			else 
				return 0xFF;
		case 0xFF68:
			if constexpr (sys == GBSystem::CGB)
				return gb.ppu->gbcRegs.BCPS.readReg();
			else if constexpr (sys == GBSystem::DMGCompatMode)
				return 0xC8;
			else
				return 0xFF;
		case 0xFF69:
			if constexpr (sys == GBSystem::CGB)
				return gb.ppu->gbcRegs.BCPS.readPaletteRAM();
			else
				return 0xFF;
		case 0xFF6A:
			if constexpr (sys == GBSystem::CGB)
				return gb.ppu->gbcRegs.OCPS.readReg();
			else if constexpr (sys == GBSystem::DMGCompatMode)
				return 0xD0;
			else
				return 0xFF;
		case 0xFF6B:
			if constexpr (sys == GBSystem::CGB)
				return gb.ppu->gbcRegs.OCPS.readPaletteRAM();
			else
				return 0xFF;
		case 0xFF70:
			if constexpr (sys == GBSystem::CGB)
				return gbc.wramBank;
			else
				return 0xFF;

		// GHDMA address registers, write-only.
		case 0xFF51:
			return 0xFF;
		case 0xFF52:
			return 0xFF;
		case 0xFF53:
			return 0xFF;
		case 0xFF54:
			return 0xFF;

		case 0xFF55:
			if constexpr (sys == GBSystem::CGB)
				return ((gbc.ghdma.status == GHDMAStatus::None) << 7) | gbc.ghdma.transferLength;
			else
				return 0xFF;
		case 0xFF56:
			if constexpr (sys == GBSystem::CGB)
				return gbc.FF56 | 0x3C;
			else
				return 0xFF;
		case 0xFF6C: // OPRI, bit 0 is set only in CGB mode.
			if constexpr (sys == GBSystem::CGB)
				return 0xFE;
			else
				return 0xFF;
		case 0xFF72:
			if constexpr (System::IsCGBDevice(sys))
				return gbc.FF72;
			else
				return 0xFF;
		case 0xFF73:
			if constexpr (System::IsCGBDevice(sys))
				return gbc.FF73;
			else
				return 0xFF;
		case 0xFF74:
			if constexpr (sys == GBSystem::CGB)
				return gbc.FF74;
			else
				return 0xFF;
		case 0xFF75:
			if constexpr (System::IsCGBDevice(sys))
				return gbc.FF75 | 0b10001111;
			else
				return 0xFF;
		case 0xFF76:
			if constexpr (System::IsCGBDevice(sys))
				return gb.apu.readPCM12();
			else
				return 0xFF;
		case 0xFF77:
			if constexpr (System::IsCGBDevice(sys))
				return gb.apu.readPCM34();
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