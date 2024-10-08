#pragma once
#include <string>
#include <array>
#include "Utils/pixelOps.h"

namespace appConfig
{
	inline bool runBootROM { true };
	inline bool pauseOnFocus { false };
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

	inline bool enableAudio { true };

	inline int filter { 1 };
	inline int palette { 0 };

	inline std::string romPath{};
	inline int saveStateNum{ 0 };

	inline std::string dmgRomPath{};
	inline std::string cgbRomPath{};

	void loadConfigFile();
	void updateConfigFile();
}