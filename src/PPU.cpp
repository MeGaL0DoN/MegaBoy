#include "PPU.h"
#include "Utils/bitOps.h"
#include <iostream>
#include <algorithm>
#include <optional>

void PPU::reset()
{
	std::memset(OAM.data(), 0, sizeof(OAM));
	std::memset(VRAM_BANK0.data(), 0, sizeof(VRAM_BANK0));
	VRAM = VRAM_BANK0.data();
	
	if (System::Current() == GBSystem::GBC)
	{
		std::memset(VRAM_BANK1.data(), 0, sizeof(VRAM_BANK1));
		std::memset(OBPpaletteRAM.data(), 255, sizeof(OBPpaletteRAM));
		std::memset(BGpaletteRAM.data(), 255, sizeof(BGpaletteRAM));
	}

	s = {};
	regs = {};
	gbcRegs = {};

	disableLCD(PPUMode::VBlank);
	s.LY = 144;

	updatePalette(regs.BGP, BGpalette);
	updateFunctionPointers();
}
void PPU::disableLCD(PPUMode mode)
{
	s.LY = 0;
	s.WLY = 0;
	s.videoCycles = 0;
	clearBuffer();
	SetPPUMode(mode);
}

void PPU::updateFunctionPointers()
{
	if (System::Current() == GBSystem::DMG)
	{
		renderObjTileFunc = &PPU::DMGrenderObjTile;
		renderBGTileFunc = &PPU::DMGrenderBGTile<false>;
		renderWinTileFunc = &PPU::DMGrenderBGTile<true>;
	}
	else
	{
		renderObjTileFunc = &PPU::GBCrenderObjTile;
		renderBGTileFunc = &PPU::GBCrenderBGTile<false>;
		renderWinTileFunc = &PPU::GBCrenderBGTile<true>;
	}
}

void PPU::saveState(std::ofstream& st)
{
	ST_WRITE(regs);
	ST_WRITE(s);

	if (System::Current() == GBSystem::GBC)
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

void PPU::loadState(std::ifstream& st)
{
	ST_READ(regs);
	ST_READ(s);

	if (System::Current() == GBSystem::GBC)
	{
		ST_READ(gbcRegs);
		ST_READ_ARR(VRAM_BANK1);
		ST_READ_ARR(BGpaletteRAM);
		ST_READ_ARR(OBPpaletteRAM);
	}
	else if (System::Current() == GBSystem::DMG)
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

void PPU::updatePalette(uint8_t val, std::array<uint8_t, 4>& palette)
{
	for (uint8_t i = 0; i < 4; i++)
		palette[i] = (getBit(val, i * 2 + 1) << 1) | getBit(val, i * 2);
}

void PPU::updateDMGScreenColors(const std::array<color, 4>& newColors)
{
	if (System::Current() != GBSystem::DMG) 
		return;

	for (uint8_t x = 0; x < SCR_WIDTH; x++)
	{
		for (uint8_t y = 0; y < SCR_HEIGHT; y++)
		{
			color pixel = getPixel(x, y);
			uint8_t pixelInd { static_cast<uint8_t>(std::find(colors.begin(), colors.end(), pixel) - colors.begin()) };
			setPixel(x, y, newColors[pixelInd]);
		}
	}
}

void PPU::checkLYC()
{
	s.lycFlag = s.LY == regs.LYC;
	regs.STAT = setBit(regs.STAT, 2, s.lycFlag);
}

void PPU::requestSTAT()
{
	if (!s.blockStat)
	{
		s.blockStat = true;
		cpu.requestInterrupt(Interrupt::STAT);
	}
}

void PPU::updateInterrupts()
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
		}
	}

	if (interrupt)
		requestSTAT();
	else
		s.blockStat = false;
}

void PPU::SetPPUMode(PPUMode ppuState)
{
	regs.STAT = setBit(regs.STAT, 1, static_cast<uint8_t>(ppuState) & 0x2);
	regs.STAT = setBit(regs.STAT, 0, static_cast<uint8_t>(ppuState) & 0x1);

	switch (ppuState)
	{
	case PPUMode::HBlank:
		s.HBLANK_CYCLES = TOTAL_SCANLINE_CYCLES - OAM_SCAN_CYCLES - s.videoCycles;
		canAccessOAM = true; canAccessVRAM = true;
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

	s.state = ppuState;
	s.videoCycles = 0;
}

void PPU::execute()
{
	if (!LCDEnabled()) return;
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

void PPU::handleHBlank()
{
	if (s.videoCycles >= s.HBLANK_CYCLES)
	{
		s.LY++;
		if (bgFIFO.fetchingWindow) s.WLY++;

		//updatedOAMPixels.clear();
		//updatedWindowPixels.clear();

		if (s.LY == 144)
		{
			SetPPUMode(PPUMode::VBlank);
			cpu.requestInterrupt(Interrupt::VBlank);
			invokeDrawCallback();
		}
		else
			SetPPUMode(PPUMode::OAMSearch);
	}
}
void PPU::handleVBlank()
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

void PPU::resetPixelTransferState()
{
	bgFIFO.reset();
	objFIFO.reset();
	s.xPosCounter = 0;
	s.scanlineDiscardPixels = -1;
	s.objFetcherActive = false;
}

void PPU::tryStartSpriteFetcher()
{
	if (!s.objFetcherActive && OBJEnable())
	{
		if (objFIFO.objInd < objCount && selectedObjects[objFIFO.objInd].X <= s.xPosCounter)
		{
			objFIFO.state = FetcherState::FetchTileNo; 
			s.objFetcherActive = true;
		}
	}
}

void PPU::executeBGFetcher()
{
	switch (bgFIFO.state)
	{
	case FetcherState::FetchTileNo:
		bgFIFO.cycles++;

		if ((bgFIFO.cycles & 0x1) == 0)
		{
			if (!bgFIFO.fetchingWindow)
			{
				uint16_t yOffset = (static_cast<uint8_t>(s.LY + regs.SCY) / 8) * 32;
				uint8_t xOffset = (bgFIFO.fetchX + (regs.SCX / 8)) & 0x1F;
				bgFIFO.tileMap = VRAM_BANK0[BGTileMapAddr() + ((yOffset + xOffset) & 0x3FF)];
			}
			else
			{
				uint16_t yOffset = (s.WLY / 8) * 32;
				uint8_t xOffset = bgFIFO.fetchX & 0x1F;
				bgFIFO.tileMap = VRAM_BANK0[WindowTileMapAddr() + ((yOffset + xOffset) & 0x3FF)];
			}

			bgFIFO.fetchX++;
			bgFIFO.state = FetcherState::FetchTileDataLow;
		}
		break;
	case FetcherState::FetchTileDataLow:
		bgFIFO.cycles++;

		if ((bgFIFO.cycles & 0x1) == 0)
		{
			bgFIFO.tileLow = VRAM_BANK0[getBGTileAddr(bgFIFO.tileMap) + getBGTileOffset()];
			bgFIFO.state = FetcherState::FetchTileDataHigh;
		}
		break;
	case FetcherState::FetchTileDataHigh:
		if (bgFIFO.newScanline)
		{
			bgFIFO.fetchX--;
			bgFIFO.newScanline = false;
			bgFIFO.state = FetcherState::FetchTileNo;
			break;
		}

		bgFIFO.cycles++;

		if ((bgFIFO.cycles & 0x1) == 0)
		{
			bgFIFO.tileHigh = VRAM_BANK0[getBGTileAddr(bgFIFO.tileMap) + getBGTileOffset() + 1];
			bgFIFO.state = FetcherState::PushFIFO;
		}
		break;
	case FetcherState::PushFIFO:
		if (bgFIFO.empty())
		{
			for (int i = 7; i >= 0; i--)
			{
				uint8_t color = (getBit(bgFIFO.tileHigh, i) << 1) | getBit(bgFIFO.tileLow, i);
				bgFIFO.push(color);
			}

			if (s.scanlineDiscardPixels == -1)
				s.scanlineDiscardPixels = (regs.SCX % 8);

			bgFIFO.cycles = 0;
			bgFIFO.state = FetcherState::FetchTileNo;
		}
		break;
	}
}

void PPU::executeObjFetcher()
{
	const auto& obj = selectedObjects[objFIFO.objInd];

	switch (objFIFO.state)
	{
	case FetcherState::FetchTileNo:
		objFIFO.cycles++;

		if ((objFIFO.cycles & 0x1) == 0)
			objFIFO.state = FetcherState::FetchTileDataLow;

		break;
	case FetcherState::FetchTileDataLow:
		objFIFO.cycles++;

		if ((objFIFO.cycles & 0x1) == 0)
		{
			objFIFO.tileLow = VRAM_BANK0[obj.tileAddr + getObjTileOffset(obj)];
			objFIFO.state = FetcherState::FetchTileDataHigh;
		}
		break;
	case FetcherState::FetchTileDataHigh:
		objFIFO.cycles++;

		if ((objFIFO.cycles & 0x1) == 0)
		{
			objFIFO.tileHigh = VRAM_BANK0[obj.tileAddr + getObjTileOffset(obj) + 1];
			objFIFO.state = FetcherState::PushFIFO;
		}
		break;
	case FetcherState::PushFIFO:
		const bool bgPriority = getBit(obj.attributes, 7);
		const bool xFlip = getBit(obj.attributes, 5);
		const uint8_t palette = getBit(obj.attributes, 4);

		while (!objFIFO.full())
			objFIFO.push(ObjFIFOEntry{});

		int cntStart = (obj.X < 0) ? (xFlip ? (obj.X * -1) : (obj.X + 7)) : (xFlip ? 0 : 7);
		int cntEnd = xFlip ? 8 : -1;
		int cntStep = xFlip ? 1 : -1;

		for (int i = cntStart; i != cntEnd; i += cntStep)
		{
			int fifoInd = xFlip ? (i - cntStart) : (cntStart - i);

			if (objFIFO[fifoInd].color == 0)
			{
				uint8_t color = (getBit(objFIFO.tileHigh, i) << 1) | getBit(objFIFO.tileLow, i);
				objFIFO[fifoInd] = ObjFIFOEntry{ color, palette, bgPriority, false };
			}
		}

		objFIFO.objInd++;
		s.objFetcherActive = false;

		tryStartSpriteFetcher();
		break;
	}
}

void PPU::renderFIFOs()
{
	uint8_t bgColorInd = bgFIFO.pop();

	if (s.scanlineDiscardPixels > 0)
		s.scanlineDiscardPixels--;
	else
	{
		std::optional<ObjFIFOEntry> objFifoEntry = objFIFO.empty() ? std::nullopt : std::make_optional(objFIFO.pop());

		if (!TileMapsEnable()) bgColorInd = 0;

		color outputColor;

		if (objFifoEntry.has_value())
		{
			const auto obj = objFifoEntry.value();
			const auto& objPalette = obj.palette == 0 ? OBP0palette : OBP1palette;

			outputColor = (obj.color != 0 && (!obj.bgPriority || bgColorInd == 0)) ?
				getColor(objPalette[obj.color]) : getColor(BGpalette[bgColorInd]);
		}
		else
			outputColor = getColor(BGpalette[bgColorInd]);

		PixelOps::setPixel(framebuffer.data(), SCR_WIDTH, s.xPosCounter, s.LY, outputColor);
		s.xPosCounter++;

		if (s.xPosCounter == SCR_WIDTH)
			SetPPUMode(PPUMode::HBlank);
		else
		{
			if (!bgFIFO.fetchingWindow)
			{
				if (WindowEnable() && s.xPosCounter >= regs.WX - 7 && s.LY >= regs.WY)
				{
					bgFIFO.fetchingWindow = true;
					bgFIFO.clear();
					bgFIFO.state = FetcherState::FetchTileNo;
					bgFIFO.fetchX = 0;
				}
			}
		}
	}
}

void PPU::handlePixelTransfer()
{
	tryStartSpriteFetcher();

	if (s.objFetcherActive)
		executeObjFetcher();
	else
		executeBGFetcher();

	if (!s.objFetcherActive && !bgFIFO.empty())
		renderFIFOs();
}

void PPU::handleOAMSearch()
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
					selectedObjects[objCount] = OAMobject { objX, objY, static_cast<uint16_t>(tileInd * 16), attributes };
					objCount++;
				}
			}
			else
			{
				if (s.LY >= objY && s.LY < objY + 8)
				{
					uint16_t tileAddr = yFlip ? ((tileInd & 0xFE) + 1) * 16 : (tileInd & 0xFE) * 16;
					selectedObjects[objCount] = OAMobject { objX, objY, tileAddr, attributes };
					objCount++;
				}
				else if (s.LY >= objY + 8 && s.LY < objY + 16)
				{
					uint16_t tileAddr = yFlip ? (tileInd & 0xFE) * 16 : ((tileInd & 0xFE) + 1) * 16;
					selectedObjects[objCount] = OAMobject { objX, static_cast<int16_t>(objY + 8), tileAddr, attributes};
					objCount++;
				}
			}

			if (objCount == 10)
				break;
		}

		//if (System::Current() == GBSystem::DMG)
		//	std::sort(selectedObjects.begin(), selectedObjects.begin() + objCount, OAMobject::ObjCompare);
		//else
		//	std::reverse(selectedObjects.begin(), selectedObjects.begin() + objCount);

		std::stable_sort(selectedObjects.begin(), selectedObjects.begin() + objCount, [](const auto& a, const auto& b) { return a.X < b.X; });

		SetPPUMode(PPUMode::PixelTransfer);
	}
}

void PPU::renderOAM()
{
	for (int i = 0; i < objCount; i++)
		(this->*renderObjTileFunc)(selectedObjects[i]);

	if (onOAMRender != nullptr)
		onOAMRender(framebuffer.data(), updatedOAMPixels, s.LY);
}

void PPU::renderScanLine()
{
	if (TileMapsEnable())
	{
		renderBackground(); 
		if (WindowEnable()) renderWindow();
	}
	else
		DMGrenderBlank();

	if (OBJEnable()) renderOAM();
}

void PPU::DMGrenderBlank()
{
	for (uint8_t x = 0; x < SCR_WIDTH; x++)
	{
		setPixel(x, s.LY, getColor(BGpalette[0]));
		bgPixelFlags[x].opaque = true;
	}

	if (onBackgroundRender != nullptr)
		onBackgroundRender(framebuffer.data(), s.LY);
}

void PPU::renderBackground()
{
	uint16_t bgTileAddr = BGTileMapAddr();
	uint16_t tileYInd = (static_cast<uint8_t>(s.LY + regs.SCY) / 8) * 32;

	for (uint8_t tileX = 0; tileX < 21; tileX++)
	{
		uint16_t tileXInd = (tileX + regs.SCX / 8) % 32; 
		uint16_t bgMapInd = bgTileAddr + tileYInd + tileXInd;
		(this->*renderBGTileFunc)(bgMapInd, (tileX * 8 - regs.SCX % 8), regs.SCY);
	}

	if (onBackgroundRender != nullptr)
		onBackgroundRender(framebuffer.data(), s.LY);
}
void PPU::renderWindow()
{
	int16_t wx = static_cast<int16_t>(regs.WX) - 7;
	if (s.LY < regs.WY || wx >= SCR_WIDTH) return;

	uint16_t winTileAddr { WindowTileMapAddr() };
	uint16_t tileYInd {static_cast<uint16_t>(s.WLY / 8 * 32) };
	uint8_t winTileXEnd{ static_cast<uint8_t>(32 - (wx / 8)) };

	for (uint8_t tileX = 0; tileX < winTileXEnd; tileX++)
	{
		uint16_t winMapInd = winTileAddr + tileYInd + tileX;
		(this->*renderWinTileFunc)(winMapInd, wx + tileX * 8, s.WLY - s.LY);
	};

	s.WLY++;

	if (onWindowRender != nullptr)
		onWindowRender(framebuffer.data(), updatedWindowPixels, s.LY);
}

template void PPU::GBCrenderBGTile<true>(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY);
template void PPU::GBCrenderBGTile<false>(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY);

template <bool updateWindowChangesBuffer>
void PPU::GBCrenderBGTile(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY)
{
	const uint8_t attributes = VRAM_BANK1[tileMapInd];

	const uint8_t cgbPalette = attributes & 0x7;
	const bool BGpriority = getBit(attributes, 7);
	const bool xFlip = getBit(attributes, 5);
	const bool yFlip = getBit(attributes, 6);
	const uint8_t* bank = getBit(attributes, 3) ? VRAM_BANK1.data() : VRAM_BANK0.data();

	const uint8_t lineOffset = 2 * (yFlip ? (7 - (s.LY + scrollY) % 8) : ((s.LY + scrollY) % 8)); 
	const uint16_t tileAddr = getBGTileAddr(VRAM_BANK0[tileMapInd]);

	const uint8_t lsbLineByte = bank[tileAddr + lineOffset];
	const uint8_t msbLineByte = bank[tileAddr + lineOffset + 1];

	for (int8_t x = 7; x >= 0; x--)
	{
		int16_t xPos = xFlip ? x + screenX : 7 - x + screenX;

		if (xPos >= 0 && xPos < SCR_WIDTH)
		{
			const uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);
			const uint8_t paletteRAMInd = cgbPalette * 8 + colorId * 2;
			const uint16_t rgb5 = BGpaletteRAM[paletteRAMInd + 1] << 8 | BGpaletteRAM[paletteRAMInd];

			setPixel(static_cast<uint8_t>(xPos), s.LY, color::fromRGB5(rgb5));
			bgPixelFlags[xPos].opaque = (colorId == 0);
			bgPixelFlags[xPos].gbcPriority = BGpriority;
			if constexpr (updateWindowChangesBuffer) updatedWindowPixels.push_back(static_cast<uint8_t>(xPos));
		}
	}
}

void PPU::GBCrenderObjTile(const OAMobject& obj)
{
	const bool xFlip = getBit(obj.attributes, 5);
	const bool yFlip = getBit(obj.attributes, 6);
	const uint8_t cgbPalette = obj.attributes & 0x7;
	const uint8_t priority = getBit(obj.attributes, 7);

	const uint8_t* bank = getBit(obj.attributes, 3) ? VRAM_BANK1.data() : VRAM_BANK0.data();

	if (obj.X < SCR_WIDTH && obj.X > -8)
	{
		const uint8_t lineOffset{ static_cast<uint8_t>(2 * (yFlip ? (obj.Y - s.LY + 7) : (8 - (obj.Y - s.LY + 8)))) };
		const uint8_t lsbLineByte{ bank[obj.tileAddr + lineOffset] };
		const uint8_t msbLineByte{ bank[obj.tileAddr + lineOffset + 1] };

		for (int8_t x = 7; x >= 0; x--)
		{
			int16_t xPos = xFlip ? x + obj.X : 7 - x + obj.X;

			if (xPos >= 0 && xPos < SCR_WIDTH)
			{
				uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);

				if (colorId != 0 && (bgPixelFlags[xPos].opaque || GBCMasterPriority() || (!priority && !bgPixelFlags[xPos].gbcPriority)))
				{
					uint8_t paletteRAMInd = cgbPalette * 8 + colorId * 2;
					uint16_t rgb5 = OBPpaletteRAM[paletteRAMInd + 1] << 8 | OBPpaletteRAM[paletteRAMInd];

					setPixel(static_cast<uint8_t>(xPos), s.LY, color::fromRGB5(rgb5));
					updatedOAMPixels.push_back(static_cast<uint8_t>(xPos));
				}
			}
		}
	}
}

template void PPU::DMGrenderBGTile<true>(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY);
template void PPU::DMGrenderBGTile<false>(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY);

template <bool updateWindowChangesBuffer>
void PPU::DMGrenderBGTile(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY)
{
	const uint8_t lineOffset = 2 * ((s.LY + scrollY) % 8);
	const uint16_t tileAddr = getBGTileAddr(VRAM_BANK0[tileMapInd]);

	const uint8_t lsbLineByte = VRAM_BANK0[tileAddr + lineOffset];
	const uint8_t msbLineByte = VRAM_BANK0[tileAddr + lineOffset + 1];

	for (int8_t x = 7; x >= 0; x--)
	{
		int16_t xPos = 7 - x + screenX;

		if (xPos >= 0 && xPos < SCR_WIDTH)
		{
			uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);
			setPixel(static_cast<uint8_t>(xPos), s.LY, getColor(BGpalette[colorId]));

			bgPixelFlags[xPos].opaque = (colorId == 0);
			if constexpr (updateWindowChangesBuffer) updatedWindowPixels.push_back(static_cast<uint8_t>(xPos));
		}
	}
}

void PPU::DMGrenderObjTile(const OAMobject& obj)
{
	const bool xFlip = getBit(obj.attributes, 5);
	const bool yFlip = getBit(obj.attributes, 6);
	const auto& palette = getBit(obj.attributes, 4) ? OBP1palette : OBP0palette;
	const uint8_t priority = getBit(obj.attributes, 7);

	if (obj.X < SCR_WIDTH && obj.X > -8)
	{
		const uint8_t lineOffset{ static_cast<uint8_t>(2 * (yFlip ? (obj.Y - s.LY + 7) : (8 - (obj.Y - s.LY + 8)))) };
		const uint8_t lsbLineByte{ VRAM_BANK0[obj.tileAddr + lineOffset] };
		const uint8_t msbLineByte{ VRAM_BANK0[obj.tileAddr + lineOffset + 1] };

		for (int8_t x = 7; x >= 0; x--)
		{
			int16_t xPos = xFlip ? x + obj.X : 7 - x + obj.X;

			if (xPos >= 0 && xPos < SCR_WIDTH)
			{
				uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);

				if (colorId != 0 && (priority == 0 || bgPixelFlags[xPos].opaque))
				{
					auto color = getColor(palette[colorId]);
					setPixel(static_cast<uint8_t>(xPos), s.LY, color);
					updatedOAMPixels.push_back(static_cast<uint8_t>(xPos));
				}
			}
		}
	}
}

void PPU::renderTileData(uint8_t* buffer, int vramBank)
{
	if (!buffer)
		return;

	const uint8_t* vram = vramBank == 1 ? VRAM_BANK1.data() : VRAM_BANK0.data();

	for (uint16_t addr = 0; addr < 0x17FF; addr += 16)
	{
		uint16_t tileInd = addr / 16;
		uint16_t screenX = (tileInd % 16) * 8;
		uint16_t screenY = (tileInd / 16) * 8;

		for (uint8_t y = 0; y < 8; y++)
		{
			uint8_t yPos { static_cast<uint8_t>(y + screenY) };
			uint8_t lsbLineByte { vram[addr + y * 2] };
			uint8_t msbLineByte { vram[addr + y * 2 + 1] };

			for (int8_t x = 7; x >= 0; x--)
			{
				uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);
				uint8_t xPos { static_cast<uint8_t>(7 - x + screenX) };
				PixelOps::setPixel(buffer, TILES_WIDTH, xPos, yPos, colors[colorId]);
			}
		}
	}
}