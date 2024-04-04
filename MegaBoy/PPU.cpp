#include "PPU.h"
#include "bitOps.h"
#include <iostream>

void PPU::reset()
{
	std::memset(VRAM, 0, sizeof(VRAM));
	std::memset(renderBuffer.data(), 0, sizeof(renderBuffer));
}

void PPU::execute(uint8_t cycles)
{
	renderBackground();

	// TODO
	switch (state)
	{
	case PPUState::OAMSearch:
		if (ticks == 40)
			state = PPUState::PixelTransfer;
		break;
	case PPUState::PixelTransfer:
		state = PPUState::HBlank;
		break;
	case PPUState::HBlank:
		if (ticks == 456)
		{
			ticks = 0;
			LY++;
			state = LY == 144 ? PPUState::VBlank : PPUState::OAMSearch;
		}
		break;
	case PPUState::VBlank:
		state = PPUState::OAMSearch;
		break;
	}
}

constexpr color palette[] = { {255, 255, 255}, {169, 169, 169}, {84, 84, 84}, {0, 0, 0} };

void PPU::renderTile(uint16_t addr, uint8_t screenX, uint8_t screenY)
{
	for (int y = 0; y < 8; y++, addr += 2)
	{
		uint8_t lsbLineByte = VRAM[addr];
		uint8_t msbLineByte = VRAM[addr + 1];

		for (int x = 0; x < 8; x++)
		{
			uint8_t colorId = (getBit(msbLineByte, 7 - x) << 1) | getBit(lsbLineByte, 7 - x);
			setPixel(x + screenX, y + screenY, palette[colorId]);
		}
	}
}

void PPU::renderBackground() {
	constexpr uint16_t bgStartAddr = 0x1800;

	for (uint8_t tileY = 0; tileY < 18; tileY++) 
	{
		for (uint8_t tileX = 0; tileX < 20; tileX++)
		{
			uint8_t tileIndex = VRAM[bgStartAddr + tileY * 32 + tileX];
			uint16_t tileDataAddr = tileIndex * 16;
			renderTile(tileDataAddr, tileX * 8, tileY * 8);
		}
	}
}
