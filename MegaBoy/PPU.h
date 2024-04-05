#pragma once
#include "MMU.h"
#include "CPU.h"
#include <array>

struct color
{
	uint8_t R, G, B;
};

enum class PPUState
{
	OAMSearch,
	PixelTransfer,
	HBlank,
	VBlank
};

class PPU
{
public:
	friend MMU;
	PPUState state;

	static constexpr uint8_t SCR_WIDTH = 160;
	static constexpr uint8_t SCR_HEIGHT = 144;

	PPU(MMU& mmu, CPU& cpu) : mmu(mmu), cpu(cpu)
	{}

	void execute(uint8_t cycles);
	void reset();
	const auto getRenderingBuffer() { return renderBuffer.data(); }
private:
	MMU& mmu;
	CPU& cpu;

	uint8_t VRAM[8192]{};
	uint16_t videoCycles{0};
	uint8_t LY{0};
	std::array<uint8_t, SCR_WIDTH * SCR_HEIGHT * 3> renderBuffer{};

	inline void setPixel(uint8_t x, uint8_t y, color c)
	{
		renderBuffer[(y * SCR_WIDTH * 3) + (x * 3)] = c.R; 
		renderBuffer[(y * SCR_WIDTH * 3) + (x * 3) + 1] = c.G;
		renderBuffer[(y * SCR_WIDTH * 3) + (x * 3) + 2] = c.B;
	}

	void renderBackground();
	void renderWindow();
	void renderTileMap(uint16_t tileMapAddr);
	void renderTile(uint16_t tile, uint8_t x, uint8_t y);
};