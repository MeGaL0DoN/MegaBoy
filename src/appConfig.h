#pragma once
#include <filesystem>

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

	inline std::filesystem::path romPath{};
	inline int saveStateNum{ 0 };

#ifdef EMSCRIPTEN
	constexpr const char* dmgBootRomPath { "data/dmg_boot.bin" };
	constexpr const char* cgbBootRomPath { "data/cgb_boot.bin" };
#else
	inline std::filesystem::path dmgBootRomPath{};
	inline std::filesystem::path cgbBootRomPath{};
#endif

	void loadConfigFile();
	void updateConfigFile();
}