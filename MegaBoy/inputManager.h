#pragma once
#include "MMU.h"
#include "CPU.h"
#include <map>

class inputManager
{
public:
	inputManager(MMU& mmu, CPU& cpu) : mmu(mmu), cpu(cpu)
	{}

	void update(int scancode, int action);
	void modeChanged(uint8_t joypadReg);
	void reset();

private:
	MMU& mmu;
	CPU& cpu;

	uint8_t dpadState { 0xF };
	uint8_t buttonState { 0xF };

	bool readButtons { false };
	bool readDpad { false };

	void updateKeyGroup(int scancode, int action, uint8_t& keyState, const std::map<int, uint8_t>& keyConfig, bool setReg, uint8_t& joypadReg);
};