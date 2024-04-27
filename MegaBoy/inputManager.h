#pragma once
#include "CPU.h"
#include <map>

class MMU;

class inputManager
{
public:
	inputManager(MMU& mmu, CPU& cpu) : mmu(mmu), cpu(cpu)
	{}

	void update(int scancode, int action);
	void reset();

	constexpr uint8_t getJoypadReg() { return joypadReg; }

	inline void setJoypadReg(uint8_t val)
	{
		// Allow writing only upper nibble to joypad register.
		joypadReg = (joypadReg & 0xCF) | (val & 0x30);
		modeChanged();
	}

private:
	MMU& mmu;
	CPU& cpu;

	uint8_t joypadReg {0xCF};
	uint8_t dpadState { 0xF };
	uint8_t buttonState { 0xF };

	bool readButtons { false };
	bool readDpad { false };

	void modeChanged();
	void updateKeyGroup(int scancode, int action, uint8_t& keyState, const std::map<int, uint8_t>& keyConfig, bool setReg);
};