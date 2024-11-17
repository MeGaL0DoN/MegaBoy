#pragma once
#include <string>
#include <array>
#include "Utils/pixelOps.h"

namespace appConfig
{
	inline bool runBootROM { true };
	inline bool loadLastROM { true };
	inline int systemPreference { 0 };

	inline bool autosaveState{ true };
	inline bool batterySaves{ true };

#ifdef EMSCRIPTEN
	inline bool backupSaves{ false };
#else
	inline bool backupSaves{ true };
#endif

	inline bool blending { false };
	inline bool vsync { true };
	inline bool integerScaling { true };
	inline bool bilinearFiltering { false };

	inline bool enableAudio { false }; // TODO: set to true by default later.

	inline int filter { 1 };
	inline int palette { 0 };

	inline std::string romPath{};
	inline int saveStateNum{ 0 };

	inline std::string dmgBootRomPath{};
	inline std::string cgbBootRomPath{};

	void loadConfigFile();
	void updateConfigFile();
}