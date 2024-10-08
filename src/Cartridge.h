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

		for (uint8_t i : rom)
			checksum += i;
	}

	bool proccessCartridgeHeader(const std::vector<uint8_t>& buffer);

	std::unique_ptr<MBCBase> mapper { nullptr };
	GBCore& gbCore;
	std::vector<uint8_t> rom{};

	static constexpr int MIN_ROM_SIZE = 0x8000;
	static constexpr int MAX_ROM_SIZE = 0x800000;
};