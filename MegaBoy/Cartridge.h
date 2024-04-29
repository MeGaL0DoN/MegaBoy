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

	uint16_t romBanks;
	uint16_t ramBanks;

	void loadROM(std::ifstream& ifs);

	template <typename T>
	inline void loadROM(T path)
	{
		std::ifstream ifs(path, std::ios::binary | std::ios::ate);
		loadROM(ifs);
	}
private:
	void proccessCartridgeHeader();

	std::unique_ptr<MBC> mapper;
	GBCore& gbCore;
	std::vector<uint8_t> rom;
};