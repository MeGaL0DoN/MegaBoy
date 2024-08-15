#pragma once
#include <string>

namespace appConfig
{
	inline bool runBootROM { false };
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
	inline bool fpsLock { true };
	inline bool vsync { true };

	inline int filter { 1 };
	inline int palette { 0 };

	inline std::string romPath{};
	inline int saveStateNum{ 0 };

	inline std::string dmgRomPath{};
	inline std::string cgbRomPath{};

	void loadConfigFile();
	void updateConfigFile();
}