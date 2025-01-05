#include <algorithm>

#include "GBCore.h"
#include "gbSystem.h"
#include "appConfig.h"

#include "Cartridge.h"
#include "Mappers/RomOnlyMBC.h"
#include "Mappers/MBC1.h"
#include "Mappers/MBC2.h"
#include "Mappers/MBC3.h"
#include "Mappers/MBC5.h"
#include "Mappers/HuC1.h"

Cartridge::Cartridge(GBCore& gbCore) : gb(gbCore) { }

uint64_t Cartridge::getGBTotalCycles() const { return gb.totalCycles(); }

bool Cartridge::loadROM(std::istream& is)
{
	is.seekg(0, std::ios::end);
	const uint32_t size = is.tellg();
	
	if (!romSizeValid(size))
		return false;

	if (!proccessCartridgeHeader(is, size))
		return false;

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

	for (size_t i = 0x134; i <= 0x14C; i++)
	{
		uint8_t byte;
		is.read(reinterpret_cast<char*>(&byte), 1);
		checksum = checksum - byte - 1;
	}

	return checksum;
}

void Cartridge::updateSystem(uint8_t cgbFlag)
{
	GBSystem gbSystem{ GBSystem::DMG };

	if (appConfig::systemPreference != GBSystemPreference::ForceDMG)
	{
		if (cgbFlag == 0xC0)
			gbSystem = GBSystem::GBC;
		else if (cgbFlag == 0x80)
		{
			if (appConfig::systemPreference == GBSystemPreference::PreferGBC)
				gbSystem = GBSystem::GBC;
		}
	}

	System::Set(gbSystem);
}

bool Cartridge::proccessCartridgeHeader(std::istream& is, uint32_t fileSize)
{
	auto readByte = [&is](uint16_t ind) -> uint8_t
	{
		uint8_t byte;
		is.seekg(ind, std::ios::beg);
		is.read(reinterpret_cast<char*>(&byte), 1);
		return byte;
	};

	const uint8_t checksum = calculateHeaderChecksum(is);
	const uint8_t storedChecksum = readByte(0x14D);

	if (checksum != storedChecksum) 
		return false;

	const uint16_t romBanks = 1 << (readByte(0x148) + 1);

	if (romBanks == 0 || romBanks > fileSize / ROM_BANK_SIZE) 
		return false;

	const bool initialHasBattery = hasBattery;
	const bool initialHasTimer = hasTimer;

	hasBattery = false;
	hasTimer = false;

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
		hasBattery = true; hasTimer = true;
		mapper = std::make_unique<MBC3>(*this);
		break;
	case 0x11:
	case 0x12:
		mapper = std::make_unique<MBC3>(*this);
		break;
	case 0x13:
		hasBattery = true;
		mapper = std::make_unique<MBC3>(*this);
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
	case 0xFF:
		hasBattery = true;
		mapper = std::make_unique<HuC1>(*this);
		break;

	default:
		std::cout << "Unknown MBC! \n";
		hasBattery = initialHasBattery;  
		hasTimer = initialHasTimer;
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

	hasRAM = ramBanks != 0;
	if (hasRAM) ram.resize(RAM_BANK_SIZE * ramBanks);

	gb.gameTitle = "";
	is.seekg(0x134, std::ios::beg);
	char titleVal;

	for (int i = 0x134; i <= 0x143; i++)
	{
		is.get(titleVal);
		if (titleVal <= 0) break; // If reached the end, or found illegal character
		gb.gameTitle += titleVal;
	}

	updateSystem(readByte(0x143));
	return true;
}