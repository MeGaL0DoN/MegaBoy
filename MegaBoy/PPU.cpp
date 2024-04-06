#include "PPU.h"
#include "bitOps.h"
#include <iostream>

void PPU::reset()
{
	std::memset(VRAM, 0, sizeof(VRAM));
	std::memset(renderBuffer.data(), 255, sizeof(renderBuffer));

	BGpalette = { 0, 1, 2, 3 };
	LY = 0;
	SetPPUMode(PPUMode::OAMSearch);
}
void PPU::disableLCD()
{
	std::memset(renderBuffer.data(), 255, sizeof(renderBuffer));
	LY = 0;
	SetPPUMode(PPUMode::HBlank);
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
	videoCycles = 0;

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
	if (videoCycles >= 20)
		SetPPUMode(PPUMode::PixelTransfer);
}

void PPU::handlePixelTransfer()
{
	if (videoCycles >= 43)
	{
		// render scanline
		SetPPUMode(PPUMode::HBlank);
	}
}

void PPU::handleHBlank()
{
	if (videoCycles >= 51)
	{
		SetLY(LY + 1);

		if (LY == 144)
		{
			SetPPUMode(PPUMode::VBlank);
			cpu.requestInterrupt(Interrupt::VBlank);

			renderBackground(); // To remove
			renderWindow(); // To remove
			renderOAM(); // To remove
		}
		else
			SetPPUMode(PPUMode::OAMSearch);
	}
}

void PPU::handleVBlank()
{
	if (videoCycles >= 114)
	{
		videoCycles = 0;
		SetLY(LY + 1);

		if (LY == 153)
		{
			SetLY(0);
			SetPPUMode(PPUMode::OAMSearch);
		}
	}
}











void PPU::renderTile(uint16_t addr, uint8_t screenX, uint8_t screenY, std::array<uint8_t, 4> palette)
{
	for (int y = 0; y < 8; y++, addr += 2)
	{
		uint8_t lsbLineByte = VRAM[addr];
		uint8_t msbLineByte = VRAM[addr + 1];

		for (int x = 0; x < 8; x++)
		{
			uint8_t colorId = (getBit(msbLineByte, 7 - x) << 1) | getBit(lsbLineByte, 7 - x);
			setPixel(x + screenX, y + screenY, getColor(palette[colorId]));
		}
	}
}

void PPU::renderBackground()
{
	const uint16_t bgStartAddr = getBit(mmu.directRead(0xFF40), 3) ? 0x1C00 : 0x1800;
	renderTileMap(bgStartAddr);
}
void PPU::renderWindow()
{
	const bool windowEnable = getBit(mmu.directRead(0xFF40), 5);
	if (windowEnable)
	{
		const uint16_t windowStartAddr = getBit(mmu.directRead(0xFF40), 6) ? 0x1C00 : 0x1800;
		renderTileMap(windowStartAddr);
	}
}

void PPU::renderTileMap(uint16_t tileMapAddr)
{
	const bool unsignedAddressing = getBit(mmu.directRead(0xFF40), 4);

	for (uint8_t tileY = 0; tileY < 18; tileY++) 
	{
		for (uint8_t tileX = 0; tileX < 20; tileX++)
		{
			uint8_t tileIndex = VRAM[tileMapAddr + tileY * 32 + tileX];
			uint16_t tileDataAddr;

			if (unsignedAddressing) tileDataAddr = tileIndex * 16;
			else tileDataAddr = 0x1000 + static_cast<int8_t>(tileIndex) * 16;

			renderTile(tileDataAddr, tileX * 8, tileY * 8, BGpalette);
		}
	}
}

void PPU::OAMTransfer(uint16_t sourceAddr)
{
	uint16_t destinationAddr = 0xFE00;

	for (int i = 0; i < 159; i++, sourceAddr++, destinationAddr++)
		mmu.directWrite(destinationAddr, mmu.directRead(sourceAddr));

	videoCycles += 640;
}

void PPU::renderOAM()
{
	uint16_t OAMAddr = 0xFE00;
	bool objEnable = getBit(mmu.directRead(0xFF40), 1);
	if (!objEnable) return;

	for (int i = 0; i < 40; i++, OAMAddr += 4)
	{
		uint8_t yPos = mmu.directRead(OAMAddr) - 16;
		uint8_t xPos = mmu.directRead(OAMAddr + 1) - 8;
		uint8_t tileInd = mmu.directRead(OAMAddr + 2);
		uint8_t attributes = mmu.directRead(OAMAddr + 3);

		bool xFlip = getBit(attributes, 5);
		bool yFlip = getBit(attributes, 6);
		const auto& palette = getBit(attributes, 4) ? OBP1palette : OBP0palette;
		uint8_t priority = getBit(attributes, 7);

		if (yPos >= 0 && yPos <= SCR_HEIGHT && xPos >= 0 && xPos <= SCR_WIDTH)
		{
			uint16_t addr = tileInd * 16;

			for (int y = 0; y < 8; y++, addr += 2)
			{
				uint8_t lsbLineByte = VRAM[addr];
				uint8_t msbLineByte = VRAM[addr + 1];

				for (int x = 0; x < 8; x++)
				{
					uint8_t xVal = xPos + x;
					uint8_t yVal = yPos + y;
					uint8_t colorId = (getBit(msbLineByte, 7 - x) << 1) | getBit(lsbLineByte, 7 - x);

					if (colorId != 0 && (priority == 0 || getPixel(xVal, yVal) == getColor(BGpalette[0])))
						setPixel(xVal, yVal, getColor(palette[colorId]));
				}
			}
		}
	}
}
