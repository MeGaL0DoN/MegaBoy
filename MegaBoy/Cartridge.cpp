#include "Cartridge.h"
#include "GbCore.h"
#include "RomOnlyMBC.h"
#include "EmptyMBC.h"
#include "MBC1.h"
#include <iostream>

Cartridge::Cartridge(GBCore& gbCore) : gbCore(gbCore), mapper(std::make_unique<EmptyMBC>(*this)) { }

void Cartridge::loadROM(std::ifstream& ifs)
{
	ROMLoaded = true;
	gbCore.reset();

	std::ifstream::pos_type pos = ifs.tellg();

	rom.resize(pos);
	rom.shrink_to_fit();

	ifs.seekg(0, std::ios::beg);
	ifs.read(reinterpret_cast<char*>(&rom[0]), pos);

	proccessCartridgeHeader();

	if (gbCore.runBootROM && std::filesystem::exists("data/boot_rom.bin"))
	{
		std::ifstream ifs("data/boot_rom.bin", std::ios::binary | std::ios::ate);
		std::ifstream::pos_type pos = ifs.tellg();
		if (pos != 256) return;

		ifs.seekg(0, std::ios::beg);
		ifs.read(reinterpret_cast<char*>(&gbCore.mmu.bootROM[0]), pos);

		// LCD disabled on boot ROM start
		gbCore.ppu.LCDC = resetBit(gbCore.ppu.LCDC, 7);
		gbCore.cpu.enableBootROM();
	}
}

void Cartridge::proccessCartridgeHeader()
{
	romBanks = 1 << (rom[0x148] + 1);
	ramBanks = 0;

	switch (rom[0x149])
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
	}

	bool hasRam = ramBanks != 0; // some cartridges claim to have ram but have 0 ram banks

	switch (rom[0x147]) // MBC type
	{
	case 0x00:
		mapper = std::make_unique<RomOnlyMBC>(*this);
		break;
	case 0x01:
		mapper = std::make_unique<MBC1>(*this);
		break;
	case 0x02:
		mapper = std::make_unique<MBC1>(*this, hasRam, false);
		break;
	case 0x03:
		mapper = std::make_unique<MBC1>(*this, hasRam, true);
		break;
	default:
		std::cout << "Unknown MBC. \n";
	}
}