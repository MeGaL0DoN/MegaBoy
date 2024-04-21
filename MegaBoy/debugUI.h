#pragma once
#include <vector>
#include <array>
#include <string>
#include "GBCore.h"

class debugUI
{
public:
	static void updateMenu(GBCore& gbCore);
	static void updateWindows(GBCore& gbCore);
private:
	static inline bool showMemoryView { false };
	static inline bool firstMemoryViewOpen { true };

	static inline bool realTimeMemView { false };
	static inline std::vector<std::string> memoryData;

	static inline bool firstVRAMView { true };
	static inline std::vector<uint8_t> backgroundFramebuffer;
	static inline std::vector<uint8_t> tileMapFrameBuffer;

	static inline bool showVRAMView { false };
	static inline uint32_t backgroundTexture;
	static inline uint32_t tileMapTexture;
};