#pragma once
#include <string>
#include <memory>
#include <vector>
#include "GBCore.h"
#include "glFunctions.h"

extern GBCore gbCore;

class debugUI
{
public:
	static void updateMenu();
	static void updateWindows(float scaleFactor);

	static constexpr void clearBuffers()
	{
		clearBGBuffer(BGFrameBuffer.get());
		clearBGBuffer(windowFrameBuffer.get());
		clearBGBuffer(OAMFrameBuffer.get());
	}

	static constexpr void updateTextures()
	{
		if (backgroundTexture) OpenGL::updateTexture(backgroundTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, BGFrameBuffer.get());
		if (windowTexture) OpenGL::updateTexture(windowTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, windowFrameBuffer.get());
		if (OAMTexture) OpenGL::updateTexture(OAMTexture, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, OAMFrameBuffer.get());
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
	static inline uint32_t windowTexture{ 0 };
	static inline uint32_t OAMTexture{ 0 };
	static inline uint32_t tileDataTexture {0};

	static constexpr void clearBGBuffer(uint8_t* buffer) { PixelOps::clearBuffer(buffer, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, gbCore.ppu.getCurrentPalette()[0]); }

	static inline void updateTileData()
	{
		gbCore.ppu.renderTileData(tileDataFrameBuffer.get());
		OpenGL::updateTexture(tileDataTexture, PPU::TILES_WIDTH, PPU::TILES_HEIGHT, tileDataFrameBuffer.get());
	}

	static constexpr void clearBGScanline(uint8_t* buffer, uint8_t LY)
	{
		for (int x = 0; x < PPU::SCR_WIDTH; x++)
			PixelOps::setPixel(buffer, PPU::SCR_WIDTH, x, LY ,gbCore.ppu.getCurrentPalette()[0]);
	}

	static void backgroundRenderEvent(const uint8_t* buffer, uint8_t LY);
	static void OAM_renderEvent(const uint8_t* buffer, const std::vector<uint8_t>& updatedPixels, uint8_t LY);
	static void windowRenderEvent(const uint8_t* buffer, const std::vector<uint8_t>& updatedPixels, uint8_t LY);
};