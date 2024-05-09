#include "MBC1.h"
#include "Cartridge.h"

MBC::MBC(Cartridge& cartridge) : cartridge(cartridge), rom(cartridge.getRom())
{ 
	cartridge.hasRAM = cartridge.ramBanks != 0;

	if (cartridge.hasRAM)
		ram.resize(0x2000 * cartridge.ramBanks); 
}

void MBC::saveBattery(std::ofstream& st) const
{
	if (cartridge.hasRAM)
		st.write(reinterpret_cast<const char*>(ram.data()), ram.size());
}

void MBC::loadBattery(std::ifstream& st) 
{
	if (cartridge.hasRAM)
		st.read(reinterpret_cast<char*>(ram.data()), ram.size());
}