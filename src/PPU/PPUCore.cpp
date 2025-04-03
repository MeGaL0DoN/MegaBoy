#include <iostream>
#include <algorithm>
#include <cstring>

#include "PPUCore.h"
#include "../GBCore.h"
#include "../Utils/bitOps.h"

template class PPUCore<GBSystem::DMG>;
template class PPUCore<GBSystem::CGB>;
template class PPUCore<GBSystem::DMGCompatMode>;

template <GBSystem sys>
void PPUCore<sys>::reset(bool clearBuf)
{
	std::memset(OAM.data(), 0, sizeof(OAM));
	std::memset(VRAM_BANK0.data(), 0, sizeof(VRAM_BANK0));
	VRAM = VRAM_BANK0.data();

	if constexpr (System::IsCGBDevice(sys))
	{
		std::memset(VRAM_BANK1.data(), 0, sizeof(VRAM_BANK1));
		gbcRegs.reset();
	}
	if constexpr (sys != GBSystem::CGB)
		updatePalette(regs.BGP, this->BGP);

	s = {};
	regs = {};

	if (clearBuf)
		clearBuffer(true);

	SetPPUMode(PPUMode::VBlank);
	s.LY = 144;
}

template <GBSystem s>
void PPUCore<s>::saveState(std::ostream& st) const
{
	ST_WRITE(regs);
	ST_WRITE(s);

	// Note: here important to use System::Current() instead of constexpr template parameter, since system can be changed after creating the ppu object.
	// Like when running CGB boot rom with DMG game, once boot rom finishes we need to convert PPU to DMG version, so the state need to be saved first,
	// but no need to save CGB only state, like VRAM_BANK1.

	const auto sys { System::Current() };

	if (System::IsCGBDevice(sys))
	{
		gbcRegs.saveState(st);

		if (sys == GBSystem::CGB)
			ST_WRITE_ARR(VRAM_BANK1);
	}

	ST_WRITE_ARR(VRAM_BANK0);
	ST_WRITE_ARR(OAM);

	if (s.state == PPUMode::PixelTransfer)
	{
		bgFIFO.saveState(st);
		objFIFO.saveState(st);
	}
	if (s.state == PPUMode::OAMSearch || s.state == PPUMode::PixelTransfer)
	{
		ST_WRITE(objCount);
		st.write(reinterpret_cast<const char*>(selectedObjects.data()), sizeof(selectedObjects[0]) * objCount);
	}
}

template <GBSystem s>
void PPUCore<s>::loadState(std::istream& st)
{
	ST_READ(regs);
	ST_READ(s);

	const auto sys { System::Current() };

	if (System::IsCGBDevice(sys))
	{
		gbcRegs.loadState(st);

		if (sys == GBSystem::CGB)
			ST_READ_ARR(VRAM_BANK1);
	}
	if (sys != GBSystem::CGB)
	{
		updatePalette(regs.BGP,  this->BGP);
		updatePalette(regs.OBP0, this->OBP0);
		updatePalette(regs.OBP1, this->OBP1);
	}

	ST_READ_ARR(VRAM_BANK0);
	ST_READ_ARR(OAM);

	if (s.state == PPUMode::PixelTransfer)
	{
		bgFIFO.loadState(st);
		objFIFO.loadState(st);
	}
	if (s.state == PPUMode::OAMSearch || s.state == PPUMode::PixelTransfer)
	{
		ST_READ(objCount);
		st.read(reinterpret_cast<char*>(selectedObjects.data()), sizeof(selectedObjects[0]) * objCount);
	}
}

template <GBSystem sys>
void PPUCore<sys>::refreshDMGScreenColors(const std::array<color, 4>& newColorPalette)
{
	if constexpr (sys != GBSystem::DMG) 
		return;

	for (uint8_t y = 0; y < SCR_HEIGHT; y++)
	{
		for (uint8_t x = 0; x < SCR_WIDTH; x++)
		{
			const color pixel { getPixel(x, y) };
			const uint8_t pixelInd { static_cast<uint8_t>(std::find(PPU::ColorPalette, PPU::ColorPalette + 4, pixel) - PPU::ColorPalette) };
			PixelOps::setPixel(framebuffer.get(), SCR_WIDTH, x, y, newColorPalette[pixelInd]);
		}
	}
}

template <GBSystem sys>
void PPUCore<sys>::setLCDEnable(bool val)
{
	if (static_cast<bool>(getBit(regs.LCDC, 7)) == val)
		return;

	if (val)
	{
		if constexpr (sys == GBSystem::CGB)
			s.lcdSkipFrame = s.videoCycles >= TOTAL_VBLANK_CYCLES;
		else
			s.lcdSkipFrame = true;

		s.videoCycles = 0;
		s.hblankCycles = OAM_SCAN_CYCLES - 4;
		s.dotsUntilVBlank = GBCore::CYCLES_PER_FRAME - TOTAL_VBLANK_CYCLES - 4;
		updateInterrupts();
	}
	else
	{
		s.videoCycles = 0;
		s.LY = 0;
		s.WLY = 0;
		SetPPUMode(PPUMode::HBlank);
	}

	regs.LCDC = setBit(regs.LCDC, 7, val);
}

template <GBSystem sys>
bool PPUCore<sys>::canReadVRAM()
{
	return s.prevState != PPUMode::PixelTransfer && (s.prevState == PPUMode::HBlank || s.state != PPUMode::PixelTransfer);
}
template <GBSystem sys>
bool PPUCore<sys>::canReadOAM()
{
	return (s.prevState == PPUMode::HBlank && s.state != PPUMode::OAMSearch) || s.state == PPUMode::VBlank;
}

template <GBSystem sys>
bool PPUCore<sys>::canWriteVRAM()
{
	return s.prevState != PPUMode::PixelTransfer;
}
template <GBSystem sys>
bool PPUCore<sys>::canWriteOAM()
{
	return (s.prevState != PPUMode::OAMSearch || s.state != PPUMode::OAMSearch) && s.prevState != PPUMode::PixelTransfer; 
}

template <GBSystem sys>
void PPUCore<sys>::updateInterrupts()
{
	bool interrupt { false };

	// For 1M cycle after hblank is over and LY is incremented, lyc flag is forced to false.
	if (s.lyIncremented) [[unlikely]]
	{
		regs.STAT = resetBit(regs.STAT, 2);
		s.lyIncremented = false;
	}
	else
	{
		const bool lycFlag { s.LY == regs.LYC };
		interrupt = lycFlag && LYC_STAT();
		regs.STAT = setBit(regs.STAT, 2, lycFlag);
	}

	switch (s.state)
	{
	case PPUMode::HBlank:
		interrupt |= HBlank_STAT();
		break;
	case PPUMode::VBlank:
		// OAM_STAT (STAT bit 5) also triggers on vblank when LY is 144! vblank_stat_intr-GS.gb mooneye test tests this.
		interrupt |= (VBlank_STAT() || (s.LY == 144 && OAM_STAT()));
		break;
	case PPUMode::OAMSearch:
		interrupt |= OAM_STAT();
		break;
	default:
		break;
	}

	if (interrupt)
	{
		if (!s.blockStat)
		{
			s.blockStat = true;
			cpu.requestInterrupt(Interrupt::STAT);
		}
	}
	else
		s.blockStat = false;
}

template <GBSystem sys>
void PPUCore<sys>::SetPPUMode(PPUMode mode)
{
	switch (mode)
	{
	case PPUMode::HBlank:
		s.hblankCycles = TOTAL_SCANLINE_CYCLES - OAM_SCAN_CYCLES - s.videoCycles;

		if constexpr (sys == GBSystem::CGB)
		{
			if (mmu.gbc.ghdma.status == GHDMAStatus::HDMA) 
				mmu.gbc.ghdma.active = true;
		}
		break;
	case PPUMode::VBlank:
		s.dotsUntilVBlank = GBCore::CYCLES_PER_FRAME;
		s.vblankLineCycles = DEFAULT_VBLANK_LINE_CYCLES;
		break;
	case PPUMode::OAMSearch:
		break;
	case PPUMode::PixelTransfer:
		resetPixelTransferState();
		break;
	}

	s.state = mode;
}

template <GBSystem sys>
void PPUCore<sys>::execute()
{
	s.prevState = s.state;

	if constexpr (System::IsCGBDevice(sys))
	{
		if (s.cgbVBlankFlag) [[unlikely]]
		{
			cpu.requestInterrupt(Interrupt::VBlank);
			s.cgbVBlankFlag = false;
		}
	}

	if (!LCDEnabled())
	{
		if constexpr (sys == GBSystem::CGB)
		{
			// On CGB LCD is not cleared immediately after being disabled. The game "Bug's Life" depends on this behavior, as it keeps disabling and enabling lcd very often.
			// I am not sure after how long exactly it gets cleared, but I am assuming its 4560 cycles (VBlank duration).

			if (s.videoCycles < TOTAL_VBLANK_CYCLES)
			{
				s.videoCycles += cpu.TcyclesPerM();

				if (s.videoCycles >= TOTAL_VBLANK_CYCLES) [[unlikely]]
					clearBuffer();
			}
		}

		return;
	};

	const auto tick = [&]()
	{
		s.videoCycles++;
		s.dotsUntilVBlank--;

		switch (s.state)
		{
		case PPUMode::OAMSearch:
			handleOAMSearch();
			break;
		case PPUMode::PixelTransfer:
			handlePixelTransfer();
			break;
		case PPUMode::HBlank:
			handleHBlank();
			break;
		case PPUMode::VBlank:
			handleVBlank();
			break;
		}
	};

	tick();
	tick();

	if constexpr (sys == GBSystem::CGB)
	{
		if (!cpu.doubleSpeedMode())
		{
			tick();
			tick();
		}
	}
	else
	{
		tick();
		tick();
	}

	updateInterrupts();
}

template <GBSystem sys>
void PPUCore<sys>::handleHBlank()
{
	if (s.videoCycles >= s.hblankCycles) [[unlikely]]
	{
		s.videoCycles -= s.hblankCycles;

		// After LCD is being enabled, first hblank is "fake" and is 4 cycles shorter than oam scan, and continues to pixel transfer instead of oam scan.
		const bool isFakeHBlank { s.hblankCycles == OAM_SCAN_CYCLES - 4 };

		if (!isFakeHBlank) [[likely]]
		{
			s.LY++;
			if (bgFIFO.s.fetchingWindow) s.WLY++;
			s.lyIncremented = true;
		}

		if constexpr (sys == GBSystem::CGB)
		{
			if (mmu.gbc.ghdma.status == GHDMAStatus::HDMA)
				mmu.gbc.ghdma.active = false;
		}

		if (s.LY == 144) [[unlikely]]
		{
			SetPPUMode(PPUMode::VBlank);

			// On CGB VBlank interrupt is 1M cycle delayed.
			if constexpr (System::IsCGBDevice(sys))
				s.cgbVBlankFlag = true;
			else
				cpu.requestInterrupt(Interrupt::VBlank);

			if (s.lcdSkipFrame)
			{
				s.lcdSkipFrame = false;
				clearBuffer();
			}
			else
				invokeDrawCallback();
		}
		else
			SetPPUMode(isFakeHBlank ? PPUMode::PixelTransfer : PPUMode::OAMSearch);
	}
}

template <GBSystem sys>
void PPUCore<sys>::handleOAMSearch()
{
	if (s.videoCycles >= OAM_SCAN_CYCLES) [[unlikely]]
	{
		s.videoCycles -= OAM_SCAN_CYCLES;
		objCount = 0;

		for (uint8_t oamAddr = 0; oamAddr < sizeof(OAM) && objCount < 10; oamAddr += 4)
		{
			const int16_t objY { static_cast<int16_t>(OAM[oamAddr] - 16) };
			const int16_t objX { static_cast<int16_t>(OAM[oamAddr + 1] - 8) };
			const uint8_t tileInd { OAM[oamAddr + 2] };
			const uint8_t attributes { OAM[oamAddr + 3] };
			const bool yFlip { static_cast<bool>(getBit(attributes, 6)) };

			if (!DoubleOBJSize())
			{
				if (s.LY >= objY && s.LY < objY + 8)
					selectedObjects[objCount++] = OAMobject{ objX, objY, static_cast<uint16_t>(tileInd * 16), attributes, oamAddr };
			}
			else
			{
				if (s.LY >= objY && s.LY < objY + 8)
				{
					const uint16_t tileAddr = yFlip ? ((tileInd & 0xFE) + 1) * 16 : (tileInd & 0xFE) * 16;
					selectedObjects[objCount++] = OAMobject{ objX, objY, tileAddr, attributes, oamAddr };
				}
				else if (s.LY >= objY + 8 && s.LY < objY + 16)
				{
					const uint16_t tileAddr = yFlip ? (tileInd & 0xFE) * 16 : ((tileInd & 0xFE) + 1) * 16;
					selectedObjects[objCount++] = OAMobject{ objX, static_cast<int16_t>(objY + 8), tileAddr, attributes, oamAddr };
				}
			}
		}

		std::stable_sort(selectedObjects.begin(), selectedObjects.begin() + objCount, [](const auto& a, const auto& b) { return a.X < b.X; });

		SetPPUMode(PPUMode::PixelTransfer);
	}
}

template <GBSystem sys>
void PPUCore<sys>::handleVBlank()
{
	if (s.videoCycles >= s.vblankLineCycles) [[unlikely]]
	{
		s.videoCycles -= s.vblankLineCycles;
		s.LY++;

		switch (s.LY)
		{
		case 153:
			s.vblankLineCycles = 4;
			break;
		case 154:
			s.vblankLineCycles = 452;
			s.LY = 0;
			s.WLY = 0;
			break;
		case 1:
			s.LY = 0;
			SetPPUMode(PPUMode::OAMSearch);
			break;
		default:
			s.vblankLineCycles = DEFAULT_VBLANK_LINE_CYCLES;
			break;
		}
	}
}

template <GBSystem sys>
void PPUCore<sys>::resetPixelTransferState()
{
	bgFIFO.reset();
	objFIFO.reset();
	s.xPosCounter = 0;
	s.latchWindowEnable = WindowEnable();
}

template <GBSystem sys>
void PPUCore<sys>::handlePixelTransfer()
{
	tryStartSpriteFetcher();

	if (objFIFO.s.fetcherActive)
		executeObjFetcher();
	else
		executeBGFetcher();

	if (!bgFIFO.s.fetchingWindow)
	{
		if (s.latchWindowEnable && s.LY >= regs.WY && s.xPosCounter >= regs.WX - 7 && regs.WX != 0) [[unlikely]]
		{
			bgFIFO.reset();
			bgFIFO.s.fetchingWindow = true;
		}
	}

	if (!objFIFO.s.fetcherActive && !bgFIFO.empty()) [[likely]]
	{
		renderFIFOs();

		if (s.xPosCounter == SCR_WIDTH) [[unlikely]]
		{
			SetPPUMode(PPUMode::HBlank);
			s.videoCycles = 0;
		}
	}
}

template <GBSystem sys>
void PPUCore<sys>::tryStartSpriteFetcher()
{
	if (objFIFO.s.fetcherActive || !OBJEnable())
		return;

	if (objFIFO.s.objInd < objCount && selectedObjects[objFIFO.s.objInd].X <= s.xPosCounter)
	{
		objFIFO.s.state = FetcherState::FetchTileNo;
		objFIFO.s.fetcherActive = true;
	}
}

template <GBSystem sys>
void PPUCore<sys>::executeBGFetcher()
{
	switch (bgFIFO.s.state)
	{
	case FetcherState::FetchTileNo:
	{
		if ((++bgFIFO.s.cycles & 0x1) != 0)
			break;

		uint16_t tileMapInd;

		if (!bgFIFO.s.fetchingWindow)
		{
			const uint16_t yOffset = (static_cast<uint8_t>(s.LY + regs.SCY) / 8) * 32;
			const uint8_t xOffset = (bgFIFO.s.fetchX + (regs.SCX / 8)) & 0x1F;
			tileMapInd = BGTileMapAddr() + ((yOffset + xOffset) & 0x3FF);
		}
		else
		{
			const uint16_t yOffset = (s.WLY / 8) * 32;
			const uint8_t xOffset = bgFIFO.s.fetchX & 0x1F;
			tileMapInd = WindowTileMapAddr() + ((yOffset + xOffset) & 0x3FF);
		}

		bgFIFO.s.tileMap = VRAM_BANK0[tileMapInd];

		if constexpr (sys == GBSystem::CGB)
			bgFIFO.s.cgbAttributes = VRAM_BANK1[tileMapInd];

		bgFIFO.s.fetchX++;
		bgFIFO.s.state = FetcherState::FetchTileDataLow;
		break;
	}
	case FetcherState::FetchTileDataLow:
	{
		if ((++bgFIFO.s.cycles & 0x1) != 0)
			break;

		s.SCYlatch = regs.SCY;
		const int tileDataAddr { getBGTileAddr(bgFIFO.s.tileMap) + getBGTileOffset() };

		if constexpr (sys == GBSystem::CGB)
		{
			const uint8_t* bank { getBit(bgFIFO.s.cgbAttributes, 3) ? VRAM_BANK1.data() : VRAM_BANK0.data() };
			bgFIFO.s.tileLow = bank[tileDataAddr];
		}
		else
			bgFIFO.s.tileLow = VRAM_BANK0[tileDataAddr];

		bgFIFO.s.state = FetcherState::FetchTileDataHigh;
		break;
	}
	case FetcherState::FetchTileDataHigh:
	{
		if ((++bgFIFO.s.cycles & 0x1) != 0)
			break;

		if (bgFIFO.s.newScanline) [[unlikely]]
		{
			bgFIFO.s.fetchX--;
			bgFIFO.s.newScanline = false;
			bgFIFO.s.state = FetcherState::FetchTileNo;
			break;
		}

		const int tileDataAddr { getBGTileAddr(bgFIFO.s.tileMap) + getBGTileOffset() + 1 };

		if constexpr (sys == GBSystem::CGB)
		{
			const uint8_t* bank { getBit(bgFIFO.s.cgbAttributes, 3) ? VRAM_BANK1.data() : VRAM_BANK0.data() };
			bgFIFO.s.tileHigh = bank[tileDataAddr];
		}
		else
			bgFIFO.s.tileHigh = VRAM_BANK0[tileDataAddr];

		bgFIFO.s.state = FetcherState::PushFIFO;
		break;
	}
	case FetcherState::PushFIFO:
	{
		if (!bgFIFO.empty())
			break;

		if (bgFIFO.s.scanlineDiscardPixels == -1)
			bgFIFO.s.scanlineDiscardPixels = bgFIFO.s.fetchingWindow ? (regs.WX < 7 ? 7 - regs.WX : 0) : (regs.SCX & 0x7);

		int cnt { 7 };

		if (bgFIFO.s.fetchingWindow)
		{
			cnt -= bgFIFO.s.scanlineDiscardPixels;
			bgFIFO.s.scanlineDiscardPixels = 0;
		}

		if constexpr (sys == GBSystem::CGB)
		{
			const bool xFlip = getBit(bgFIFO.s.cgbAttributes, 5);
			const bool priority = getBit(bgFIFO.s.cgbAttributes, 7);
			const uint8_t cgbPalette = bgFIFO.s.cgbAttributes & 0x7;

			const int cntStart { (xFlip ? 0 : cnt) };
			const int cntEnd { xFlip ? cnt + 1 : -1 };
			const int cntStep { xFlip ? 1 : -1 };

			for (int i = cntStart; i != cntEnd; i += cntStep)
				bgFIFO.push(FIFOEntry{ getColorID(bgFIFO.s.tileLow, bgFIFO.s.tileHigh, i), cgbPalette, priority });
		}
		else
		{
			for (int i = cnt; i >= 0; i--)
				bgFIFO.push(FIFOEntry{ getColorID(bgFIFO.s.tileLow, bgFIFO.s.tileHigh, i) });
		}

		bgFIFO.s.cycles = 0;
		bgFIFO.s.state = FetcherState::FetchTileNo;
		break;
	}
	}
}

template <GBSystem sys>
void PPUCore<sys>::executeObjFetcher()
{
	const auto& obj { selectedObjects[objFIFO.s.objInd] };

	switch (objFIFO.s.state)
	{
	case FetcherState::FetchTileNo:
		if ((++objFIFO.s.cycles & 0x1) == 0)
			objFIFO.s.state = FetcherState::FetchTileDataLow;

		break;
	case FetcherState::FetchTileDataLow:
	{
		if ((++objFIFO.s.cycles & 0x1) != 0)
			break;

		if constexpr (sys == GBSystem::CGB)
		{
			const uint8_t* bank { getBit(obj.attributes, 3) ? VRAM_BANK1.data() : VRAM_BANK0.data() };
			objFIFO.s.tileLow = bank[obj.tileAddr + getObjTileOffset(obj)];
		}
		else
			objFIFO.s.tileLow = VRAM_BANK0[obj.tileAddr + getObjTileOffset(obj)];

		objFIFO.s.state = FetcherState::FetchTileDataHigh;
		break;
	}
	case FetcherState::FetchTileDataHigh:
	{
		if ((++objFIFO.s.cycles & 0x1) != 0)
			break;

		if constexpr (sys == GBSystem::CGB)
		{
			const uint8_t* bank { getBit(obj.attributes, 3) ? VRAM_BANK1.data() : VRAM_BANK0.data() };
			objFIFO.s.tileHigh = bank[obj.tileAddr + getObjTileOffset(obj) + 1];
		}
		else
			objFIFO.s.tileHigh = VRAM_BANK0[obj.tileAddr + getObjTileOffset(obj) + 1];

		objFIFO.s.state = FetcherState::PushFIFO;
		break;
	}
	case FetcherState::PushFIFO:
	{
		const bool bgPriority = getBit(obj.attributes, 7);
		const bool xFlip = getBit(obj.attributes, 5);
		uint8_t palette;

		if constexpr (sys == GBSystem::CGB)
			palette = obj.attributes & 0x7;
		else
			palette = getBit(obj.attributes, 4);

		while (!objFIFO.full())
			objFIFO.push(FIFOEntry{});

		const int cntStart { (obj.X < 0) ? (xFlip ? (obj.X * -1) : (obj.X + 7)) : (xFlip ? 0 : 7) };
		const int cntEnd { xFlip ? 8 : -1 };
		const int cntStep { xFlip ? 1 : -1 };

		for (int i = cntStart; i != cntEnd; i += cntStep)
		{
			const int fifoInd { xFlip ? (i - cntStart) : (cntStart - i) };
			const uint8_t colorId { getColorID(objFIFO.s.tileLow, objFIFO.s.tileHigh, i) };
			bool overwriteObj;

			if constexpr (sys == GBSystem::CGB)
				overwriteObj = colorId != 0 && (objFIFO[fifoInd].color == 0 || selectedObjects[objFIFO.s.objInd].oamAddr < selectedObjects[objFIFO.s.objInd - 1].oamAddr);
			else
				overwriteObj = objFIFO[fifoInd].color == 0;

			if (overwriteObj)
				objFIFO[fifoInd] = FIFOEntry{ colorId, palette, bgPriority };
		}

		objFIFO.s.objInd++;
		objFIFO.s.fetcherActive = false;

		tryStartSpriteFetcher();
		break;
	}
	}
}

template <GBSystem sys>
void PPUCore<sys>::renderFIFOs()
{
	auto bg { bgFIFO.pop() };

	if (bgFIFO.s.scanlineDiscardPixels > 0)
	{
		bgFIFO.s.scanlineDiscardPixels--;
		return;
	}

	if constexpr (sys != GBSystem::CGB)
		if (!DMGTileMapsEnable()) bg.color = 0;

	color outputColor;

	if (!objFIFO.empty())
	{
		const auto obj { objFIFO.pop() };
		bool objHasPriority { obj.color != 0 && OBJEnable() };

		if constexpr (sys == GBSystem::CGB)
			objHasPriority &= (bg.color == 0 || GBCMasterPriority() || (!obj.priority && !bg.priority));
		else
			objHasPriority &= (!obj.priority || bg.color == 0);

		outputColor = objHasPriority ? getColor<true, true>(obj.color, obj.palette) : getColor<false, true>(bg.color, bg.palette);

		if (debugPPU && objHasPriority)
			PixelOps::setPixel(debugOAMFramebuffer.get(), SCR_WIDTH, s.xPosCounter, s.LY, getColor<true>(obj.color, obj.palette));
	}
	else
		outputColor = getColor<false, true>(bg.color, bg.palette);

	if (debugPPU)
	{
		const auto framebuf { bgFIFO.s.fetchingWindow ? debugWindowFramebuffer.get() : debugBGFramebuffer.get() };
		PixelOps::setPixel(framebuf, SCR_WIDTH, s.xPosCounter, s.LY, getColor<false>(bg.color, bg.palette));
	}

	setPixel(s.xPosCounter, s.LY, outputColor);
	s.xPosCounter++;
}


// DEBUG


template <GBSystem sys>
void PPUCore<sys>::renderTileData(uint8_t* buffer, int vramBank)
{
	const uint8_t* vram { vramBank == 1 ? VRAM_BANK1.data() : VRAM_BANK0.data() };

	for (int addr = 0; addr < 0x17FF; addr += 16)
	{
		const int tileInd { addr / 16 };
		const int screenX { (tileInd % 16) * 8 };
		const int screenY { (tileInd / 16) * 8 };

		for (int y = 0; y < 8; y++)
		{
			const int yPos { y + screenY };
			const uint8_t lsbLineByte { vram[addr + y * 2] };
			const uint8_t msbLineByte { vram[addr + y * 2 + 1] };

			for (int x = 7; x >= 0; x--)
			{
				const uint8_t colorId { getColorID(lsbLineByte, msbLineByte, x) };
				const int xPos { 7 - x + screenX };
				PixelOps::setPixel(buffer, TILES_WIDTH, xPos, yPos, PPU::ColorPalette[colorId]);
			}
		}
	}
}

template <GBSystem sys>
void PPUCore<sys>::renderTileMap(uint8_t* buffer, uint16_t addr)
{
	for (int y = 0; y < 32; y++)
	{
		for (int x = 0; x < 32; x++)
		{
			const int tileMapInd { (addr - 0x8000) + y * 32 + x};
			const uint8_t tileMap { VRAM_BANK0[tileMapInd] };
			const int screenX { x * 8 }, screenY { y * 8 };

			if constexpr (sys == GBSystem::CGB)
			{
				const uint8_t attributes { VRAM_BANK1[tileMapInd] };

				const bool yFlip = getBit(attributes, 6);
				const bool xFlip = getBit(attributes, 5);
				const uint8_t* bank { getBit(attributes, 3) ? VRAM_BANK1.data() : VRAM_BANK0.data() };

				const int yStart { yFlip ? 7 : 0 }, yEnd { yFlip ? -1 : 8 }, yStep { yFlip ? -1 : 1 };

				for (int tileY = yStart; tileY != yEnd; tileY += yStep)
				{
					const uint8_t lsbLineByte { bank[getBGTileAddr(tileMap) + tileY * 2] };
					const uint8_t msbLineByte { bank[getBGTileAddr(tileMap) + tileY * 2 + 1] };

					const int yPos { tileY + screenY };
					const int xStart { xFlip ? 0 : 7 }, xEnd { xFlip ? 8 : -1 }, xStep { xFlip ? 1 : -1 };

					for (int tileX = xStart; tileX != xEnd; tileX += xStep)
					{
						const uint8_t colorId { getColorID(lsbLineByte, msbLineByte, tileX) };
						const int xPos { 7 - tileX + screenX };
						PixelOps::setPixel(buffer, TILEMAP_WIDTH, xPos, yPos, getColor<false>(colorId, attributes & 0x7));
					}
				}
			}
			else
			{
				for (int tileY = 0; tileY < 8; tileY++)
				{
					const int yPos { tileY + screenY };
					const uint8_t lsbLineByte { VRAM_BANK0[getBGTileAddr(tileMap) + tileY * 2] };
					const uint8_t msbLineByte { VRAM_BANK0[getBGTileAddr(tileMap) + tileY * 2 + 1] };

					for (int tileX = 7; tileX >= 0; tileX--)
					{
						const uint8_t colorId { getColorID(lsbLineByte, msbLineByte, tileX) };
						const int xPos { 7 - tileX + screenX };
						PixelOps::setPixel(buffer, TILEMAP_WIDTH, xPos, yPos, getColor<false>(colorId, 0));
					}
				}
			}
		}
	}
}