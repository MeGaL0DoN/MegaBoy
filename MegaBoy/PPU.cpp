#include "PPU.h"
#include "bitOps.h"
#include <iostream>
#include <algorithm>

void PPU::reset()
{
	std::memset(VRAM.data(), 0, sizeof(VRAM));
	disableLCD(PPUMode::OAMSearch);

	LCDC = 0x91;
	STAT = 0x85;
	SCY = 0x00;
	SCX = 0x00;
	LYC = 0x00;
	WY = 0;
	WX = 0;

	BGP = 0xFC;
	updatePalette(BGP, BGpalette);
}
void PPU::disableLCD(PPUMode mode)
{
	LY = 0;
	WLY = 0;
	videoCycles = 0;
	clearBuffer();
	SetPPUMode(mode);
}

void PPU::updatePalette(uint8_t val, std::array<uint8_t, 4>& palette)
{
	for (int i = 0; i < 4; i++)
		palette[i] = (getBit(val, i * 2 + 1) << 1) | getBit(val, i * 2);
}

void PPU::updateScreenColors(const std::array<color, 4>& newColors)
{
	for (int x = 0; x < SCR_WIDTH; x++)
	{
		for (int y = 0; y < SCR_HEIGHT; y++)
		{
			color pixel = getPixel(x, y);
			uint8_t pixelInd = std::find(colors.begin(), colors.end(), pixel) - colors.begin();
			setPixel(x, y, newColors[pixelInd]);
		}
	}
}

void PPU::SetLY(uint8_t val)
{
	LY = val;
	STAT = resetBit(STAT, 2);

	if (LY == LYC)
	{
		STAT = setBit(STAT, 2);

		if (LYC_STAT())
			cpu.requestInterrupt(Interrupt::STAT);
	}
}

void PPU::SetPPUMode(PPUMode ppuState)
{
	STAT = setBit(STAT, 1, static_cast<uint8_t>(ppuState) & 0x2);
	STAT = setBit(STAT, 0, static_cast<uint8_t>(ppuState) & 0x1);

	state = ppuState;

	switch (state)
	{
	case PPUMode::HBlank:
		if (HBlank_STAT()) cpu.requestInterrupt(Interrupt::STAT);
		break;
	case PPUMode::VBlank:
		if (VBlank_STAT()) cpu.requestInterrupt(Interrupt::STAT);
		break;
	case PPUMode::OAMSearch:
		if (OAM_STAT()) cpu.requestInterrupt(Interrupt::STAT);
		break;
	}
}

void PPU::execute()
{
	if (!LCDEnabled()) return;
	videoCycles++;

	switch (state)
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
}

void PPU::handleHBlank()
{
	if (videoCycles >= HBLANK_CYCLES)
	{
		videoCycles -= HBLANK_CYCLES;
		SetLY(LY + 1);

		updatedOAMPixels.clear();
		updatedWindowPixels.clear();

		if (LY == 144)
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
	if (videoCycles >= VBLANK_CYCLES)
	{
		videoCycles -= VBLANK_CYCLES;

		if (LY == 153)
		{
			SetLY(0);
			WLY = 0;
			SetPPUMode(PPUMode::OAMSearch);
		}
		else
			SetLY(LY + 1);
	}
}

void PPU::handlePixelTransfer()
{
	if (videoCycles >= PIXEL_TRANSFER_CYCLES)
	{
		videoCycles -= PIXEL_TRANSFER_CYCLES;
		renderScanLine();
		SetPPUMode(PPUMode::HBlank);
	}
}

void PPU::handleOAMSearch()
{
	if (videoCycles >= OAM_SCAN_CYCLES)
	{
		videoCycles -= OAM_SCAN_CYCLES;
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
				if (LY >= objY && LY < objY + 8)
				{
					selectedObjects[objCount] = object{ objX, objY, static_cast<uint16_t>(tileInd * 16), attributes };
					objCount++;
				}
			}
			else
			{
				if (LY >= objY && LY < objY + 8)
				{
					uint16_t tileAddr = yFlip ? ((tileInd & 0xFE) + 1) * 16 : (tileInd & 0xFE) * 16;
					selectedObjects[objCount] = object{ objX, objY, tileAddr, attributes };
					objCount++;
				}
				else if (LY >= objY + 8 && LY < objY + 16)
				{
					uint16_t tileAddr = yFlip ? (tileInd & 0xFE) * 16 : ((tileInd & 0xFE) + 1) * 16;
					selectedObjects[objCount] = object{ objX, objY + 8, tileAddr, attributes };
					objCount++;
				}
			}

			if (objCount == 10)
				break;
		}

		std::sort(selectedObjects, selectedObjects + objCount, object::objComparator);
		SetPPUMode(PPUMode::PixelTransfer);
	}
}

void PPU::renderOAM()
{
	for (int i = 0; i < objCount; i++)
		renderObjTile(selectedObjects[i].tileAddr, selectedObjects[i].attributes, selectedObjects[i].X, selectedObjects[i].Y);

	if (onOAMRender != nullptr)
		onOAMRender(framebuffer.data(), updatedOAMPixels, LY);
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
	for (int x = 0; x < SCR_WIDTH; x++)
	{
		setPixel(x, LY, getColor(BGpalette[0]));
		opaqueBackgroundPixels[x] = true;
	}

	if (onBackgroundRender != nullptr)
		onBackgroundRender(framebuffer.data(), LY);
}

void PPU::renderBackground()
{
	uint16_t bgTileAddr = BGTileAddr();
	uint16_t tileYInd = (static_cast<uint8_t>(LY + SCY) / 8) * 32;

	for (uint8_t tileX = 0; tileX < 21; tileX++)
	{
		uint16_t tileXInd = (tileX + SCX / 8) % 32; 
		uint8_t tileIndex = VRAM[bgTileAddr + tileYInd + tileXInd];
		renderBGTile<false>(getBGTileAddr(tileIndex), (tileX * 8 - SCX % 8), SCY);
	}

	if (onBackgroundRender != nullptr)
		onBackgroundRender(framebuffer.data(), LY);
}
void PPU::renderWindow()
{
	int16_t wx = static_cast<int16_t>(WX) - 7;
	if (LY < WY || wx >= SCR_WIDTH) return;

	uint16_t winTileAddr = WindowTileAddr();
	uint16_t tileYInd = WLY / 8 * 32;
	uint8_t winTileXEnd = 32 - (wx / 8);

	for (uint8_t tileX = 0; tileX < winTileXEnd; tileX++)
	{
		uint8_t tileIndex = VRAM[winTileAddr + tileYInd + tileX];
		renderBGTile<true>(getBGTileAddr(tileIndex), wx + tileX * 8, WLY - LY);
	};

	WLY++;

	if (onWindowRender != nullptr)
		onWindowRender(framebuffer.data(), updatedWindowPixels, LY);
}

template void PPU::renderBGTile<true>(uint16_t addr, int16_t screenX, uint8_t scrollY);
template void PPU::renderBGTile<false>(uint16_t addr, int16_t screenX, uint8_t scrollY);

template <bool updateWindowChangesBuffer>
void PPU::renderBGTile(uint16_t addr, int16_t screenX, uint8_t scrollY)
{
	uint8_t lineOffset = 2 * ((LY + scrollY) % 8);
	uint8_t lsbLineByte = VRAM[addr + lineOffset];
	uint8_t msbLineByte = VRAM[addr + lineOffset + 1];

	for (int x = 7; x >= 0; x--)
	{
		uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);
		int16_t xPos = 7 - x + screenX;
		if (xPos >= 0 && xPos < SCR_WIDTH)
		{
			auto color = getColor(BGpalette[colorId]);
			setPixel(static_cast<uint8_t>(xPos), LY, getColor(BGpalette[colorId]));

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
		const uint8_t lineOffset = 2 * (yFlip ? (objY - LY + 7) : (8 - (objY - LY + 8)));
		const uint8_t lsbLineByte = VRAM[tileAddr + lineOffset];
		const uint8_t msbLineByte = VRAM[tileAddr + lineOffset + 1];

		for (int x = 7; x >= 0; x--)
		{
			uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);
			int16_t xPos = xFlip ? x + objX : 7 - x + objX;

			if (xPos >= 0 && xPos < SCR_WIDTH)
			{
				if (colorId != 0 && (priority == 0 || opaqueBackgroundPixels[xPos]))
				{
					auto color = getColor(palette[colorId]);
					setPixel(static_cast<uint8_t>(xPos), LY, color);
					updatedOAMPixels.push_back(static_cast<uint8_t>(xPos));
				}
			}
		}
	}
}

void PPU::renderTileData(uint8_t* buffer)
{
	for (int addr = 0; addr < 0x17FF; addr += 16)
	{
		uint16_t tileInd = addr / 16;
		uint16_t screenX = (tileInd % 16) * 8;
		uint16_t screenY = (tileInd / 16) * 8;

		for (int y = 0; y < 8; y++)
		{
			uint8_t yPos = y + screenY;
			uint8_t lsbLineByte = VRAM[addr + y * 2];
			uint8_t msbLineByte = VRAM[addr + y * 2 + 1];

			for (int x = 7; x >= 0; x--)
			{
				uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);
				uint8_t xPos = 7 - x + screenX;
				PixelOps::setPixel(buffer, TILES_WIDTH, xPos, yPos, colors[colorId]);
			}
		}
	}
}