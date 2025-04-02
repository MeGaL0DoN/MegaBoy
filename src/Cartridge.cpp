#include <algorithm>

#include "Cartridge.h"
#include "GBCore.h"

#include "Mappers/NoMBC.h"
#include "Mappers/MBC1.h"
#include "Mappers/MBC2.h"
#include "Mappers/MBC3.h"
#include "Mappers/MBC5.h"
#include "Mappers/HuC1.h"
#include "Mappers/HuC3.h"

Cartridge::Cartridge(GBCore& gbCore) : gb(gbCore), rom(std::vector<uint8_t>(MIN_ROM_SIZE * 2, 0xFF)), mapper(std::make_unique<RomOnlyMBC>(*this))
{}

uint64_t Cartridge::getGBCycles() const { return gb.totalCycles(); }

void Cartridge::unload()
{
	romLoaded = false;
	hasRAM = false;
	hasBattery = false;
	romBanks = 2;
	ramBanks = 0;

	rom.resize(MIN_ROM_SIZE * 2);
	std::memset(rom.data(), 0xFF, MIN_ROM_SIZE * 2);
	rom.shrink_to_fit();

	ram.clear();
	ram.shrink_to_fit();

	mapper = std::make_unique<RomOnlyMBC>(*this);
}

bool Cartridge::loadROM(std::istream& is)
{
	is.seekg(0, std::ios::end);
	const uint32_t size = is.tellg();
	
	if (!romSizeValid(size))
		return false;

	if (!proccessCartridgeHeader(is, size))
		return false;

	// 16 KB
	if (size == MIN_ROM_SIZE)
	{
		// Pad to 32 KB
		rom.resize(MIN_ROM_SIZE * 2);
		std::memset(rom.data() + MIN_ROM_SIZE, 0xFF, MIN_ROM_SIZE);
	}
	else
		rom.resize(size);

	rom.shrink_to_fit();
	is.seekg(0, std::ios::beg);
	is.read(reinterpret_cast<char*>(rom.data()), size);

	romLoaded = true;
	return true;
}

uint8_t Cartridge::calculateHeaderChecksum(std::istream& is) const
{
	uint8_t checksum = 0;
	is.seekg(0x134, std::ios::beg);

	for (int i = 0x134; i <= 0x14C; i++)
	{
		uint8_t byte;
		is.read(reinterpret_cast<char*>(&byte), 1);
		checksum = checksum - byte - 1;
	}

	return checksum;
}

void Cartridge::updateSystem(uint8_t cgbFlag)
{
	// If cgb flag is 0x80, game has CGB features but is backwards compatible with DMG. If its 0xC0, game is CGB-only. Any other value with bit 7 unset is DMG-only features.

	switch (static_cast<GBSystemPreference>(appConfig::systemPreference))
	{
	case GBSystemPreference::PreferCGB:
		System::Set(getBit(cgbFlag, 7) ? GBSystem::CGB : GBSystem::DMG);
		break;
	case GBSystemPreference::PreferDMG:
		System::Set(cgbFlag == 0xC0 ? GBSystem::CGB : GBSystem::DMG);
		break;
	case GBSystemPreference::ForceDMG:
		System::Set(GBSystem::DMG);
		break;
	case GBSystemPreference::ForceCGB:
		System::Set(GBSystem::CGB);
		break;
	}
}

bool Cartridge::proccessCartridgeHeader(std::istream& is, uint32_t fileSize)
{
	const auto readByte = [&is](uint16_t ind) -> uint8_t
	{
		uint8_t byte;
		is.seekg(ind, std::ios::beg);
		is.read(reinterpret_cast<char*>(&byte), 1);
		return byte;
	};

	const uint8_t checksum { calculateHeaderChecksum(is) };
	const uint8_t storedChecksum { readByte(0x14D) };

	if (checksum != storedChecksum) 
		return false;

	const uint16_t romBanks = 1 << (readByte(0x148) + 1);

	if (romBanks > std::max(fileSize, MIN_ROM_SIZE * 2) / ROM_BANK_SIZE) 
		return false;

	const bool initialHasBattery { hasBattery };
	hasBattery = false;

	switch (readByte(0x147)) // MBC Type
	{
	case 0x00:
		mapper = std::make_unique<RomOnlyMBC>(*this);
		break;
	case 0x01:
	case 0x02:
		mapper = std::make_unique<MBC1>(*this);
		break;
	case 0x03:
		hasBattery = true;
		mapper = std::make_unique<MBC1>(*this);
		break;
	case 0x05:
		mapper = std::make_unique<MBC2>(*this);
		break;
	case 0x06:
		hasBattery = true;
		mapper = std::make_unique<MBC2>(*this);
		break;
	case 0x0F:
	case 0x10:
		hasBattery = true; 
		mapper = std::make_unique<MBC3>(*this, true);
		break;
	case 0x11:
	case 0x12:
		mapper = std::make_unique<MBC3>(*this, false);
		break;
	case 0x13:
		hasBattery = true;
		mapper = std::make_unique<MBC3>(*this, false);
		break;
	case 0x19:
	case 0x1A:
		mapper = std::make_unique<MBC5>(*this, false);
		break;
	case 0x1B:
		hasBattery = true;
		mapper = std::make_unique<MBC5>(*this, false);
		break;
	case 0x1C:
	case 0x1D:
		mapper = std::make_unique<MBC5>(*this, true);
		break;
	case 0x1E:
		hasBattery = true;
		mapper = std::make_unique<MBC5>(*this, true);
		break;
	case 0xFE:
		hasBattery = true;
		mapper = std::make_unique<HuC3>(*this);
		break;
	case 0xFF:
		hasBattery = true;
		mapper = std::make_unique<HuC1>(*this);
		break;

	default:
		std::cout << "Unknown MBC! \n";
		hasBattery = initialHasBattery;  
		return false;
	}

	switch (readByte(0x149)) // RAM Size
	{
	case 0x02:
		ramBanks = 1;
		break;
	case 0x03:
		ramBanks = 4;
		break;
	case 0x04:
		ramBanks = 16;
		break;
	case 0x05:
		ramBanks = 8;
		break;
	default:
		ramBanks = 0;
		break;
	}

	this->romBanks = romBanks;
	this->checksum = checksum;
	this->RTC = mapper->getRTC();

	hasRAM = ramBanks != 0;
	ram.resize(RAM_BANK_SIZE * ramBanks);
	ram.shrink_to_fit();

	gb.gameTitle = "";
	is.seekg(0x134, std::ios::beg);
	char titleVal;

	for (int i = 0x134; i <= 0x143; i++)
	{
		is.get(titleVal);
		if (titleVal <= 0) break; // If reached the end, or found illegal character
		gb.gameTitle += titleVal;
	}

	return true;
}