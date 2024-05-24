#pragma once
#include <fstream>
#include <memory>
#include <vector>
#include "MBCBase.h"
#include "RTCTimer.h"

class GBCore;

class Cartridge
{
public:
	constexpr const std::vector<uint8_t>& getRom() const { return rom; }
	inline MBCBase* getMapper() { return mapper.get(); }

	Cartridge(GBCore& gbCore);

	uint32_t checksum { 0 };
	bool ROMLoaded { false };
	bool readSuccessfully { false };

	bool hasRAM{ false };
	bool hasBattery { false };
	uint16_t romBanks { 0 };
	uint16_t ramBanks { 0 };

	bool hasTimer { false };
	RTCTimer timer{};

	bool loadROM(std::ifstream& ifs);

private:
	void calculateChecksum()
	{
		checksum = 0;

		for (size_t i = 0; i < rom.size(); ++i) 
			checksum += rom[i];
	}

	bool proccessCartridgeHeader(const std::vector<uint8_t>& buffer);

	std::unique_ptr<MBCBase> mapper;
	GBCore& gbCore;
	std::vector<uint8_t> rom;
};