#include "appConfig.h"
#include <filesystem>
#include <mini/ini.h>
#include "Utils/fileUtils.h"
#include "GBCore.h"
#include "keyBindManager.h"

extern GBCore gbCore;

const auto mINIFilePath = FileUtils::executableFolderPath / "data" / "config.ini";
mINI::INIFile file { mINI::mINIFilePath(mINIFilePath) };
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
	if (!std::filesystem::exists(mINIFilePath))
		return;

	file.read(config);

	to_bool(batterySaves, "options", "batterySaves");
	to_bool(autosaveState, "options", "autosaveState");
	to_bool(loadLastROM, "options", "loadLastROM");
	to_int(systemPreference, "options", "preferredSystem");

	if (config.has("keyBinds"))
	{
		for (int i = 0; i < KeyBindManager::TOTAL_BINDS; i++)
		{
			const char* keyName = KeyBindManager::getMegaBoyKeyName(static_cast<MegaBoyKey>(i));

			if (config["keyBinds"].has(keyName))
				to_int(KeyBindManager::keyBinds[i], "keyBinds", keyName);
		}
	}

	to_bool(blending, "graphics", "blending");
	to_bool(vsync, "graphics", "vsync");
	to_bool(integerScaling, "graphics", "integerScaling");
	to_bool(bilinearFiltering, "graphics", "bilinearFiltering");

	to_int(palette, "graphics", "palette");
	to_int(filter, "graphics", "filter");

	if (config.has("customPalette"))
	{
		for (int i = 0; i < 4; i++)
		{
			const std::string section = "Color " + std::to_string(i);

			if (config["customPalette"].has(section))
				PPU::CUSTOM_PALETTE[i] = color::fromHex(config["customPalette"][section]);
		}
	}

	to_bool(enableAudio, "audio", "enable");
	to_bool(runBootROM, "bootroms", "runBootROM");

#ifndef EMSCRIPTEN
	romPath = config["gameState"]["romPath"];
	to_int(saveStateNum, "gameState", "saveStateNum");

	if (config.get("bootroms").has("dmgBootRomPath"))
		dmgBootRomPath = config["bootRoms"]["dmgBootRomPath"];

	if (config.get("bootroms").has("cgbBootRomPath"))
		cgbBootRomPath = config["bootRoms"]["cgbBootRomPath"];
#endif
}

void appConfig::updateConfigFile()
{
#ifndef EMSCRIPTEN
	const auto dataFolderPath { FileUtils::executableFolderPath / "data" };

	if (!std::filesystem::exists(dataFolderPath))
		std::filesystem::create_directory(dataFolderPath);
#endif

	config["options"]["batterySaves"] = to_string(batterySaves);
	config["options"]["autosaveState"] = to_string(autosaveState);
	config["options"]["loadLastROM"] = to_string(loadLastROM);
	config["options"]["preferredSystem"] = std::to_string(systemPreference);

	if (KeyBindManager::keyBinds != KeyBindManager::defaultKeyBinds() || config.has("keyBinds"))
	{
		for (int i = 0; i < KeyBindManager::TOTAL_BINDS; i++)
			config["keyBinds"][KeyBindManager::getMegaBoyKeyName(static_cast<MegaBoyKey>(i))] = std::to_string(KeyBindManager::keyBinds[i]);
	}

	config["graphics"]["blending"] = to_string(blending);
	config["graphics"]["vsync"] = to_string(vsync);
	config["graphics"]["integerScaling"] = to_string(integerScaling);
	config["graphics"]["bilinearFiltering"] = to_string(bilinearFiltering);
	config["graphics"]["palette"] = std::to_string(palette);
	config["graphics"]["filter"] = std::to_string(filter);

	if (PPU::CUSTOM_PALETTE != PPU::DEFAULT_CUSTOM_PALETTE || config.has("customPalette"))
	{
		for (int i = 0; i < 4; i++)
			config["customPalette"]["Color " + std::to_string(i)] = PPU::CUSTOM_PALETTE[i].toHex();
	}

	config["audio"]["enable"] = to_string(enableAudio);
	config["bootroms"]["runBootROM"] = to_string(runBootROM);

#ifndef EMSCRIPTEN
	if (gbCore.cartridge.ROMLoaded())
	{
		config["gameState"]["romPath"] = FileUtils::pathToUTF8(gbCore.getROMPath());
		config["gameState"]["saveStateNum"] = std::to_string(gbCore.getSaveNum());
	}
	else
		config.remove("gameState");

	if (!dmgBootRomPath.empty())
		config["bootroms"]["dmgBootRomPath"] = FileUtils::pathToUTF8(dmgBootRomPath);

	if (!cgbBootRomPath.empty())
		config["bootroms"]["cgbBootRomPath"] = FileUtils::pathToUTF8(cgbBootRomPath);
#endif

	(void)file.generate(config, true);
}