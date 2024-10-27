#include "appConfig.h"
#include <filesystem>
#include <mini/ini.h>
#include "Utils/fileUtils.h"
#include "GBCore.h"

using namespace appConfig;

extern GBCore gbCore;

mINI::INIFile file { mINI::mINIFilePath(FileUtils::executableFolderPath / "data" / "config.ini") };
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

inline std::string to_string(bool val)
{
	return val ? "true" : "false";
}

void appConfig::loadConfigFile()
{
#ifdef EMSCRIPTEN
	return; // currently not saving settings persistently on the web
#endif

	const auto dataFolderPath { FileUtils::executableFolderPath / "data" };

	if (!std::filesystem::exists(dataFolderPath))
		std::filesystem::create_directory(dataFolderPath);

	file.read(config);
	
	to_bool(runBootROM, "options", "runBootROM");

	to_bool(batterySaves, "options", "batterySaves");
	to_bool(pauseOnFocus, "options", "pauseOnFocus");
	to_bool(autosaveState, "options", "autosaveState");
	to_bool(backupSaves, "options", "backupSaves");
	to_bool(loadLastROM, "options", "loadLastROM");

	to_bool(blending, "graphics", "blending");
	to_bool(vsync, "graphics", "vsync");

	to_int(palette, "graphics", "palette");
	to_int(filter, "graphics", "filter");

	for (int i = 0; i < 4; i++)
	{
		const std::string section = "Color " + std::to_string(i);

		if (config["customPalette"].has(section))
			PPU::CUSTOM_PALETTE[i] = color::fromHex(config["customPalette"][section]);
	}

	to_bool(enableAudio, "audio", "enable");

	to_int(systemPreference, "system", "preferredSystem");

	romPath = config["gameState"]["romPath"];
	to_int(saveStateNum, "gameState", "saveStateNum");

	dmgRomPath = config["bootRoms"]["dmgRomPath"];
	cgbRomPath = config["bootRoms"]["cgbRomPath"];
}

void appConfig::updateConfigFile()
{
#ifdef EMSCRIPTEN
	return; // currently not saving settings persistently on the web
#endif

	config["options"]["runBootROM"] = to_string(runBootROM);
	config["options"]["batterySaves"] = to_string(batterySaves);
	config["options"]["pauseOnFocus"] = to_string(pauseOnFocus);
	config["options"]["autosaveState"] = to_string(autosaveState);
	config["options"]["backupSaves"] = to_string(backupSaves);
	config["options"]["loadLastROM"] = to_string(loadLastROM);

	config["graphics"]["blending"] = to_string(blending);
	config["graphics"]["vsync"] = to_string(vsync);
	config["graphics"]["palette"] = std::to_string(palette);
	config["graphics"]["filter"] = std::to_string(filter);

	if (PPU::CUSTOM_PALETTE != PPU::DEFAULT_CUSTOM_PALETTE) 
	{
		for (int i = 0; i < 4; i++)
			config["customPalette"]["Color " + std::to_string(i)] = PPU::CUSTOM_PALETTE[i].toHex();
	}

	config["audio"]["enable"] = to_string(enableAudio);

	config["system"]["preferredSystem"] = std::to_string(systemPreference);

	if (gbCore.cartridge.ROMLoaded)
	{
		config["gameState"]["romPath"] = FileUtils::pathToUTF8(gbCore.getROMPath());
		config["gameState"]["saveStateNum"] = std::to_string(gbCore.getSaveNum());
	}
	else
		config.remove("gameState");

	if (!dmgRomPath.empty())
		config["bootRoms"]["dmgRomPath"] = FileUtils::pathToUTF8(dmgRomPath);

	if (!cgbRomPath.empty())
		config["bootRoms"]["cgbRomPath"] = FileUtils::pathToUTF8(cgbRomPath);

	file.generate(config, true);
}