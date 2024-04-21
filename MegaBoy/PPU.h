#pragma once
#include "MMU.h"
#include "CPU.h"
#include "bitOps.h"
#include <array>
#include <bitset>

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

class debugUI;

class PPU
{
public:
	friend MMU;
	friend debugUI;

	static constexpr uint8_t SCR_WIDTH = 160;
	static constexpr uint8_t SCR_HEIGHT = 144;
	static constexpr uint32_t FRAMEBUFFER_SIZE = SCR_WIDTH * SCR_HEIGHT * 3;

	PPU(MMU& mmu, CPU& cpu) : mmu(mmu), cpu(cpu), currentBuffer(framebuffer)
	{
		setColorsPalette(BGB_GREEN_PALETTE);
		reset();
	}

	void execute();
	void reset();
	void clearBuffer();

	constexpr const uint8_t* getFrameBuffer() { return framebuffer; }
	constexpr void bindFrameBuffer(uint8_t* buffer) { currentBuffer = buffer; }
	constexpr void unbindFrameBuffer() { currentBuffer = framebuffer; }

	static constexpr std::array<color, 4> GRAY_PALETTE = { color {255, 255, 255}, color {169, 169, 169}, color {84, 84, 84}, color {0, 0, 0} };
	static constexpr std::array<color, 4> CLASSIC_PALETTE = { color {155, 188, 15}, color {139, 172, 15}, color {48, 98, 48}, color {15, 56, 15} };
	static constexpr std::array<color, 4> BGB_GREEN_PALETTE = { color {224, 248, 208}, color {136, 192, 112 }, color {52, 104, 86}, color{8, 24, 32} };

	constexpr void setColorsPalette(const std::array<color, 4>& newColors) { colors = newColors; }
	void updateScreenColors(const std::array<color, 4>& newColors);
private:
	MMU& mmu;
	CPU& cpu;

	PPUMode state;
	uint8_t VRAM[8192];
	uint16_t videoCycles;
	uint8_t LY;
	uint8_t WLY;

	uint8_t* currentBuffer;
	uint8_t framebuffer[FRAMEBUFFER_SIZE];

	std::bitset<SCR_WIDTH> opaqueBackgroundPixels{};

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

	constexpr void setPixel(uint8_t x, uint8_t y, color c)
	{
		uint32_t baseInd = (y * SCR_WIDTH * 3) + (x * 3);

		currentBuffer[baseInd] = c.R; 
		currentBuffer[baseInd + 1] = c.G;
		currentBuffer[baseInd + 2] = c.B;
	}
	constexpr color getPixel(uint8_t x, uint8_t y)
	{
		uint32_t baseInd = (y * SCR_WIDTH * 3) + (x * 3);

		return color
		{
			currentBuffer[baseInd],
			currentBuffer[baseInd + 1],
			currentBuffer[baseInd + 2]
		};
	}

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
	void renderBackground(uint8_t LY, bool saveTransparencyValues);
	void renderWindow(uint8_t LY, uint8_t& WLY, bool saveTransparencyValues);
	void renderOAM(uint8_t LY, bool checkBgTransparency);
	void renderBlank();

	void renderTileMap(uint8_t* buffer);

	inline uint16_t getBGTileAddr(uint8_t tileInd) { return BGUnsignedAddressing() ? tileInd * 16 : 0x1000 + static_cast<int8_t>(tileInd) * 16; }
	void renderBGTile(uint16_t addr, uint8_t LY, uint8_t screenX, uint8_t scrollY, bool saveTransparencyValues);
	void renderObjTile(uint16_t tileAddr, uint8_t LY, uint8_t attributes, int16_t objX, int16_t objY, bool checkBgTransparency);

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