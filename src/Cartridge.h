#pragma once

#include <memory>
#include <vector>

#include "Mappers/MBCBase.h"
#include "Mappers/RTC.h"
#include "Utils/bitOps.h"
#include "appConfig.h"
#include "gbSystem.h"

class GBCore;

class Cartridge
{
public:
	static constexpr uint32_t MIN_ROM_SIZE = 0x4000;
	static constexpr uint32_t MAX_ROM_SIZE = 0x800000;

	static constexpr bool romSizeValid(uint32_t size) { return size >= MIN_ROM_SIZE && size <= MAX_ROM_SIZE; }
	static inline bool romSizeValid(std::istream& st)
	{
		st.seekg(0, std::ios::end);
		return romSizeValid(st.tellg());
	}

	explicit Cartridge(GBCore& gbCore);

	inline MBCBase* getMapper() const { return mapper.get(); }

	// MBC6 checks.
	constexpr uint16_t romBankSize() const { return mapperID == 0x20 ? 0x2000 : 0x4000; }
	constexpr uint16_t ramBankSize() const { return mapperID == 0x20 ? 0x1000 : 0x2000; }

	constexpr bool loaded() const { return romLoaded; }
	constexpr uint8_t getChecksum() const { return checksum; }

	uint64_t getGBCycles() const;

	bool hasRAM { false };
	bool hasBattery { false };

	uint16_t romBanks { 2 };
	uint16_t ramBanks { 0 };

	RTC* rtc { nullptr };

	std::vector<uint8_t> rom{};
	std::vector<uint8_t> ram{};

	bool loadROM(std::istream& st);
	void unload();

	static uint8_t calculateHeaderChecksum(std::istream& st);

	inline void updateSystem()
	{
		if (!romLoaded) return;
		updateSystem(rom[0x143]);
	}

	inline bool isDMGCompatSystem() const
	{
		if (!romLoaded || System::Current() != GBSystem::CGB)
			return false;

		return appConfig::systemPreference == GBSystemPreference::ForceCGB && !getBit(rom[0x143], 7);
	}
private:
	bool processCartridgeHeader(std::istream& st);
	static void updateSystem(uint8_t cgbFlag);

	GBCore& gb;
	std::unique_ptr<MBCBase> mapper;
	uint8_t mapperID { 0x00 };

	bool romLoaded { false };
	uint8_t checksum { 0 };
};