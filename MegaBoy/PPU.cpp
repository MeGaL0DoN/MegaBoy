#include "PPU.h"
#include "bitOps.h"
#include <iostream>
#include <algorithm>

void PPU::reset()
{
	std::memset(VRAM, 0, sizeof(VRAM));
	dmaTransfer = false;
	disableLCD(PPUMode::OAMSearch);
}
void PPU::clearBuffer()
{
	for (int x = 0; x < SCR_WIDTH; x++)
	{
		for (int y = 0; y < SCR_HEIGHT; y++)
			setPixel(x, y, colors[0]);
	}
}
void PPU::disableLCD(PPUMode mode)
{
	SetLY(0);
	WLY = 0;
	videoCycles = 0;
	clearBuffer();
	SetPPUMode(mode);
}

void PPU::startDMATransfer(uint16_t sourceAddr)
{
	dmaTransfer = true;
	dmaCycles = 0;
	dmaSourceAddr = sourceAddr;
}

void PPU::updatePalette(uint8_t val, std::array<uint8_t, 4> palette)
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
	opaqueBackgroundPixels.reset();

	if (LY == LYC())
	{
		mmu.directWrite(0xFF41, setBit(mmu.directRead(0xFF41), 2));

		if (LYC_STAT())
			cpu.requestInterrupt(Interrupt::STAT);
	}
}

void PPU::SetPPUMode(PPUMode ppuState)
{
	uint8_t lcdS = mmu.directRead(0xFF41);
	lcdS = setBit(lcdS, 1, static_cast<uint8_t>(ppuState) & 0x2);
	lcdS = setBit(lcdS, 0, static_cast<uint8_t>(ppuState) & 0x1);

	mmu.directWrite(0xFF41, lcdS);
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
	if (dmaTransfer)
	{
		uint16_t destinationAddr = 0xFE00 + dmaCycles;
		mmu.directWrite(destinationAddr, mmu.directRead(dmaSourceAddr++));
		dmaCycles++;

		if (dmaCycles >= DMA_CYCLES)
			dmaTransfer = false;
	}

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

void PPU::handleOAMSearch()
{
	if (videoCycles >= OAM_SCAN_CYCLES)
	{
		videoCycles -= OAM_SCAN_CYCLES;
		SetPPUMode(PPUMode::PixelTransfer);
	}
}
void PPU::handleHBlank()
{
	if (videoCycles >= HBLANK_CYCLES)
	{
		videoCycles -= HBLANK_CYCLES;
		SetLY(LY + 1);

		if (LY == 144)
		{
			SetPPUMode(PPUMode::VBlank);
			cpu.requestInterrupt(Interrupt::VBlank);
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
		SetLY(LY + 1);

		if (LY == 153)
		{
			SetLY(0);
			WLY = 0;
			SetPPUMode(PPUMode::OAMSearch);
		}
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
}

void PPU::renderBackground()
{
	// TODO: check horizontal scrolling.

	uint16_t bgTileAddr = BGTileAddr();
	uint16_t tileYInd = (static_cast<uint8_t>(LY + SCY()) / 8) * 32;
	uint8_t scrollX = SCX();

	for (uint8_t tileX = 0; tileX < 20; tileX++)
	{
		uint16_t tileXInd = (tileX + scrollX / 8) % 32; // To check
		uint8_t tileIndex = VRAM[bgTileAddr + tileYInd + tileXInd];
		renderBGTile(getBGTileAddr(tileIndex), LY, tileX * 8 - scrollX % 8, SCY());
	}
}
void PPU::renderWindow()
{
	int16_t wx = static_cast<int16_t>(WX()) - 7;
	uint8_t wy = WY();
	if (LY < wy || wx < 0 || wx >= SCR_WIDTH) return;

	uint16_t winTileAddr = WindowTileAddr();
	uint16_t tileYInd = (WLY) / 8 * 32;
	uint8_t winTileXEnd = 32 - (wx / 8);

	for (uint8_t tileX = 0; tileX < winTileXEnd; tileX++)
	{
		uint8_t tileIndex = VRAM[winTileAddr + tileYInd + tileX];
		renderBGTile(getBGTileAddr(tileIndex), LY, wx + tileX * 8, 0);
	};

	WLY++;
}

void PPU::renderBGTile(uint16_t addr, uint8_t LY, uint8_t screenX, uint8_t scrollY)
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
			setPixel(xPos, LY, getColor(BGpalette[colorId]));
			if (colorId == 0) opaqueBackgroundPixels[xPos] = true;
		}
	}
}

void PPU::renderObjTile(uint16_t tileAddr, uint8_t attributes, int16_t objX, int16_t objY)
{
	const bool xFlip = getBit(attributes, 5);
	const bool yFlip = getBit(attributes, 6);
	const auto& palette = getBit(attributes, 4) ? OBP1palette : OBP0palette;
	const uint8_t priority = getBit(attributes, 7);

	if (objX + 8 < SCR_WIDTH && objX > -8)
	{
		const uint8_t lineOffset = 2 * (yFlip ? (objY - LY + 8) : (8 - (objY - LY + 8)));
		const uint8_t lsbLineByte = VRAM[tileAddr + lineOffset];
		const uint8_t msbLineByte = VRAM[tileAddr + lineOffset + 1];

		for (int x = 7; x >= 0; x--)
		{
			uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);
			int16_t xPos = xFlip ? x + objX : 7 - x + objX;

			if (xPos >= 0 && xPos < SCR_WIDTH)
			{
				if (colorId != 0 && (priority == 0 || opaqueBackgroundPixels[xPos]))
					setPixel(xPos, LY, getColor(palette[colorId]));
			}
		}
	}
}

void PPU::renderOAM()
{
	uint16_t OAMAddr = 0xFE00;
	uint8_t scanlineObjects{ 0 };
	uint8_t objSize = OBJSize();

	for (int i = 0; i < 40; i++, OAMAddr += 4)
	{
		const int16_t objY = static_cast<int16_t>(mmu.directRead(OAMAddr)) - 16;
		const int16_t objX = static_cast<int16_t>(mmu.directRead(OAMAddr + 1)) - 8;
		const uint8_t tileInd = mmu.directRead(OAMAddr + 2);
		const uint8_t attributes = mmu.directRead(OAMAddr + 3);

		if (objSize == 8)
		{
			if (LY >= objY && LY < objY + 8)
			{
				renderObjTile(tileInd * 16, attributes, objX, objY);
				scanlineObjects++;
			}
		}
		else
		{
			if (LY >= objY && LY < objY + 8)
			{
				renderObjTile((tileInd & 0xFE) * 16, attributes, objX, objY);
				scanlineObjects++;
			}
			else if (LY >= objY + 8 && LY < objY + 16)
			{
				renderObjTile(((tileInd & 0xFE) + 1) * 16, attributes, objX, objY + 8);
				scanlineObjects++;
			}
		}

		if (scanlineObjects == 10)
			return;
	}
}


//void PPU::renderOAM()
//{
//	uint16_t OAMAddr = 0xFE00;
//	uint8_t scanlineObjects { 0 };
//	uint8_t objSize = OBJSize();
//
//	for (int i = 0; i < 40; i++, OAMAddr += 4)
//	{
//		const int16_t objY = static_cast<int16_t>(mmu.directRead(OAMAddr)) - 16;
//		const int16_t objX = static_cast<int16_t>(mmu.directRead(OAMAddr + 1)) - 8;
//		const uint8_t tileInd = mmu.directRead(OAMAddr + 2);
//		const uint8_t attributes = mmu.directRead(OAMAddr + 3);
//
//		if (objSize == 8)
//		{
//
//			if (objY + 8 > LY && objY <= LY)
//			{
//				renderObjTile(tileInd * 16, attributes, objX, objY);
//				scanlineObjects++;
//			}
//		}
//
//
//		//if (!DoubleOBJSize())
//		//{
//		//	if (objY + 8 > LY && objY <= LY)
//		//	{
//		//		renderObjTile(tileInd * 16, attributes, objX, objY);
//		//		scanlineObjects++;
//		//	}
//		//}
//		//else
//		//{
//		//	if (true)
//		//	{
//		//		renderObjTile(objY <= LY + 8 ? tileInd * 16 : tileInd * 32, attributes, objX, objY);
//		//		scanlineObjects++;
//		//	}
//		//}
//
//		if (scanlineObjects == 10)
//			return;
//	}
//}