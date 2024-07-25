#include "appConfig.h"
#include <filesystem>
#include <mini/ini.h>
#include "stringUtils.h"
#include "GBCore.h"

using namespace appConfig;

extern GBCore gbCore;

mINI::INIFile file { mINI::mINIFilePath(StringUtils::executableFolderPath / "data" / "config.ini") };
mINI::INIStructure config;

inline void to_bool(bool& val, const char* section, const char* valName)
{
	if (config[section].has(valName))
		val = config[section][valName] == "true";
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
	const auto dataFolderPath { StringUtils::executableFolderPath / "data" };

	if (!std::filesystem::exists(dataFolderPath))
		std::filesystem::create_directory(dataFolderPath);

	file.read(config);
	
	to_bool(runBootROM, "options", "runBootROM");

	to_bool(batterySaves, "options", "batterySaves");
	to_bool(pauseOnFocus, "options", "pauseOnFocus");
	to_bool(autosaveState, "options", "autosaveState");
	to_bool(loadLastROM, "options", "loadLastROM");

	to_bool(blending, "graphics", "blending");
	to_bool(vsync, "graphics", "vsync");
	to_bool(fpsLock, "graphics", "fpsLock");

	to_int(palette, "graphics", "palette");
	to_int(filter, "graphics", "filter");
	to_int(systemPreference, "system", "preferredSystem");

	romPath = config["gameState"]["romPath"];
	to_int(saveStateNum, "gameState", "saveStateNum");
}

void appConfig::updateConfigFile()
{
	config["options"]["runBootROM"] = to_string(runBootROM);
	config["options"]["batterySaves"] = to_string(batterySaves);
	config["options"]["pauseOnFocus"] = to_string(pauseOnFocus);
	config["options"]["autosaveState"] = to_string(autosaveState);
	config["options"]["loadLastROM"] = to_string(loadLastROM);

	config["graphics"]["blending"] = to_string(blending);
	config["graphics"]["vsync"] = to_string(vsync);
	config["graphics"]["fpsLock"] = to_string(fpsLock);

	config["graphics"]["palette"] = std::to_string(palette);
	config["graphics"]["filter"] = std::to_string(filter);
	config["system"]["preferredSystem"] = std::to_string(systemPreference);

	if (gbCore.cartridge.ROMLoaded)
	{
		config["gameState"]["romPath"] = StringUtils::pathToUTF8(gbCore.getROMPath());
		config["gameState"]["saveStateNum"] = std::to_string(gbCore.getSaveNum());
	}
	else
		config.remove("gameState");

	file.generate(config, true);
}