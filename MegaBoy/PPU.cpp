#include "PPU.h"
#include "bitOps.h"
#include <iostream>
#include <algorithm>

void PPU::reset()
{
	std::memset(VRAM.data(), 0, sizeof(VRAM));
	s = {};
	regs = {};

	disableLCD(PPUMode::OAMSearch);
	updatePalette(regs.BGP, BGpalette);

	canAccessOAM = false;
	canAccessVRAM = true;
}
void PPU::disableLCD(PPUMode mode)
{
	s.LY = 0;
	s.WLY = 0;
	s.videoCycles = 0;
	clearBuffer();
	SetPPUMode(mode);
}

#define WRITE(var) st.write(reinterpret_cast<char*>(&var), sizeof(var))
#define WRITE_ARR(var) st.write(reinterpret_cast<char*>(var.data()), sizeof(var))

void PPU::saveState(std::ofstream& st)
{
	WRITE(regs);
	WRITE(s);

	WRITE_ARR(VRAM);
	WRITE_ARR(OAM);

	if (s.state != PPUMode::VBlank)
	{
		WRITE(objCount);
		WRITE_ARR(selectedObjects);
		WRITE_ARR(opaqueBackgroundPixels);
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

	READ_ARR(VRAM);
	READ_ARR(OAM);

	updatePalette(regs.BGP, BGpalette);
	updatePalette(regs.OBP0, OBP0palette);
	updatePalette(regs.OBP1, OBP1palette);

	SetPPUMode(s.state);

	if (s.state != PPUMode::VBlank)
	{
		READ(objCount);
		READ_ARR(selectedObjects);
		READ_ARR(opaqueBackgroundPixels);
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

void PPU::updateScreenColors(const std::array<color, 4>& newColors)
{
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

	if (s.statRegChanged)
	{
		regs.STAT = s.newStatVal;
		s.statRegChanged = false;
	}
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
	if (s.videoCycles >= VBLANK_CYCLES)
	{
		s.videoCycles -= VBLANK_CYCLES;

		if (s.LY == 153)
		{
			s.LY = 0;
			s.WLY = 0;
			SetPPUMode(PPUMode::OAMSearch);
		}
		else
			s.LY++;
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

		std::sort(selectedObjects.begin(), selectedObjects.begin() + objCount, object::objComparator);
		SetPPUMode(PPUMode::PixelTransfer);
	}
}

void PPU::renderOAM()
{
	for (int i = 0; i < objCount; i++)
		renderObjTile(selectedObjects[i].tileAddr, selectedObjects[i].attributes, selectedObjects[i].X, selectedObjects[i].Y);

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
		renderBlank();

	if (OBJEnable()) renderOAM();
}

void PPU::renderBlank()
{
	for (uint8_t x = 0; x < SCR_WIDTH; x++)
	{
		setPixel(x, s.LY, getColor(BGpalette[0]));
		opaqueBackgroundPixels[x] = true;
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
		uint8_t tileIndex = VRAM[bgTileAddr + tileYInd + tileXInd];
		renderBGTile<false>(getBGTileAddr(tileIndex), (tileX * 8 - regs.SCX % 8), regs.SCY);
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
		uint8_t tileIndex = VRAM[winTileAddr + tileYInd + tileX];
		renderBGTile<true>(getBGTileAddr(tileIndex), wx + tileX * 8, s.WLY - s.LY);
	};

	s.WLY++;

	if (onWindowRender != nullptr)
		onWindowRender(framebuffer.data(), updatedWindowPixels, s.LY);
}

template void PPU::renderBGTile<true>(uint16_t addr, int16_t screenX, uint8_t scrollY);
template void PPU::renderBGTile<false>(uint16_t addr, int16_t screenX, uint8_t scrollY);

template <bool updateWindowChangesBuffer>
void PPU::renderBGTile(uint16_t addr, int16_t screenX, uint8_t scrollY)
{
	uint8_t lineOffset = 2 * ((s.LY + scrollY) % 8);
	uint8_t lsbLineByte = VRAM[addr + lineOffset];
	uint8_t msbLineByte = VRAM[addr + lineOffset + 1];

	for (int8_t x = 7; x >= 0; x--)
	{
		uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);
		int16_t xPos = 7 - x + screenX;

		if (xPos >= 0 && xPos < SCR_WIDTH)
		{
			setPixel(static_cast<uint8_t>(xPos), s.LY, getColor(BGpalette[colorId]));
			opaqueBackgroundPixels[xPos] = (colorId == 0);
			if constexpr (updateWindowChangesBuffer) updatedWindowPixels.push_back(static_cast<uint8_t>(xPos));
		}
	}
}

void PPU::renderObjTile(uint16_t tileAddr, uint8_t attributes, int16_t objX, int16_t objY)
{
	const bool xFlip = getBit(attributes, 5);
	const bool yFlip = getBit(attributes, 6);
	const auto& palette = getBit(attributes, 4) ? OBP1palette : OBP0palette;
	const uint8_t priority = getBit(attributes, 7);

	if (objX < SCR_WIDTH && objX > -8)
	{
		const uint8_t lineOffset { static_cast<uint8_t>(2 * (yFlip ? (objY - s.LY + 7) : (8 - (objY - s.LY + 8)))) };
		const uint8_t lsbLineByte { VRAM[tileAddr + lineOffset] };
		const uint8_t msbLineByte { VRAM[tileAddr + lineOffset + 1] };

		for (int8_t x = 7; x >= 0; x--)
		{
			uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);
			int16_t xPos = xFlip ? x + objX : 7 - x + objX;

			if (xPos >= 0 && xPos < SCR_WIDTH)
			{
				if (colorId != 0 && (priority == 0 || opaqueBackgroundPixels[xPos]))
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