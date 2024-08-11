#pragma once
#include "CPU.h"
#include <map>
#include <fstream>

class MMU;

class inputManager
{
public:
	inputManager(CPU& cpu) : cpu(cpu)
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

	inline void saveState(std::ofstream& st) { st.write(reinterpret_cast<char*>(&joypadReg), sizeof(joypadReg)); }
	inline void loadState(std::ifstream& st) { st.read(reinterpret_cast<char*>(&joypadReg), sizeof(joypadReg)); }

private:
	CPU& cpu;

	uint8_t joypadReg {0xCF};
	uint8_t dpadState { 0xF };
	uint8_t buttonState { 0xF };

	void modeChanged();
	void updateKeyGroup(int scancode, int action, uint8_t& keyState, const std::map<int, uint8_t>& keyConfig, bool setReg);
};