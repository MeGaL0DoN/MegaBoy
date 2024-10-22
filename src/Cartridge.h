#pragma once
#include <fstream>
#include <memory>
#include <vector>
#include "Mappers/MBCBase.h"
#include "Mappers/RTCTimer.h"

class GBCore;

class Cartridge
{
public:
	constexpr const std::vector<uint8_t>& getRom() const { return rom; }
	inline MBCBase* getMapper() const { return mapper.get(); }

	explicit Cartridge(GBCore& gbCore);

	uint8_t checksum { 0 };
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
	bool verifyChecksum(const std::vector<uint8_t>& buffer)
	{
		checksum = 0;

		for (size_t i = 0x134; i <= 0x14C; i++)
			checksum = checksum - buffer[i] - 1;

		return checksum == buffer[0x14D];
	}

	bool proccessCartridgeHeader(const std::vector<uint8_t>& buffer);

	std::unique_ptr<MBCBase> mapper { nullptr };
	GBCore& gbCore;
	std::vector<uint8_t> rom{};

	static constexpr int MIN_ROM_SIZE = 0x8000;
	static constexpr int MAX_ROM_SIZE = 0x800000;
};