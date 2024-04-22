#pragma once
#include <array>
#include <string>
#include <memory>
#include "GBCore.h"

class debugUI
{
public:
	static void updateMenu(GBCore& gbCore);
	static void updateWindows(GBCore& gbCore);

	static constexpr void clearBGBuffer() { PixelOps::clearBuffer(BGFrameBuffer.get(), PPU::SCR_WIDTH, PPU::SCR_HEIGHT, color { 255, 255, 255 }); }
	static constexpr void clearTileDataBuffer() { PixelOps::clearBuffer(tileDataFrameBuffer.get(), PPU::TILES_SIZE, PPU::TILES_SIZE, color{ 255, 255, 255 }); }
private:
	static inline bool showMemoryView { false };
	static inline bool realTimeMemView { false };
	static inline std::unique_ptr<std::string[]> memoryData;

	static inline std::unique_ptr<uint8_t[]> BGFrameBuffer;
	static inline std::unique_ptr<uint8_t[]> tileDataFrameBuffer;

	static inline bool showVRAMView { false };
	static inline uint32_t backgroundTexture;
	static inline uint32_t tileDataTexture;

	static inline bool firstMemoryViewOpen{ true };
	static inline bool firstVRAMViewOpen { true };
	static inline bool firstVRAMTileDataOpen { true };

	static constexpr void setBGPixel(uint8_t x, uint8_t y, color c) { PixelOps::setPixel(BGFrameBuffer.get(), PPU::SCR_WIDTH, x, y, c); }
	static constexpr void clearBGScanline(uint8_t LY)
	{
		for (int x = 0; x < PPU::SCR_WIDTH; x++)
			setBGPixel(x, LY, color{ 255, 255, 255 });
	}

	friend void backgroundRenderEvent(const std::array<uint8_t, PPU::FRAMEBUFFER_SIZE>& buffer, uint8_t LY);
	friend void OAM_Window_renderEvent(const std::array<pixelInfo, PPU::SCR_WIDTH>& updatedPixels, uint8_t LY);
};