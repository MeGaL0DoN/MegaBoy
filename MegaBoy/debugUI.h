#pragma once
#include <array>
#include <string>
#include <memory>
#include "GBCore.h"

extern GBCore gbCore;

class debugUI
{
public:
	static void updateMenu();
	static void updateWindows();

	static constexpr void clearBuffers()
	{
		clearTileDataBuffer();
		clearBGBuffer(BGFrameBuffer.get());
		clearBGBuffer(windowFrameBuffer.get());
		clearBGBuffer(OAMFrameBuffer.get());
	}
private:
	static inline bool showMemoryView { false };
	static inline bool realTimeMemView { false };
	static inline std::unique_ptr<std::string[]> memoryData;

	static inline std::unique_ptr<uint8_t[]> BGFrameBuffer;
	static inline std::unique_ptr<uint8_t[]> windowFrameBuffer;
	static inline std::unique_ptr<uint8_t[]> OAMFrameBuffer;
	static inline std::unique_ptr<uint8_t[]> tileDataFrameBuffer;

	static inline bool showVRAMView { false };
	static inline uint32_t backgroundTexture {0};
	static inline uint32_t tileDataTexture {0};

	static constexpr void clearBGBuffer(uint8_t* buffer) { PixelOps::clearBuffer(buffer, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu.getCurrentPalette()[0]); }
	static constexpr void clearTileDataBuffer() { PixelOps::clearBuffer(tileDataFrameBuffer.get(), PPU::TILES_SIZE, PPU::TILES_SIZE, gbCore.ppu.getCurrentPalette()[0]); }

	static constexpr void clearBGScanline(uint8_t* buffer, uint8_t LY)
	{
		for (int x = 0; x < PPU::SCR_WIDTH; x++)
			PixelOps::setPixel(buffer, PPU::SCR_WIDTH, x, LY ,gbCore.ppu.getCurrentPalette()[0]);
	}

	friend void backgroundRenderEvent(const std::array<uint8_t, PPU::FRAMEBUFFER_SIZE>& buffer, uint8_t LY);
	friend void OAM_renderEvent(const std::array<pixelInfo, PPU::SCR_WIDTH>& updatedPixels, uint8_t LY);
	friend void windowRenderEvent(const std::array<pixelInfo, PPU::SCR_WIDTH>& updatedPixels, uint8_t LY);
};