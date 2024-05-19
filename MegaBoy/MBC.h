#pragma once
#include <cstdint>
#include <vector>
#include <fstream>
#include "MBCBase.h"
#include "Cartridge.h"

struct MBCstate
{
	bool ramEnable{ false };
	uint8_t romBank { 2 };
};

// mbc state class template
template <typename T>
class MBC : public MBCBase
{
public:
	MBC(Cartridge& cartridge) : cartridge(cartridge), rom(cartridge.getRom())
	{
		cartridge.hasRAM = cartridge.ramBanks != 0;

		if (cartridge.hasRAM)
			ram.resize(0x2000 * cartridge.ramBanks);
	}

	void saveState(std::ofstream& st) const
	{
		st.write(reinterpret_cast<const char*>(&s), sizeof(s));

		if (cartridge.hasRAM)
			st.write(reinterpret_cast<const char*>(ram.data()), ram.size());
	}

	void loadState(std::ifstream& st)
	{
		st.read(reinterpret_cast<char*>(&s), sizeof(s));

		if (cartridge.hasRAM)
			st.read(reinterpret_cast<char*>(ram.data()), ram.size());
	}

	void reset(bool resetBattery)
	{
		s = {};

		if (resetBattery) 
			std::fill(ram.begin(), ram.end(), static_cast<uint8_t>(0));
	}

protected:
	const Cartridge& cartridge;

	const std::vector<uint8_t>& rom;
	std::vector<uint8_t> ram;

	T s;
};