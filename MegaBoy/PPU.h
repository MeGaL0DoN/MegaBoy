#pragma once
#include "MMU.h"
#include "CPU.h"
#include "bitOps.h"
#include <array>

struct color
{
	uint8_t R, G, B;

	bool operator ==(color other)
	{
		return R == other.R && G == other.G && B == other.B;
	}
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
	PPUState state{ PPUState::OAMSearch };

	static constexpr uint8_t SCR_WIDTH = 160;
	static constexpr uint8_t SCR_HEIGHT = 144;

	PPU(MMU& mmu, CPU& cpu) : mmu(mmu), cpu(cpu)
	{
		reset();
	}

	void execute(uint8_t cycles);
	void OAMTransfer(uint16_t sourceAddr);
	void reset();
	const auto getRenderingBuffer() { return renderBuffer.data(); }
private:
	MMU& mmu;
	CPU& cpu;

	uint8_t VRAM[8192];
	uint16_t videoCycles;
	uint8_t LY;
	std::array<uint8_t, SCR_WIDTH * SCR_HEIGHT * 3> renderBuffer{};

	static constexpr std::array<color, 4> colors = { color {255, 255, 255}, color {169, 169, 169}, color {84, 84, 84}, color {0, 0, 0} };

	std::array<uint8_t, 4> BGpalette;
	std::array<uint8_t, 4> OBP0palette;
	std::array<uint8_t, 4> OBP1palette;

	inline void updatePalette(uint8_t val, std::array<uint8_t, 4>& palette)
	{
		for (int i = 0; i < 4; i++)
			palette[i] = (getBit(val, i * 2 + 1) << 1) | getBit(val, i * 2);
	}

	constexpr color getColor(uint8_t ind) { return colors[ind]; }

	inline void setPixel(uint8_t x, uint8_t y, color c)
	{
		renderBuffer[(y * SCR_WIDTH * 3) + (x * 3)] = c.R; 
		renderBuffer[(y * SCR_WIDTH * 3) + (x * 3) + 1] = c.G;
		renderBuffer[(y * SCR_WIDTH * 3) + (x * 3) + 2] = c.B;
	}
	inline color getPixel(uint8_t x, uint8_t y)
	{
		return color
		{
			renderBuffer[(y * SCR_WIDTH * 3) + (x * 3)],
			renderBuffer[(y * SCR_WIDTH * 3) + (x * 3) + 1],
			renderBuffer[(y * SCR_WIDTH * 3) + (x * 3) + 2]
		};
	}

	void renderBackground();
	void renderWindow();
	void renderOAM();
	void renderTileMap(uint16_t tileMapAddr);
	void renderTile(uint16_t tile, uint8_t x, uint8_t y, std::array<uint8_t, 4> palette);
};