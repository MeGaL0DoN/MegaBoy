#pragma once
#include "MMU.h"
#include "CPU.h"
#include "PPU.h"
#include "inputManager.h"

class GBCore
{
public:
	void update();

	MMU mmu {*this};
	CPU cpu { mmu };
	PPU ppu { mmu };
	inputManager input { mmu };

private:
};