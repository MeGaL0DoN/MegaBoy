#pragma once
#include "MMU.h"
#include "CPU.h"
#include "bitOps.h"
#include <array>
#include <bitset>

#include "pixelOps.h"
using color = PixelOps::color;

enum class PPUMode : uint8_t
{
	HBlank = 0,
	VBlank = 1,
	OAMSearch = 2,
	PixelTransfer = 3,
};

struct pixelInfo
{
	bool isSet;
	color data;
};

class debugUI;

class PPU
{
public:
	static constexpr uint8_t SCR_WIDTH = 160;
	static constexpr uint8_t SCR_HEIGHT = 144;
	static constexpr uint16_t TILES_SIZE = 32 * 8;

	static constexpr uint32_t FRAMEBUFFER_SIZE = SCR_WIDTH * SCR_HEIGHT * 3;
	static constexpr uint32_t TILEDATA_FRAMEBUFFER_SIZE = TILES_SIZE * TILES_SIZE * 3;

	void (*onBackgroundRender)(const std::array<uint8_t, FRAMEBUFFER_SIZE>& buffer, uint8_t LY);
	void (*onWindowRender)(const std::array<pixelInfo, SCR_WIDTH>& updatedPixels, uint8_t LY);
	void (*onOAMRender)(const std::array<pixelInfo, SCR_WIDTH>& updatedPixels, uint8_t LY);

	constexpr void resetCallbacks()
	{
		onBackgroundRender = nullptr;
		onWindowRender = nullptr;
		onOAMRender = nullptr;
	}

	friend MMU;

	PPU(MMU& mmu, CPU& cpu) : mmu(mmu), cpu(cpu)
	{
		setColorsPalette(BGB_GREEN_PALETTE);
		reset();
	}

	void execute();
	void reset();
	constexpr void clearBuffer() { PixelOps::clearBuffer(framebuffer.data(), SCR_WIDTH, SCR_HEIGHT, colors[0]); }

	constexpr const uint8_t* getFrameBuffer() { return framebuffer.data(); }
	void renderTileData(uint8_t* buffer);

	static constexpr std::array<color, 4> GRAY_PALETTE = { color {255, 255, 255}, color {169, 169, 169}, color {84, 84, 84}, color {0, 0, 0} };
	static constexpr std::array<color, 4> CLASSIC_PALETTE = { color {155, 188, 15}, color {139, 172, 15}, color {48, 98, 48}, color {15, 56, 15} };
	static constexpr std::array<color, 4> BGB_GREEN_PALETTE = { color {224, 248, 208}, color {136, 192, 112 }, color {52, 104, 86}, color{8, 24, 32} };

	constexpr void setColorsPalette(const std::array<color, 4>& newColors) { colors = newColors; }
	constexpr const std::array<color, 4> getCurrentPalette() { return colors; }

	void updateScreenColors(const std::array<color, 4>& newColors);
private:
	MMU& mmu;
	CPU& cpu;

	PPUMode state;
	uint8_t VRAM[8192];
	uint16_t videoCycles;
	uint8_t LY;
	uint8_t WLY;

	std::array<uint8_t, FRAMEBUFFER_SIZE> framebuffer;
	std::bitset<SCR_WIDTH> opaqueBackgroundPixels{};

	std::array<pixelInfo, SCR_WIDTH> updatedWindowPixels;
	std::array<pixelInfo, SCR_WIDTH> updatedOAMPixels;
	std::array<pixelInfo, SCR_WIDTH> updatedBGPixels;

	bool dmaTransfer;
	uint8_t dmaCycles;
	uint16_t dmaSourceAddr;

	static constexpr uint8_t DMA_CYCLES = 160;
	static constexpr uint8_t OAM_SCAN_CYCLES = 20;
	static constexpr uint8_t PIXEL_TRANSFER_CYCLES = 43;
	static constexpr uint8_t HBLANK_CYCLES = 51;
	static constexpr uint8_t VBLANK_CYCLES = 114;

	std::array<color, 4> colors;
	constexpr color getColor(uint8_t ind) { return colors[ind]; }

	constexpr void setPixel(uint8_t x, uint8_t y, color c) { PixelOps::setPixel(framebuffer.data(), SCR_WIDTH, x, y, c); }
	constexpr color getPixel(uint8_t x, uint8_t y) { return PixelOps::getPixel(framebuffer.data(), SCR_WIDTH, x, y); }

	std::array<uint8_t, 4> BGpalette;
	std::array<uint8_t, 4> OBP0palette;
	std::array<uint8_t, 4> OBP1palette;

	void updatePalette(uint8_t val, std::array<uint8_t, 4>& palette);
	void startDMATransfer(uint16_t sourceAddr);

	void SetLY(uint8_t val);
	void SetPPUMode(PPUMode ppuState);
	void disableLCD(PPUMode mode = PPUMode::HBlank);

	void handleOAMSearch();
	void handleHBlank();
	void handleVBlank();
	void handlePixelTransfer();

	void renderScanLine();
	void renderBackground();
	void renderWindow();
	void renderOAM();
	void renderBlank();

	inline uint16_t getBGTileAddr(uint8_t tileInd) { return BGUnsignedAddressing() ? tileInd * 16 : 0x1000 + static_cast<int8_t>(tileInd) * 16; }
	void renderBGTile(uint16_t addr, int16_t screenX, uint8_t scrollY, pixelInfo* updatedPixelsBuffer = nullptr);
	void renderObjTile(uint16_t tileAddr, uint8_t attributes, int16_t objX, int16_t objY);

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