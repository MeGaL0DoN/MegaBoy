#include "PPU.h"
#include "bitOps.h"
#include <iostream>

void PPU::reset()
{
	std::memset(VRAM, 0, sizeof(VRAM));
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

void PPU::updatePalette(uint8_t val, std::array<uint8_t, 4>& palette)
{
	for (int i = 0; i < 4; i++)
		palette[i] = (getBit(val, i * 2 + 1) << 1) | getBit(val, i * 2);
}

// To fix - cycles, copying 1 byte per M cycle.
void PPU::OAMTransfer(uint16_t sourceAddr)
{
	uint16_t destinationAddr = 0xFE00;

	for (int i = 0; i < 159; i++, sourceAddr++, destinationAddr++)
		mmu.directWrite(destinationAddr, mmu.directRead(sourceAddr));

	//videoCycles += 640;
}

void PPU::SetLY(uint8_t val)
{
	LY = val;

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

void PPU::execute(uint8_t cycles)
{
	if (!LCDEnabled()) return;
	videoCycles += cycles;

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
		setPixel(x, LY, colors[0]);
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
		uint8_t a = scrollX % 8;
		renderTile(getTileAddr(tileIndex), LY, tileX * 8 - scrollX % 8, SCY());
	}
}
void PPU::renderWindow()
{
	int16_t wx = static_cast<int16_t>(WX()) - 7;
	uint8_t wy = WY();
	if (wy > LY) return;

	uint16_t winTileAddr = WindowTileAddr();
	uint16_t tileYInd = (LY - wy) / 8 * 32;
	uint8_t winTileXEnd = 20 - (wx / 8);

	for (uint8_t tileX = 0; tileX < winTileXEnd; tileX++)
	{
		uint8_t tileIndex = VRAM[winTileAddr + tileYInd + tileX];
		renderTile(getTileAddr(tileIndex), LY, wx + tileX * 8, 0/*wy*/);
	}
}

void PPU::renderTile(uint16_t addr, uint8_t LY, uint8_t screenX, uint8_t scrollY)
{
	uint8_t lineOffset = 2 * ((LY + scrollY) % 8);
	uint8_t lsbLineByte = VRAM[addr + lineOffset];
	uint8_t msbLineByte = VRAM[addr + lineOffset + 1];

	for (int x = 7; x >= 0; x--)
	{
		uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);
		uint8_t xPos = 7 - x + screenX;
		if (xPos < SCR_WIDTH) setPixel(xPos, LY, getColor(BGpalette[colorId]));
	}
}

void PPU::renderOAM()
{
	uint16_t OAMAddr = 0xFE00;

	for (int i = 0; i < 40; i++, OAMAddr += 4)
	{
		const int16_t objY = static_cast<int16_t>(mmu.directRead(OAMAddr)) - 8;
		const int16_t objX = static_cast<int16_t>(mmu.directRead(OAMAddr + 1)) - 8;
		const uint8_t tileInd = mmu.directRead(OAMAddr + 2);
		const uint8_t attributes = mmu.directRead(OAMAddr + 3);

		const bool xFlip = getBit(attributes, 5);
		const bool yFlip = getBit(attributes, 6);
		const auto& palette = getBit(attributes, 4) ? OBP1palette : OBP0palette;
		const uint8_t priority = getBit(attributes, 7);

		const uint16_t tileAddr = tileInd * 16;

		if (!DoubleOBJSize())
		{
			if (objY > LY && objY <= LY + 8)
			{
				const uint8_t lineOffset = 2 * (yFlip ? (objY - LY) : (8 - (objY - LY)));
				const uint8_t lsbLineByte = VRAM[tileAddr + lineOffset];
				const uint8_t msbLineByte = VRAM[tileAddr + lineOffset + 1];

				for (int x = 7; x >= 0; x--)
				{
					uint8_t colorId = (getBit(msbLineByte, x) << 1) | getBit(lsbLineByte, x);
					uint8_t xPos = xFlip ? x + objX : 7 - x + objX;

					if (colorId != 0 && (priority == 0 || getPixel(xPos, LY) == getColor(BGpalette[0])))
						setPixel(xPos, LY, getColor(palette[colorId]));
				}
			}
		}
	}
}