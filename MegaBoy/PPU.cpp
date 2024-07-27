#include "PPU.h"
#include "bitOps.h"
#include <iostream>
#include <algorithm>

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

	disableLCD(PPUMode::OAMSearch);
	updatePalette(regs.BGP, BGpalette);

	canAccessOAM = false;
	canAccessVRAM = true;

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
		renderObjTileFunc = &PPU::DMG_renderObjTile;
		renderBGTileFunc = &PPU::DMG_renderBGTile<false>;
		renderWinTileFunc = &PPU::DMG_renderBGTile<true>;
	}
	else
	{
		renderObjTileFunc = &PPU::GBC_renderObjTile;
		renderBGTileFunc = &PPU::GBC_renderBGTile<false>;
		renderWinTileFunc = &PPU::GBC_renderBGTile<true>;
	}
}

#define WRITE(var) st.write(reinterpret_cast<char*>(&var), sizeof(var))
#define WRITE_ARR(var) st.write(reinterpret_cast<char*>(var.data()), sizeof(var))

void PPU::saveState(std::ofstream& st)
{
	WRITE(regs);
	WRITE(s);

	if (System::Current() == GBSystem::GBC)
	{
		WRITE(gbcRegs);
		WRITE_ARR(VRAM_BANK1);
		WRITE_ARR(BGpaletteRAM);
		WRITE_ARR(OBPpaletteRAM);
	}

	WRITE_ARR(VRAM_BANK0);
	WRITE_ARR(OAM);

	if (s.state != PPUMode::VBlank)
	{
		WRITE(objCount);
		WRITE_ARR(selectedObjects);
		WRITE_ARR(bgPixelFlags);
	}
}

#undef WRITE
#undef WRITE_ARR

#define READ(var) st.read(reinterpret_cast<char*>(&var), sizeof(var))
#define READ_ARR(var) st.read(reinterpret_cast<char*>(var.data()), sizeof(var))

void PPU::loadState(std::ifstream& st)
{
	READ(regs);
	READ(s);

	if (System::Current() == GBSystem::GBC)
	{
		READ(gbcRegs);
		READ_ARR(VRAM_BANK1);
		READ_ARR(BGpaletteRAM);
		READ_ARR(OBPpaletteRAM);
	}
	else if (System::Current() == GBSystem::DMG)
	{
		updatePalette(regs.BGP, BGpalette);
		updatePalette(regs.OBP0, OBP0palette);
		updatePalette(regs.OBP1, OBP1palette);
	}

	READ_ARR(VRAM_BANK0);
	READ_ARR(OAM);

	SetPPUMode(s.state);

	if (s.state != PPUMode::VBlank)
	{
		READ(objCount);
		READ_ARR(selectedObjects);
		READ_ARR(bgPixelFlags);
	}

	if (!LCDEnabled())
		clearBuffer();
}

#undef READ
#undef READ_ARR

void PPU::updatePalette(uint8_t val, std::array<uint8_t, 4>& palette)
{
	for (uint8_t i = 0; i < 4; i++)
		palette[i] = (getBit(val, i * 2 + 1) << 1) | getBit(val, i * 2);
}

void PPU::updateDMG_ScreenColors(const std::array<color, 4>& newColors)
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

	s.state = ppuState;

	switch (s.state)
	{
	case PPUMode::HBlank:
		canAccessOAM = true; canAccessVRAM = true;
		break;
	case PPUMode::VBlank:
		canAccessOAM = true; canAccessVRAM = true;
		break;
	case PPUMode::OAMSearch:
		canAccessOAM = false;
		break;
	case PPUMode::PixelTransfer:
		canAccessVRAM = false;
		break;
	}
}

void PPU::execute()
{
	if (!LCDEnabled()) return;
	s.videoCycles += (cpu.doubleSpeed() ? 2 : 4);

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
	if (s.videoCycles >= HBLANK_CYCLES)
	{
		s.videoCycles -= HBLANK_CYCLES;
		s.LY++;

		updatedOAMPixels.clear();
		updatedWindowPixels.clear();

		if (s.LY == 144)
		{
			s.VBLANK_CYCLES = DEFAULT_VBLANK_CYCLES;
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

void PPU::handlePixelTransfer()
{
	if (s.videoCycles >= PIXEL_TRANSFER_CYCLES)
	{
		s.videoCycles -= PIXEL_TRANSFER_CYCLES;
		renderScanLine();
		SetPPUMode(PPUMode::HBlank);
	}
}

void PPU::handleOAMSearch()
{
	if (s.videoCycles >= OAM_SCAN_CYCLES)
	{
		s.videoCycles -= OAM_SCAN_CYCLES;
		objCount = 0;

		const bool doubleObj = DoubleOBJSize();

		for (uint16_t OAMAddr = 0; OAMAddr < sizeof(OAM); OAMAddr += 4)
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
					selectedObjects[objCount] = object{ objX, objY, static_cast<uint16_t>(tileInd * 16), attributes };
					objCount++;
				}
			}
			else
			{
				if (s.LY >= objY && s.LY < objY + 8)
				{
					uint16_t tileAddr = yFlip ? ((tileInd & 0xFE) + 1) * 16 : (tileInd & 0xFE) * 16;
					selectedObjects[objCount] = object{ objX, objY, tileAddr, attributes };
					objCount++;
				}
				else if (s.LY >= objY + 8 && s.LY < objY + 16)
				{
					uint16_t tileAddr = yFlip ? (tileInd & 0xFE) * 16 : ((tileInd & 0xFE) + 1) * 16;
					selectedObjects[objCount] = object{ objX, static_cast<int16_t>(objY + 8), tileAddr, attributes };
					objCount++;
				}
			}

			if (objCount == 10)
				break;
		}

		if (System::Current() == GBSystem::DMG)
			std::sort(selectedObjects.begin(), selectedObjects.begin() + objCount, object::objComparator);
		else
			std::reverse(selectedObjects.begin(), selectedObjects.begin() + objCount); 

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
		DMG_renderBlank();

	if (OBJEnable()) renderOAM();
}

void PPU::DMG_renderBlank()
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
	uint16_t bgTileAddr = BGTileAddr();
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

	uint16_t winTileAddr { WindowTileAddr() };
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

inline uint8_t toRGB8(uint8_t rgb5)
{
	return (rgb5 << 3) | (rgb5 >> 2);
}

template void PPU::GBC_renderBGTile<true>(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY);
template void PPU::GBC_renderBGTile<false>(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY);

template <bool updateWindowChangesBuffer>
void PPU::GBC_renderBGTile(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY)
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

void PPU::GBC_renderObjTile(const object& obj)
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

template void PPU::DMG_renderBGTile<true>(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY);
template void PPU::DMG_renderBGTile<false>(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY);

template <bool updateWindowChangesBuffer>
void PPU::DMG_renderBGTile(uint16_t tileMapInd, int16_t screenX, uint8_t scrollY)
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

void PPU::DMG_renderObjTile(const object& obj)
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

void PPU::renderTileData(uint8_t* buffer)
{
	if (!buffer)
		return;

	for (uint16_t addr = 0; addr < 0x17FF; addr += 16)
	{
		uint16_t tileInd = addr / 16;
		uint16_t screenX = (tileInd % 16) * 8;
		uint16_t screenY = (tileInd / 16) * 8;

		for (uint8_t y = 0; y < 8; y++)
		{
			uint8_t yPos { static_cast<uint8_t>(y + screenY) };
			uint8_t lsbLineByte { VRAM[addr + y * 2] };
			uint8_t msbLineByte { VRAM[addr + y * 2 + 1] };

			for (int8_t x = 7; x >= 0; x--)
			{
				uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);
				uint8_t xPos { static_cast<uint8_t>(7 - x + screenX) };
				PixelOps::setPixel(buffer, TILES_WIDTH, xPos, yPos, colors[colorId]);
			}
		}
	}
}