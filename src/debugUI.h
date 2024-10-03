#pragma once
#include <string>
#include <memory>
#include <vector>
#include "GBCore.h"
#include "Utils/glFunctions.h"
#include "Utils/pixelOps.h"

extern GBCore gbCore;

class debugUI
{
public:
	static void updateMenu();
	static void updateWindows(float scaleFactor);
	static void updateTextures(bool all);

	static inline void clearBuffers()
	{
		clearBGBuffer(BGFrameBuffer.get());
		clearBGBuffer(windowFrameBuffer.get());
		clearBGBuffer(OAMFrameBuffer.get());
	}

private:
	static inline bool showMemoryView { false };
	static inline bool showCPUView { false };
	static inline bool showVRAMView{ false };
	static inline bool showAudioView{ false };

	static inline bool realTimeMemView { false };
	static inline std::unique_ptr<std::string[]> memoryData;

	static inline std::unique_ptr<uint8_t[]> BGFrameBuffer;
	static inline std::unique_ptr<uint8_t[]> windowFrameBuffer;
	static inline std::unique_ptr<uint8_t[]> OAMFrameBuffer;
	static inline std::unique_ptr<uint8_t[]> tileDataFrameBuffer;

	static inline uint32_t backgroundTexture {0};
	static inline uint32_t windowTexture{ 0 };
	static inline uint32_t OAMTexture{ 0 };
	static inline uint32_t tileDataTexture {0};

	static inline void clearBGBuffer(uint8_t* buffer) { PixelOps::clearBuffer(buffer, PPU::SCR_WIDTH, PPU::SCR_HEIGHT, PPU::ColorPalette[0]); }
	static inline void clearTileDataBuffer() { PixelOps::clearBuffer(tileDataFrameBuffer.get(), PPU::TILES_WIDTH, PPU::TILES_HEIGHT, PPU::ColorPalette[0]); }

	static constexpr void clearBGScanline(uint8_t* buffer, uint8_t LY)
	{
		for (uint8_t x = 0; x < PPU::SCR_WIDTH; x++)
			PixelOps::setPixel(buffer, PPU::SCR_WIDTH, x, LY, PPU::ColorPalette[0]);
	}

	static void backgroundRenderEvent(const uint8_t* buffer, uint8_t LY);
	static void OAM_renderEvent(const uint8_t* buffer, const std::vector<uint8_t>& updatedPixels, uint8_t LY);
	static void windowRenderEvent(const uint8_t* buffer, const std::vector<uint8_t>& updatedPixels, uint8_t LY);
};