#pragma once
#include "MMU.h"
#include <array>

struct color
{
	uint8_t R, G, B;
};

class PPU
{
public:
	friend MMU;

	static constexpr uint8_t SCR_WIDTH = 160;
	static constexpr uint8_t SCR_HEIGHT = 144;

	PPU(MMU& mmu) : mmu(mmu)
	{}

	void execute(uint8_t cycles);
	const auto getRenderingBuffer() { return renderBuffer.data(); }
private:
	MMU& mmu;

	uint8_t VRAM[8192]{};
	std::array<uint8_t, SCR_WIDTH * SCR_HEIGHT * 3> renderBuffer{};

	inline void setPixel(uint8_t x, uint8_t y, color c)
	{
		renderBuffer[(y * SCR_WIDTH * 3) + (x * 3)] = c.R; 
		renderBuffer[(y * SCR_WIDTH * 3) + (x * 3) + 1] = c.G;
		renderBuffer[(y * SCR_WIDTH * 3) + (x * 3) + 2] = c.B;
	}

	void renderTile(uint16_t tile, uint8_t x, uint8_t y);
	void renderBackground();
};