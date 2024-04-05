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
	void update();

	MMU mmu {*this};
	CPU cpu { mmu };
	PPU ppu { mmu, cpu };
	inputManager input { mmu, cpu };

private:
};