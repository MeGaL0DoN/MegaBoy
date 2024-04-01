#pragma once
#include "MMU.h"

class PPU
{
public:
	PPU(MMU& mmu) : mmu(mmu)
	{}

	void execute(uint8_t cycles);
private:
	MMU& mmu;
};