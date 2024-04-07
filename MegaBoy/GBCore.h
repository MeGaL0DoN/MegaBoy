#pragma once
#include "MMU.h"
#include "CPU.h"
#include "PPU.h"
#include "inputManager.h"

class GBCore
{
public:
	static constexpr int CYCLES_PER_FRAME = 17556;
	static constexpr double FRAME_RATE = 1.0 / 59.7;

	GBCore()
	{
		if (std::filesystem::exists("data/boot_rom.bin"))
			runBootROM = true;
	}

	void update();
	bool paused { false };
	bool runBootROM { false };

	MMU mmu {*this};
	CPU cpu { mmu };
	PPU ppu { mmu, cpu };
	inputManager input { mmu, cpu };

private:
};