#pragma once
#include <string>

namespace appConfig
{
	inline bool runBootROM { true };
	inline bool loadLastROM { true };
	inline int systemPreference { 0 };

	inline bool autosaveState{ true };
	inline bool batterySaves{ true };

	inline bool blending { false };
	inline bool vsync { true };
	inline bool integerScaling { true };
	inline bool bilinearFiltering { false };

	inline bool enableAudio { false }; // TODO: set to true by default later.

	inline int filter { 1 };
	inline int palette { 0 };

	inline std::string romPath{};
	inline int saveStateNum{ 0 };

#ifdef EMSCRIPTEN
	constexpr const char* dmgBootRomPath { "bootroms/dmg_boot.bin" };
	constexpr const char* cgbBootRomPath { "bootroms/cgb_boot.bin" };
#else
	inline std::string dmgBootRomPath{};
	inline std::string cgbBootRomPath{};
#endif

	void loadConfigFile();
	void updateConfigFile();
}