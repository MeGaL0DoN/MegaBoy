#pragma once
#include <fstream>
#include <memory>
#include "MBC.h"

class GBCore;

class Cartridge
{
public:
	Cartridge(GBCore& gbCore);

	bool ROMLoaded { false };
	std::unique_ptr<MBC> mapper;

	void loadROM(std::ifstream& ifs);

	template <typename T>
	inline void loadROM(T path)
	{
		std::ifstream ifs(path, std::ios::binary | std::ios::ate);
		loadROM(ifs);
	}
private:
	GBCore& gbCore;
	std::unique_ptr<uint8_t[]> data;
};