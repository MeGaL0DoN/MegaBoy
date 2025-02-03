#include <iostream>
#include <algorithm>
#include <cstring>

#include "PPUCore.h"
#include "../GBCore.h"
#include "../Utils/rngOps.h"
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
		updatePalette(regs.BGP, BGpalette);

	s = {};
	regs = {};

	if (clearBuf)
		clearBuffer(true);

	SetPPUMode(PPUMode::VBlank);
	s.LY = 144;
}

template <GBSystem sys>
void PPUCore<sys>::setLCDEnable(bool val)
{
	if (static_cast<bool>(getBit(regs.LCDC, 7)) == val)
		return;

	if (val)
	{
		s.lcdSkipFrame = s.videoCycles >= TOTAL_VBLANK_CYCLES;
		s.videoCycles = 0;
		s.hblankCycles = OAM_SCAN_CYCLES - 4;
		dotsUntilVBlank = GBCore::CYCLES_PER_FRAME - TOTAL_VBLANK_CYCLES - 4;
		regs.LCDC = setBit(regs.LCDC, 7);
	}
	else
	{
		s.videoCycles = 0;
		s.LY = 0;
		s.WLY = 0;
		SetPPUMode(PPUMode::HBlank);
		regs.LCDC = resetBit(regs.LCDC, 7);
	}
}

template <GBSystem sys>
void PPUCore<sys>::saveState(std::ostream& st) const
{
	ST_WRITE(regs);
	ST_WRITE(s);

	// Note: here important to use System::Current() instead of constexpr template parameter, since system can be changed after creating the ppu object.
	// Like when running CGB boot rom with DMG game, once boot rom finishes we need to convert PPU to DMG version, so the state need to be saved first,
	// but no need to save CGB only state, like VRAM_BANK1.

	const auto sys = System::Current();

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

template <GBSystem sys>
void PPUCore<sys>::loadState(std::istream& st)
{
	ST_READ(regs);
	ST_READ(s);

	const auto sys = System::Current();

	if (System::IsCGBDevice(sys))
	{
		gbcRegs.loadState(st);

		if (sys == GBSystem::CGB)
			ST_READ_ARR(VRAM_BANK1);
	}
	if (sys != GBSystem::CGB)
	{
		updatePalette(regs.BGP, BGpalette);
		updatePalette(regs.OBP0, OBP0palette);
		updatePalette(regs.OBP1, OBP1palette);
	}

	ST_READ_ARR(VRAM_BANK0);
	ST_READ_ARR(OAM);

	SetPPUMode(s.state);

	if (s.state == PPUMode::PixelTransfer)
	{
		bgFIFO.loadState(st);
		objFIFO.loadState(st);
		latchWindowEnable = WindowEnable();
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
			color pixel = getPixel(x, y);
			uint8_t pixelInd { static_cast<uint8_t>(std::find(PPU::ColorPalette, PPU::ColorPalette + 4, pixel) - PPU::ColorPalette) };
			PixelOps::setPixel(framebuffer.get(), SCR_WIDTH, x, y, newColorPalette[pixelInd]);
		}
	}
}

template <GBSystem sys>
void PPUCore<sys>::checkLYC()
{
	s.lycFlag = s.LY == regs.LYC;
	regs.STAT = setBit(regs.STAT, 2, s.lycFlag);
}

template <GBSystem sys>
void PPUCore<sys>::updateInterrupts()
{
	bool interrupt = s.lycFlag && LYC_STAT();

	switch (s.state)
	{
	case PPUMode::HBlank:
		interrupt |= HBlank_STAT();
		break;
	case PPUMode::VBlank:
		interrupt |= VBlank_STAT();
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
	regs.STAT &= (~0b11);
	regs.STAT |= static_cast<uint8_t>(mode);

	switch (mode)
	{
	case PPUMode::HBlank:
		s.hblankCycles = TOTAL_SCANLINE_CYCLES - OAM_SCAN_CYCLES - s.videoCycles;
		canAccessOAM = true; canAccessVRAM = true;

		if constexpr (sys == GBSystem::CGB)
		{
			if (mmu.gbc.ghdma.status == GHDMAStatus::HDMA) 
				mmu.gbc.ghdma.active = true;
		}
		break;
	case PPUMode::VBlank:
		dotsUntilVBlank = GBCore::CYCLES_PER_FRAME;
		s.vblankLineCycles = DEFAULT_VBLANK_LINE_CYCLES;
		canAccessOAM = true; canAccessVRAM = true;
		break;
	case PPUMode::OAMSearch:
		canAccessOAM = false; canAccessVRAM = true;
		break;
	case PPUMode::PixelTransfer:
		canAccessOAM = false; canAccessVRAM = false;
		resetPixelTransferState();
		break;
	}

	s.state = mode;
}

template <GBSystem sys>
void PPUCore<sys>::execute(uint8_t cycles)
{
	if (!LCDEnabled())
	{
		// LCD is not cleared immediately after being disabled. The game "Bug's Life" depends on this behavior, as it keeps disabling and enabling lcd very often.
		// I am not sure after how long exactly it gets cleared, but I am assuming its 4560 cycles (VBlank duration).

		if (s.videoCycles < TOTAL_VBLANK_CYCLES)
		{
			s.videoCycles += cycles;

			if (s.videoCycles >= TOTAL_VBLANK_CYCLES)
				clearBuffer();
		}

		return;
	}

	for (int i = 0; i < cycles; i++)
	{
		s.videoCycles++;
		dotsUntilVBlank--;

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

		checkLYC();
		updateInterrupts();
	}
}

template <GBSystem sys>
void PPUCore<sys>::handleHBlank()
{
	if (s.videoCycles >= s.hblankCycles)
	{
		s.videoCycles -= s.hblankCycles;

		// After LCD is being enabled, first hblank is "fake" and is 4 cycles shorter than oam scan, and continues to pixel transfer instead of oam scan.
		const bool isFakeHBlank = s.hblankCycles == OAM_SCAN_CYCLES - 4; 

		if (!isFakeHBlank)
		{
			s.LY++;
			if (bgFIFO.s.fetchingWindow) s.WLY++;
		}

		if constexpr (sys == GBSystem::CGB)
		{
			if (mmu.gbc.ghdma.status == GHDMAStatus::HDMA)
				mmu.gbc.ghdma.active = false;
		}

		if (s.LY == 144)
		{
			SetPPUMode(PPUMode::VBlank);
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
	if (s.videoCycles >= OAM_SCAN_CYCLES)
	{
		s.videoCycles -= OAM_SCAN_CYCLES;
		objCount = 0;

		const bool doubleObj = DoubleOBJSize();

		for (uint8_t OAMAddr = 0; OAMAddr < sizeof(OAM); OAMAddr += 4)
		{
			const int16_t objY = static_cast<int16_t>(OAM[OAMAddr]) - 16;
			const int16_t objX = static_cast<int16_t>(OAM[OAMAddr + 1]) - 8;
			const uint8_t tileInd = OAM[OAMAddr + 2];
			const uint8_t attributes = OAM[OAMAddr + 3];
			const bool yFlip = getBit(attributes, 6);

			if (!doubleObj)
			{
				if (s.LY >= objY && s.LY < objY + 8)
				{
					selectedObjects[objCount] = OAMobject{ objX, objY, static_cast<uint16_t>(tileInd * 16), attributes, OAMAddr };
					objCount++;
				}
			}
			else
			{
				if (s.LY >= objY && s.LY < objY + 8)
				{
					uint16_t tileAddr = yFlip ? ((tileInd & 0xFE) + 1) * 16 : (tileInd & 0xFE) * 16;
					selectedObjects[objCount] = OAMobject{ objX, objY, tileAddr, attributes, OAMAddr };
					objCount++;
				}
				else if (s.LY >= objY + 8 && s.LY < objY + 16)
				{
					uint16_t tileAddr = yFlip ? (tileInd & 0xFE) * 16 : ((tileInd & 0xFE) + 1) * 16;
					selectedObjects[objCount] = OAMobject{ objX, static_cast<int16_t>(objY + 8), tileAddr, attributes, OAMAddr };
					objCount++;
				}
			}

			if (objCount == 10)
				break;
		}

		std::stable_sort(selectedObjects.begin(), selectedObjects.begin() + objCount, [](const auto& a, const auto& b) { return a.X < b.X; });

		SetPPUMode(PPUMode::PixelTransfer);
	}
}

template <GBSystem sys>
void PPUCore<sys>::handleVBlank()
{
	if (s.videoCycles >= s.vblankLineCycles)
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
	latchWindowEnable = WindowEnable();
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
		if (latchWindowEnable && s.LY >= regs.WY && s.xPosCounter >= regs.WX - 7 && regs.WX != 0) [[unlikely]]
		{
			bgFIFO.reset();
			bgFIFO.s.fetchingWindow = true;
		}
	}

	if (!objFIFO.s.fetchRequested && !bgFIFO.empty())
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
	if (!objFIFO.s.fetchRequested && OBJEnable())
	{
		if (objFIFO.s.objInd < objCount && selectedObjects[objFIFO.s.objInd].X <= s.xPosCounter)
		{
			objFIFO.s.state = FetcherState::FetchTileNo; 
			objFIFO.s.fetchRequested = true;
		}
	}
}

template <GBSystem sys>
void PPUCore<sys>::executeBGFetcher()
{
	switch (bgFIFO.s.state)
	{
	case FetcherState::FetchTileNo:
		bgFIFO.s.cycles++;

		if ((bgFIFO.s.cycles & 0x1) == 0)
		{
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
		}
		break;
	case FetcherState::FetchTileDataLow:
		bgFIFO.s.cycles++;

		if ((bgFIFO.s.cycles & 0x1) == 0)
		{
			if constexpr (sys == GBSystem::CGB)
			{
				const uint8_t* bank = getBit(bgFIFO.s.cgbAttributes, 3) ? VRAM_BANK1.data() : VRAM_BANK0.data();
				bgFIFO.s.tileLow = bank[getBGTileAddr(bgFIFO.s.tileMap) + getBGTileOffset()];
			}
			else
				bgFIFO.s.tileLow = VRAM_BANK0[getBGTileAddr(bgFIFO.s.tileMap) + getBGTileOffset()];

			bgFIFO.s.state = FetcherState::FetchTileDataHigh;
		}
		break;
	case FetcherState::FetchTileDataHigh:
		bgFIFO.s.cycles++;

		if ((bgFIFO.s.cycles & 0x1) == 0)
		{
			if (bgFIFO.s.newScanline)
			{
				bgFIFO.s.fetchX--;
				bgFIFO.s.newScanline = false;
				bgFIFO.s.state = FetcherState::FetchTileNo;
				break;
			}

			if constexpr (sys == GBSystem::CGB)
			{
				const uint8_t* bank = getBit(bgFIFO.s.cgbAttributes, 3) ? VRAM_BANK1.data() : VRAM_BANK0.data();
				bgFIFO.s.tileHigh = bank[getBGTileAddr(bgFIFO.s.tileMap) + getBGTileOffset() + 1];
			}
			else
				bgFIFO.s.tileHigh = VRAM_BANK0[getBGTileAddr(bgFIFO.s.tileMap) + getBGTileOffset() + 1];

			bgFIFO.s.state = FetcherState::PushFIFO;
		}
		break;
	case FetcherState::PushFIFO: 
		if (bgFIFO.empty())
		{
			if (bgFIFO.s.scanlineDiscardPixels == -1)
				bgFIFO.s.scanlineDiscardPixels = bgFIFO.s.fetchingWindow ? (regs.WX < 7 ? 7 - regs.WX : 0) : (regs.SCX & 0x7);

			int cnt = 7;

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

				const int cntStart = (xFlip ? 0 : cnt);
				const int cntEnd = xFlip ? cnt + 1 : -1;
				const int cntStep = xFlip ? 1 : -1;

				for (int i = cntStart; i != cntEnd; i += cntStep)
					bgFIFO.push(FIFOEntry { getColorID(bgFIFO.s.tileLow, bgFIFO.s.tileHigh, i), cgbPalette, priority });
			}
			else
			{
				for (int i = cnt; i >= 0; i--)
					bgFIFO.push(FIFOEntry { getColorID(bgFIFO.s.tileLow, bgFIFO.s.tileHigh, i) });
			}

			bgFIFO.s.cycles = 0;
			bgFIFO.s.state = FetcherState::FetchTileNo;
		}

		if (objFIFO.s.fetchRequested)
			objFIFO.s.fetcherActive = true;

		break;
	}
}

template <GBSystem sys>
void PPUCore<sys>::executeObjFetcher()
{
	const auto& obj = selectedObjects[objFIFO.s.objInd];

	switch (objFIFO.s.state)
	{
	case FetcherState::FetchTileNo:
		objFIFO.s.cycles++;

		if ((objFIFO.s.cycles & 0x1) == 0)
			objFIFO.s.state = FetcherState::FetchTileDataLow;

		break;
	case FetcherState::FetchTileDataLow:
		objFIFO.s.cycles++;

		if ((objFIFO.s.cycles & 0x1) == 0)
		{
			if constexpr (sys == GBSystem::CGB)
			{
				const uint8_t* bank = getBit(obj.attributes, 3) ? VRAM_BANK1.data() : VRAM_BANK0.data();
				objFIFO.s.tileLow = bank[obj.tileAddr + getObjTileOffset(obj)];
			}
			else
				objFIFO.s.tileLow = VRAM_BANK0[obj.tileAddr + getObjTileOffset(obj)];

			objFIFO.s.state = FetcherState::FetchTileDataHigh;
		}
		break;
	case FetcherState::FetchTileDataHigh:
		objFIFO.s.cycles++;

		if ((objFIFO.s.cycles & 0x1) == 0)
		{
			if constexpr (sys == GBSystem::CGB)
			{
				const uint8_t* bank = getBit(obj.attributes, 3) ? VRAM_BANK1.data() : VRAM_BANK0.data();
				objFIFO.s.tileHigh = bank[obj.tileAddr + getObjTileOffset(obj) + 1];
			}
			else
				objFIFO.s.tileHigh = VRAM_BANK0[obj.tileAddr + getObjTileOffset(obj) + 1];

			objFIFO.s.state = FetcherState::PushFIFO;
		}
		break;
	case FetcherState::PushFIFO:
		const bool bgPriority = getBit(obj.attributes, 7);
		const bool xFlip = getBit(obj.attributes, 5);
		uint8_t palette;

		if constexpr (sys == GBSystem::CGB)
			palette = obj.attributes & 0x7;
		else
			palette = getBit(obj.attributes, 4);

		while (!objFIFO.full())
			objFIFO.push(FIFOEntry{});

		const int cntStart = (obj.X < 0) ? (xFlip ? (obj.X * -1) : (obj.X + 7)) : (xFlip ? 0 : 7);
		const int cntEnd = xFlip ? 8 : -1;
		const int cntStep = xFlip ? 1 : -1;

		for (int i = cntStart; i != cntEnd; i += cntStep)
		{
			const int fifoInd = xFlip ? (i - cntStart) : (cntStart - i);
			const uint8_t colorId = getColorID(objFIFO.s.tileLow, objFIFO.s.tileHigh, i);
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
		objFIFO.s.fetchRequested = false;

		tryStartSpriteFetcher();
		break;
	}
}

template <GBSystem sys>
void PPUCore<sys>::renderFIFOs()
{
	auto bg = bgFIFO.pop();

	if (bgFIFO.s.scanlineDiscardPixels > 0)
		bgFIFO.s.scanlineDiscardPixels--;
	else
	{
		if constexpr (sys != GBSystem::CGB)
			if (!DMGTileMapsEnable()) bg.color = 0;

		color outputColor;

		if (!objFIFO.empty())
		{
			const auto obj = objFIFO.pop();
			bool objHasPriority = obj.color != 0 && OBJEnable();

			if constexpr (sys == GBSystem::CGB)
				objHasPriority &= (bg.color == 0 || GBCMasterPriority() || (!obj.priority && !bg.priority));
			else
				objHasPriority &= (!obj.priority || bg.color == 0);

			outputColor = objHasPriority ? getColor<true>(obj.color, obj.palette) : getColor<false>(bg.color, bg.palette);

			if (debugPPU) [[unlikely]]
			{
				if (objHasPriority)
					PixelOps::setPixel(debugOAMFramebuffer.get(), SCR_WIDTH, s.xPosCounter, s.LY, getColor<true>(obj.color, obj.palette));
			}
		}
		else
			outputColor = getColor<false>(bg.color, bg.palette);

		if (debugPPU) [[unlikely]]
		{
			const auto framebuffer = bgFIFO.s.fetchingWindow ? debugWindowFramebuffer.get() : debugBGFramebuffer.get();
			PixelOps::setPixel(framebuffer, SCR_WIDTH, s.xPosCounter, s.LY, getColor<false>(bg.color, bg.palette));
		}

		setPixel(s.xPosCounter, s.LY, outputColor);
		s.xPosCounter++;
	}
}


// DEBUG


template <GBSystem sys>
void PPUCore<sys>::renderTileData(uint8_t* buffer, int vramBank)
{
	const uint8_t* vram = vramBank == 1 ? VRAM_BANK1.data() : VRAM_BANK0.data();

	for (uint16_t addr = 0; addr < 0x17FF; addr += 16)
	{
		const uint16_t tileInd = addr / 16;
		const uint16_t screenX = (tileInd % 16) * 8;
		const uint16_t screenY = (tileInd / 16) * 8;

		for (uint8_t y = 0; y < 8; y++)
		{
			const uint8_t yPos { static_cast<uint8_t>(y + screenY) };
			const uint8_t lsbLineByte { vram[addr + y * 2] };
			const uint8_t msbLineByte { vram[addr + y * 2 + 1] };

			for (int8_t x = 7; x >= 0; x--)
			{
				const uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);
				const uint8_t xPos { static_cast<uint8_t>(7 - x + screenX) };
				PixelOps::setPixel(buffer, TILES_WIDTH, xPos, yPos, PPU::ColorPalette[colorId]);
			}
		}
	}
}

template <GBSystem sys>
void PPUCore<sys>::renderTileMap(uint8_t* buffer, uint16_t tileMapAddr)
{
	for (uint16_t y = 0; y < 32; y++)
	{
		for (uint16_t x = 0; x < 32; x++)
		{
			const uint16_t tileMapInd = tileMapAddr + y * 32 + x;
			const uint8_t tileMap = VRAM_BANK0[tileMapInd];
			const uint16_t screenX = x * 8, screenY = y * 8;

			if constexpr (sys == GBSystem::CGB)
			{
				const uint8_t attributes = VRAM_BANK1[tileMapInd];

				const bool yFlip = getBit(attributes, 6);
				const bool xFlip = getBit(attributes, 5);
				const uint8_t* bank = getBit(attributes, 3) ? VRAM_BANK1.data() : VRAM_BANK0.data();
				const uint8_t cgbPalette = attributes & 0x7;

				const int8_t yStart = (yFlip ? 7 : 0), yEnd = (yFlip ? -1 : 8), yStep = (yFlip ? -1 : 1);

				for (int8_t tileY = yStart; tileY != yEnd; tileY += yStep)
				{
					const uint8_t yPos { static_cast<uint8_t>(tileY + screenY) };
					const uint8_t lsbLineByte{ bank[getBGTileAddr(tileMap) + tileY * 2] };
					const uint8_t msbLineByte{ bank[getBGTileAddr(tileMap) + tileY * 2 + 1] };

					const int8_t xStart = (xFlip ? 0 : 7), xEnd = (xFlip ? 8 : -1), xStep = (xFlip ? 1 : -1);

					for (int8_t tileX = xStart; tileX != xEnd; tileX += xStep)
					{
						const uint8_t colorId = getColorID(lsbLineByte, msbLineByte, tileX);
						const uint8_t xPos{ static_cast<uint8_t>(7 - tileX + screenX) };
						PixelOps::setPixel(buffer, TILEMAP_WIDTH, xPos, yPos, getColor<false>(colorId, cgbPalette));
					}
				}
			}
			else
			{
				for (uint8_t tileY = 0; tileY < 8; tileY++)
				{
					const uint8_t yPos { static_cast<uint8_t>(tileY + screenY) };
					const uint8_t lsbLineByte { VRAM_BANK0[getBGTileAddr(tileMap) + tileY * 2] };
					const uint8_t msbLineByte { VRAM_BANK0[getBGTileAddr(tileMap) + tileY * 2 + 1] };

					for (int8_t tileX = 7; tileX >= 0; tileX--)
					{
						const uint8_t colorId = getColorID(lsbLineByte, msbLineByte, tileX);
						const uint8_t xPos{ static_cast<uint8_t>(7 - tileX + screenX) };
						PixelOps::setPixel(buffer, TILEMAP_WIDTH, xPos, yPos, getColor<false>(colorId, 0));
					}
				}
			}
		}
	}
}