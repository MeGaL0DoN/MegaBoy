#pragma once
#include <cstdint>
#include <vector>
#include "MBCBase.h"
#include "../defines.h"
#include "../Cartridge.h"

struct MBCstate
{
	bool ramEnable{ false };
	uint8_t romBank { 1 };
};

// mbc state class template
template <typename T>
class MBC : public MBCBase
{
public:
	MBC(Cartridge& cartridge) : cartridge(cartridge), rom(cartridge.rom), ram(cartridge.ram)
	{ }

	virtual void saveBattery(std::ostream& st) const override
	{
		if (cartridge.hasRAM)
			st.write(reinterpret_cast<const char*>(ram.data()), ram.size());
	}
	virtual void loadBattery(std::istream& st) override
	{
		if (cartridge.hasRAM)
		{
			st.read(reinterpret_cast<char*>(ram.data()), ram.size());
			sramDirty = true;
		}
	}

	void saveState(std::ostream& st) const override
	{
		ST_WRITE(s);
		saveBattery(st);
	}

	void loadState(std::istream& st) override
	{
		ST_READ(s);
		loadBattery(st);
	}

	void reset(bool resetBattery) override
	{
		s = {};
		if (resetBattery) resetBatteryState();
	}
protected:
	Cartridge& cartridge;

	const std::vector<uint8_t>& rom;
	std::vector<uint8_t>& ram;
	T s;

	virtual void resetBatteryState()
	{
		std::fill(ram.begin(), ram.end(), static_cast<uint8_t>(0));
	}
};