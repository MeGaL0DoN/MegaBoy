#pragma once
#include <filesystem>
#include "MMU.h"
#include "CPU.h"
#include "PPU.h"
#include "APU.h"
#include "inputManager.h"
#include "serialPort.h"
#include "Cartridge.h"

class GBCore
{
public:
	static constexpr int CYCLES_PER_FRAME = 17556;
	static constexpr double FRAME_RATE = 1.0 / 59.7;

	GBCore();

	static constexpr int getCycles(double deltaTime) { return static_cast<int>((CYCLES_PER_FRAME * (deltaTime / FRAME_RATE))); }

	void update(int cyclesToExecute = CYCLES_PER_FRAME);
	void stepComponents();

	void saveState();
	void loadState();

	void loadBootROM();

	void reset();
	void restartROM();

	bool paused { false };
	bool runBootROM { false };
	std::string gameTitle { };

	MMU mmu { *this };
	CPU cpu { *this };
	PPU ppu{ mmu, cpu };
	APU apu{};
	inputManager input { mmu, cpu };
	serialPort serial { cpu };
	Cartridge cartridge { *this };
};