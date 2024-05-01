#include "MBC1.h"
#include "Cartridge.h"

MBC::MBC(Cartridge& cartridge) : cartridge(cartridge), rom(cartridge.getRom())
{ 
	cartridge.hasRAM = cartridge.ramBanks != 0;

	if (cartridge.hasRAM)
		ram.resize(0x2000 * cartridge.ramBanks); 
}