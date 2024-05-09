#pragma once
#include <fstream>
#include <memory>
#include "MBC.h"

class GBCore;

class Cartridge
{
public:
	constexpr const std::vector<uint8_t>& getRom() const { return rom; }
	inline MBC* getMapper() { return mapper.get(); }

	Cartridge(GBCore& gbCore);

	bool ROMLoaded { false };
	bool readSuccessfully { false };

	bool hasRAM{ false };
	bool hasBattery { false };
	uint16_t romBanks{0};
	uint16_t ramBanks{0};

	bool loadROM(std::ifstream& ifs);

	template <typename T>
	inline bool loadROM(T path)
	{
		std::ifstream ifs(path, std::ios::binary | std::ios::ate);
		return loadROM(ifs);
	}

//	std::ofstream saveStream;
	void saveGame();

private:
	//std::string gameSaveFile;

	//static constexpr const char* SAVE_FILE_SIGNATURE = "MegaBoy Emulator Save File";

	//bool loadGameROM();
	//bool loadSaveFile();

	bool proccessCartridgeHeader(const std::vector<uint8_t>& buffer);

	std::unique_ptr<MBC> mapper;
	GBCore& gbCore;
	std::vector<uint8_t> rom;
};