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

enum class PPUMode : uint8_t
{
	HBlank = 0,
	VBlank = 1,
	OAMSearch = 2,
	PixelTransfer = 3,
};

class PPU
{
public:
	friend MMU;

	static constexpr uint8_t SCR_WIDTH = 160;
	static constexpr uint8_t SCR_HEIGHT = 144;

	PPU(MMU& mmu, CPU& cpu) : mmu(mmu), cpu(cpu)
	{
		reset();
	}

	void execute(uint8_t cycles);
	void reset();
	const auto getRenderingBuffer() { return renderBuffer.data(); }

	void disableLCD(PPUMode mode = PPUMode::HBlank);
	void OAMTransfer(uint16_t sourceAddr);

	std::array<uint8_t, 4> BGpalette;
	std::array<uint8_t, 4> OBP0palette;
	std::array<uint8_t, 4> OBP1palette;

	inline void updatePalette(uint8_t val, std::array<uint8_t, 4>& palette)
	{
		for (int i = 0; i < 4; i++)
			palette[i] = (getBit(val, i * 2 + 1) << 1) | getBit(val, i * 2);
	}
private:
	MMU& mmu;
	CPU& cpu;

	PPUMode state;
	uint8_t VRAM[8192];
	uint16_t videoCycles;
	uint8_t LY;
	std::array<uint8_t, SCR_WIDTH * SCR_HEIGHT * 3> renderBuffer{};

	static constexpr uint8_t OAM_SCAN_CYCLES = 20;
	static constexpr uint8_t PIXEL_TRANSFER_CYCLES = 43;
	static constexpr uint8_t HBLANK_CYCLES = 51;
	static constexpr uint8_t VBLANK_CYCLES = 114;

	static constexpr std::array<color, 4> colors = { color {255, 255, 255}, color {169, 169, 169}, color {84, 84, 84}, color {0, 0, 0} };
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

	/////////////////////////////////////////////
	void SetLY(uint8_t val);
	void SetPPUMode(PPUMode ppuState);

	void handleOAMSearch();
	void handleHBlank();
	void handleVBlank();
	void handlePixelTransfer();

	void renderScanLine();
	void renderTile_S(uint16_t addr, uint8_t screenX, uint8_t scrollY);
	void renderTileMap_S(uint16_t tileMapAddr, uint8_t scrollX, uint8_t scrollY);
	void renderBackground_S();
	void renderWindow_S();
	void renderOAM_S();
	void renderBlank();

	inline bool TileMapsEnable() { return getBit(mmu.directRead(0xFF40), 0); }
	inline bool OBJEnable() { return getBit(mmu.directRead(0xFF40), 1); }
	inline bool DoubleOBJSize() { return getBit(mmu.directRead(0xFF40), 2); }
	inline uint16_t BGTileAddr() { return getBit(mmu.directRead(0xFF40), 3) ? 0x1C00 : 0x1800; }
	inline uint16_t WindowTileAddr() { return getBit(mmu.directRead(0xFF40), 6) ? 0x1C00 : 0x1800; }
	inline bool BGUnsignedAddressing() { return getBit(mmu.directRead(0xFF40), 4); }
	inline bool WindowEnable() { return getBit(mmu.directRead(0xFF40), 5); }
	inline bool LCDEnabled() { return getBit(mmu.directRead(0xFF40), 7); }

	constexpr uint8_t LYC() { return mmu.directRead(0xFF45); }
	inline bool LYC_STAT() { return getBit(mmu.directRead(0xFF41), 6); }
	inline bool OAM_STAT() { return getBit(mmu.directRead(0xFF41), 5); }
	inline bool VBlank_STAT() { return getBit(mmu.directRead(0xFF41), 4); }
	inline bool HBlank_STAT() { return getBit(mmu.directRead(0xFF41), 3); }

	constexpr uint8_t SCY() { return mmu.directRead(0xFF42); }
	constexpr uint8_t SCX() { return mmu.directRead(0xFF43); }
	constexpr uint8_t WY() { return mmu.directRead(0xFF4A); }
	constexpr uint8_t WX() { return mmu.directRead(0xFF4B); }
};