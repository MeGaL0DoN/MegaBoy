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

private:
	static inline bool showMemoryView { false };
	static inline bool showCPUView { false };
	static inline bool showVRAMView{ false };
	static inline bool showAudioView{ false };

	static inline bool realTimeMemView { false };
	static inline std::unique_ptr<std::string[]> memoryData;

	static inline std::unique_ptr<uint8_t[]> BGFrameBuffer;
	static inline std::unique_ptr<uint8_t[]> windowFrameBuffer;
	static inline std::unique_ptr<uint8_t[]> tileDataFrameBuffer;

	static inline uint32_t backgroundTexture {0};
	static inline uint32_t windowTexture{ 0 };
	static inline uint32_t tileDataTexture {0};

	static inline void clearBuffer(uint8_t* buffer, uint16_t width, uint16_t height) { PixelOps::clearBuffer(buffer, width, height, PPU::ColorPalette[0]); }
};