#include "MBC1.h"
#include "Cartridge.h"

MBC::MBC(const Cartridge& cartridge, bool hasRAM, bool hasBattery) : rom(cartridge.getRom()), hasBattery(hasBattery), hasRAM(hasRAM), cartridge(cartridge)
{ 
	if (hasRAM)
		ram.resize(0x2000 * cartridge.ramBanks); 
}