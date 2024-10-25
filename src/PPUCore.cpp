#include "PPUCore.h"
#include "Utils/bitOps.h"
#include <iostream>
#include <algorithm>

template class PPUCore<GBSystem::DMG>;
template class PPUCore<GBSystem::GBC>;

template <GBSystem sys>
void PPUCore<sys>::reset()
{
	std::memset(OAM.data(), 0, sizeof(OAM));
	std::memset(VRAM_BANK0.data(), 0, sizeof(VRAM_BANK0));
	VRAM = VRAM_BANK0.data();
	
	if constexpr (sys == GBSystem::GBC)
	{
		std::memset(VRAM_BANK1.data(), 0, sizeof(VRAM_BANK1));
		std::memset(OBPpaletteRAM.data(), 255, sizeof(OBPpaletteRAM));
		std::memset(BGpaletteRAM.data(), 255, sizeof(BGpaletteRAM));
	}
	else if constexpr (sys == GBSystem::DMG)
		updatePalette(regs.BGP, BGpalette);

	s = {};
	regs = {};
	gbcRegs = {};

	clearBuffer();
	SetPPUMode(PPUMode::VBlank);
	s.LY = 144;
}

template <GBSystem sys>
void PPUCore<sys>::setLCDEnable(bool val)
{
	if (val)
		s.lcdWasEnabled = true;
	else
	{
		s.LY = 0;
		s.WLY = 0;
		clearBuffer();
		SetPPUMode(PPUMode::HBlank);
	}
}

template <GBSystem sys>
void PPUCore<sys>::saveState(std::ofstream& st)
{
	ST_WRITE(regs);
	ST_WRITE(s);

	if constexpr (sys == GBSystem::GBC)
	{
		ST_WRITE(gbcRegs);
		ST_WRITE_ARR(VRAM_BANK1);
		ST_WRITE_ARR(BGpaletteRAM);
		ST_WRITE_ARR(OBPpaletteRAM);
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
void PPUCore<sys>::loadState(std::ifstream& st)
{
	ST_READ(regs);
	ST_READ(s);

	if constexpr (sys == GBSystem::GBC)
	{
		ST_READ(gbcRegs);
		ST_READ_ARR(VRAM_BANK1);
		ST_READ_ARR(BGpaletteRAM);
		ST_READ_ARR(OBPpaletteRAM);
	}
	else if constexpr (sys == GBSystem::DMG)
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
	}
	if (s.state == PPUMode::OAMSearch || s.state == PPUMode::PixelTransfer)
	{
		ST_READ(objCount);
		st.read(reinterpret_cast<char*>(selectedObjects.data()), sizeof(selectedObjects[0]) * objCount);
	}

	if (!LCDEnabled())
		clearBuffer();
}

template <GBSystem sys>
void PPUCore<sys>::refreshDMGScreenColors(const std::array<color, 4>& newColorPalette)
{
	if constexpr (sys != GBSystem::DMG) 
		return;

	for (uint8_t x = 0; x < SCR_WIDTH; x++)
	{
		for (uint8_t y = 0; y < SCR_HEIGHT; y++)
		{
			color pixel = getPixel(x, y);
			uint8_t pixelInd { static_cast<uint8_t>(std::find(PPU::ColorPalette.begin(), PPU::ColorPalette.end(), pixel) - PPU::ColorPalette.begin()) };
			setPixel(x, y, newColorPalette[pixelInd]);
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
void PPUCore<sys>::requestSTAT()
{
	if (!s.blockStat)
	{
		s.blockStat = true;
		cpu.requestInterrupt(Interrupt::STAT);
	}
}

template <GBSystem sys>
void PPUCore<sys>::updateInterrupts()
{
	bool interrupt = s.lycFlag && LYC_STAT();

	if (!interrupt)
	{
		switch (s.state)
		{
		case PPUMode::HBlank:
			if (HBlank_STAT()) interrupt = true;
			break;
		case PPUMode::VBlank:
			if (VBlank_STAT()) interrupt = true;
			break;
		case PPUMode::OAMSearch:
			if (OAM_STAT()) interrupt = true;
			break;
		default:
			break;
		}
	}

	if (interrupt)
		requestSTAT();
	else
		s.blockStat = false;
}

template <GBSystem sys>
void PPUCore<sys>::SetPPUMode(PPUMode PPUState)
{
	regs.STAT = setBit(regs.STAT, 1, static_cast<uint8_t>(PPUState) & 0x2);
	regs.STAT = setBit(regs.STAT, 0, static_cast<uint8_t>(PPUState) & 0x1);

	switch (PPUState)
	{
	case PPUMode::HBlank:
		s.HBLANK_CYCLES = TOTAL_SCANLINE_CYCLES - OAM_SCAN_CYCLES - s.videoCycles;
		canAccessOAM = true; canAccessVRAM = true;

		if constexpr (sys == GBSystem::GBC)
		{
			if (mmu.gbc.ghdma.status == GHDMAStatus::HDMA) 
				mmu.gbc.ghdma.active = true;
		}
		break;
	case PPUMode::VBlank:
		s.VBLANK_CYCLES = DEFAULT_VBLANK_CYCLES;
		canAccessOAM = true; canAccessVRAM = true;
		break;
	case PPUMode::OAMSearch:
		canAccessOAM = false;
		break;
	case PPUMode::PixelTransfer:
		canAccessVRAM = false;
		resetPixelTransferState();
		break;
	}

	s.state = PPUState;
	s.videoCycles = 0;
}

template <GBSystem sys>
void PPUCore<sys>::execute(uint8_t cycles)
{
	if (!LCDEnabled()) return;

	for (int i = 0; i < cycles; i++)
	{
		s.videoCycles++;

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
	if constexpr (sys == GBSystem::GBC)
	{
		if (s.videoCycles == MMU::GHDMA_BLOCK_CYCLES)
			mmu.gbc.ghdma.active = false;
	}

	if (s.videoCycles >= s.HBLANK_CYCLES)
	{
		s.LY++;
		if (bgFIFO.s.fetchingWindow) s.WLY++;

		if (s.LY == 144)
		{
			SetPPUMode(PPUMode::VBlank);
			cpu.requestInterrupt(Interrupt::VBlank);

			// First frame after enabling LCD is blank.
			if (s.lcdWasEnabled)
			{
				s.lcdWasEnabled = false;
				clearBuffer();
			}
			else 
				invokeDrawCallback();
		}
		else
			SetPPUMode(PPUMode::OAMSearch);
	}
}

template <GBSystem sys>
void PPUCore<sys>::handleOAMSearch()
{
	if (s.videoCycles >= OAM_SCAN_CYCLES)
	{
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
	if (s.videoCycles >= s.VBLANK_CYCLES)
	{
		s.LY++;
		s.videoCycles -= s.VBLANK_CYCLES;

		switch (s.LY)
		{
		case 153:
			s.VBLANK_CYCLES	= 4;
			break;
		case 154:
			s.VBLANK_CYCLES = 452;
			s.LY = 0;
			s.WLY = 0;
			break;
		case 1:
			s.LY = 0;
			SetPPUMode(PPUMode::OAMSearch);
			break;
		default:
			s.VBLANK_CYCLES = DEFAULT_VBLANK_CYCLES;
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
		if (WindowEnable() && s.LY >= regs.WY && s.xPosCounter >= regs.WX - 7 && regs.WX != 0)
		{
			bgFIFO.reset();
			bgFIFO.s.fetchingWindow = true;
		}
	}

	if (!objFIFO.s.fetchRequested && !bgFIFO.empty())
		renderFIFOs();
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
				uint16_t yOffset = (static_cast<uint8_t>(s.LY + regs.SCY) / 8) * 32;
				uint8_t xOffset = (bgFIFO.s.fetchX + (regs.SCX / 8)) & 0x1F;
				tileMapInd = BGTileMapAddr() + ((yOffset + xOffset) & 0x3FF);
			}
			else
			{
				uint16_t yOffset = (s.WLY / 8) * 32;
				uint8_t xOffset = bgFIFO.s.fetchX & 0x1F;
				tileMapInd = WindowTileMapAddr() + ((yOffset + xOffset) & 0x3FF);
			}

			bgFIFO.s.tileMap = VRAM_BANK0[tileMapInd];

			if constexpr (sys == GBSystem::GBC)
				bgFIFO.s.cgbAttributes = VRAM_BANK1[tileMapInd];

			bgFIFO.s.fetchX++;
			bgFIFO.s.state = FetcherState::FetchTileDataLow;
		}
		break;
	case FetcherState::FetchTileDataLow:
		bgFIFO.s.cycles++;

		if ((bgFIFO.s.cycles & 0x1) == 0)
		{
			if constexpr (sys == GBSystem::GBC)
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
		if (bgFIFO.s.newScanline)
		{
			bgFIFO.s.fetchX--;
			bgFIFO.s.newScanline = false;
			bgFIFO.s.state = FetcherState::FetchTileNo;
			break;
		}

		bgFIFO.s.cycles++;

		if ((bgFIFO.s.cycles & 0x1) == 0)
		{
			if constexpr (sys == GBSystem::GBC)
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

			if constexpr (sys == GBSystem::GBC)
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
			if constexpr (sys == GBSystem::GBC)
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
			if constexpr (sys == GBSystem::GBC)
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

		if constexpr (sys == GBSystem::DMG)
			palette = getBit(obj.attributes, 4);
		else
			palette = obj.attributes & 0x7;

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

			if constexpr (sys == GBSystem::DMG)
				overwriteObj = objFIFO[fifoInd].color == 0;
			else
				overwriteObj = colorId != 0 && (objFIFO[fifoInd].color == 0 || selectedObjects[objFIFO.s.objInd].oamAddr < selectedObjects[objFIFO.s.objInd - 1].oamAddr);

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
		if constexpr (sys == GBSystem::DMG)
			if (!DMGTileMapsEnable()) bg.color = 0;

		color outputColor;

		if (!objFIFO.empty())
		{
			const auto obj = objFIFO.pop();
			bool objHasPriority = obj.color != 0 && OBJEnable();

			if constexpr (sys == GBSystem::DMG)
				objHasPriority &= (!obj.priority || bg.color == 0);
			else
				objHasPriority &= (bg.color == 0 || GBCMasterPriority() || (!obj.priority && !bg.priority));

			outputColor = objHasPriority ? getColor<true>(obj.color, obj.palette) : getColor<false>(bg.color, bg.palette);
		}
		else
			outputColor = getColor<false>(bg.color, bg.palette);

		PixelOps::setPixel(framebuffer.data(), SCR_WIDTH, s.xPosCounter, s.LY, outputColor);
		s.xPosCounter++;

		if (s.xPosCounter == SCR_WIDTH) [[unlikely]]
			SetPPUMode(PPUMode::HBlank);
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
			uint16_t tileMapInd = tileMapAddr + y * 32 + x;
			uint8_t tileMap = VRAM_BANK0[tileMapInd];

			uint16_t screenX = x * 8;
			uint16_t screenY = y * 8;

			if constexpr (sys == GBSystem::GBC)
			{
				const uint8_t attributes = VRAM_BANK1[tileMapInd];

				const bool yFlip = getBit(attributes, 6);
				const bool xFlip = getBit(attributes, 5);
				const uint8_t* bank = getBit(attributes, 3) ? VRAM_BANK1.data() : VRAM_BANK0.data();
				const uint8_t cgbPalette = attributes & 0x7;

				const int8_t yStart = (yFlip ? 7 : 0);
				const int8_t yEnd = yFlip ? -1 : 8;
				const int8_t yStep = yFlip ? -1 : 1;

				for (int8_t tileY = yStart; tileY != yEnd; tileY += yStep)
				{
					uint8_t yPos{ static_cast<uint8_t>(tileY + screenY) };
					uint8_t lsbLineByte{ bank[getBGTileAddr(tileMap) + tileY * 2] };
					uint8_t msbLineByte{ bank[getBGTileAddr(tileMap) + tileY * 2 + 1] };

					const int8_t xStart = (xFlip ? 0 : 7);
					const int8_t xEnd = xFlip ? 8 : -1;
					const int8_t xStep = xFlip ? 1 : -1;

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
					const uint8_t yPos{ static_cast<uint8_t>(tileY + screenY) };
					const uint8_t lsbLineByte{ VRAM_BANK0[getBGTileAddr(tileMap) + tileY * 2] };
					const uint8_t msbLineByte{ VRAM_BANK0[getBGTileAddr(tileMap) + tileY * 2 + 1] };

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