#pragma once
#include "MBC.h"

struct MBC6State
{
	bool ramEnable { false };
	bool flashEnable { false };
	bool flashWriteEnable { false };
	bool flashModeBankA { false };
	bool flashModeBankB { false };
	uint8_t ramBankA { 0 };
	uint8_t ramBankB { 0 };
	uint8_t romBankA { 0 };
	uint8_t romBankB { 0 };
};

class MBC6 : public MBC<MBC6State>
{
public:
	using MBC::MBC;

	uint8_t read(uint16_t addr) const override
	{
		const auto getRomVal = [&](uint8_t bank, uint16_t baseAddr) -> uint8_t
		{
			// 0x2000 (8 KB) sized ram banks instead of the usual 16 KB.
			return rom[(bank & (cartridge.romBanks - 1)) * 0x2000 + (addr - baseAddr)];
		};
		const auto getRamVal = [&](uint8_t bank, uint16_t baseAddr) -> uint8_t
		{
			// 0x1000 (4 KB) sized ram banks instead of the usual 8 KB.
			return s.ramEnable ? ram[(bank & (cartridge.ramBanks - 1)) * 0x1000 + (addr - baseAddr)] : 0xFF;
		};

		if (addr <= 0x3FFF)
		{
			return rom[addr];
		}
		if (addr <= 0x5FFF)
		{
			if (s.flashModeBankA)
				; // TODO, not emulating flash currently...

			return getRomVal(s.romBankA, 0x4000);
		}
		if (addr <= 0x7FFF)
		{
			if (s.flashModeBankB)
				; //

			return getRomVal(s.romBankB, 0x6000);
		}
		if (addr >= 0xA000)
		{
			if (addr <= 0xAFFF)
				return getRamVal(s.ramBankA, 0xA000);
			if (addr <= 0xBFFF)
				return getRamVal(s.ramBankB, 0xB000);
		}

		return 0xFF;
	}

	void write(uint16_t addr, uint8_t val) override
	{
		if (addr <= 0x03FF)
		{
			if (val == 0xA)
				s.ramEnable = true;
			else if (val == 0x0)
				s.ramEnable = false;
		}
		else if (addr <= 0x07FF)
		{
			s.ramBankA = val;
		}
		else if (addr <= 0x0BFF)
		{
			s.ramBankB = val;
		}
		else if (addr <= 0x0FFF)
		{
			s.flashEnable = getBit(val, 0);
		}
		else if (addr == 0x1000)
		{
			s.flashWriteEnable = getBit(val, 0);
		}
		else if (addr >= 0x2000)
		{
			if (addr <= 0x27FF)
			{
				s.romBankA = val;
			}
			else if (addr <= 0x2FFF)
			{
				if (val == 0x8)
					s.flashModeBankA = true;
				else if (val == 0x0)
					s.flashModeBankA = false;
			}
			else if (addr <= 0x37FF)
			{
				s.romBankB = val;
			}
			else if (addr <= 0x3FFF)
			{
				if (val == 0x8)
					s.flashModeBankB = true;
				else if (val == 0x0)
					s.flashModeBankB = false;
			}
			else if (addr >= 0xA000)
			{
				const auto writeRamVal = [&](uint8_t bank, uint16_t baseAddr)
				{
					if (s.ramEnable)
					{
						ram[(bank & (cartridge.ramBanks - 1)) * 0x1000 + (addr - baseAddr)] = val;
						sramDirty = true;
					}
				};

				if (addr <= 0xAFFF)
					writeRamVal(s.ramBankA, 0xA000);
				else if (addr <= 0xBFFF)
					writeRamVal(s.ramBankB, 0xB000);
			}
		}
	}
};