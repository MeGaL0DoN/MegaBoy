#pragma once
#include "MMU.h"

class inputManager
{
public:
	inputManager(MMU& mmu) : mmu(mmu)
	{}

	bool update(int scancode, int action);

private:
	MMU& mmu;
};