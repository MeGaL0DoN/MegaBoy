#include "Cartridge.h"
#include "GbCore.h"
#include "NoMBC.h"
#include <iostream>

Cartridge::Cartridge(GBCore& gbCore) : gbCore(gbCore) {}

void Cartridge::loadROM(std::ifstream& ifs)
{
	ROMLoaded = true;
	gbCore.reset();

	std::ifstream::pos_type pos = ifs.tellg();
	data = std::make_unique<uint8_t[]>(pos);

	ifs.seekg(0, std::ios::beg);
	ifs.read(reinterpret_cast<char*>(&data[0]), pos);

	//switch (data[0x147]) // MBC type
	//{
	//case 0x00:
		mapper = std::make_unique<NoMBC>(data.get());
	//	break;
	//default:
	//	std::cout << "Unknown MBC. \n";
	//}

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