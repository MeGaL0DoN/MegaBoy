#pragma once
#include <filesystem>
#include "MMU.h"
#include "CPU.h"
#include "PPU.h"
#include "inputManager.h"
#include "serialPort.h"
#include "Cartridge.h"

class GBCore
{
public:
	static constexpr int CYCLES_PER_FRAME = 17556;
	static constexpr double FRAME_RATE = 1.0 / 59.7;

	GBCore();

	static constexpr int getCycles(double deltaTime) { return static_cast<int>((GBCore::CYCLES_PER_FRAME * (deltaTime / GBCore::FRAME_RATE))); }

	void update(int cyclesToExecute = CYCLES_PER_FRAME);
	void stepComponents();
	void reset();

	bool paused { false };
	bool runBootROM { false };

	MMU mmu { *this };
	CPU cpu { *this };
	PPU ppu{ mmu, cpu };
	inputManager input { mmu, cpu };
	serialPort serial { cpu };
	Cartridge cartridge { *this };
};