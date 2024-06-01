#pragma once
#include <string>

namespace appConfig
{
	inline bool runBootROM { false };
	inline bool batterySaves { true };;
	inline bool pauseOnFocus { false };
	inline bool autosaveState { true };
	inline bool loadLastROM { true };

	inline bool blending { false };
	inline bool fpsLock { true };
	inline bool vsync { true };

	inline int filter { 0 };
	inline int palette { 0 };

	inline std::string romPath{};
	inline int saveStateNum{ 0 };

	void loadConfigFile();
	void updateConfigFile();
}