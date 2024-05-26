#include "appConfig.h"
#include <filesystem>
#include <mini/ini.h>

using namespace appConfig;

mINI::INIFile file{ "data/config.ini" };
mINI::INIStructure config;

inline void to_bool(bool& val, const char* section, const char* valName)
{
	if (config[section].has(valName))
		val = config.get(section).get(valName) == "true";
}

inline void to_int(int& val, const char* section, const char* valName)
{
	if (config[section].has(valName))
	{
		std::stringstream ss(config[section][valName]);
		ss >> val;
	}
}

constexpr std::string to_string(bool val)
{
	return val ? "true" : "false";
}

void appConfig::loadConfigFile()
{
	file.read(config);

	if (!config["options"].has("runBootROM"))
		runBootROM = std::filesystem::exists("data/boot_rom.bin");
	else
		to_bool(runBootROM,"options", "runBootROM");

	to_bool(batterySaves, "options", "batterySaves");
	to_bool(pauseOnFocus, "options", "pauseOnFocus");
	to_bool(autosaveState, "options", "autosaveState");

	to_bool(blending, "graphics", "blending");
	to_bool(vsync, "graphics", "vsync");
	to_bool(fpsLock, "graphics", "fpsLock");

	to_int(palette, "graphics", "palette");
	to_int(filter, "graphics", "filter");
}

void appConfig::updateConfigFile()
{
	config["options"]["runBootROM"] = to_string(runBootROM);
	config["options"]["batterySaves"] = to_string(batterySaves);
	config["options"]["pauseOnFocus"] = to_string(pauseOnFocus);
	config["options"]["autosaveState"] = to_string(autosaveState);

	config["graphics"]["blending"] = to_string(blending);
	config["graphics"]["vsync"] = to_string(vsync);
	config["graphics"]["fpsLock"] = to_string(fpsLock);

	config["graphics"]["palette"] = std::to_string(palette);
	config["graphics"]["filter"] = std::to_string(filter);

	file.generate(config, true);
}