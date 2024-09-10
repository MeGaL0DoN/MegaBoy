#pragma once
#include "CPU/CPU.h"
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

	inline uint8_t readJoypadReg()
	{
		if (!readButtons && !readDpad)
			return 0xCF;

		uint8_t keyState{ 0xF0 };

		if (readDpad)
		{
			keyState = resetBit(keyState, 4);
			keyState |= dpadState;
		}
		if (readButtons)
		{
			keyState = resetBit(keyState, 5);
			keyState |= buttonState;
		}

		return keyState;
	}

	inline void setJoypadReg(uint8_t val)
	{
		readButtons = !getBit(val, 5);
		readDpad = !getBit(val, 4);
	}

	inline void saveState(std::ofstream& st) { st.write(reinterpret_cast<char*>(&readButtons), 1); st.write(reinterpret_cast<char*>(&readDpad), 1); }
	inline void loadState(std::ifstream& st) { st.read(reinterpret_cast<char*>(&readButtons), 1); st.read(reinterpret_cast<char*>(&readDpad), 1);}

private:
	CPU& cpu;

	bool readButtons { true };
	bool readDpad { true };

	uint8_t dpadState { 0xF };
	uint8_t buttonState { 0xF };

	void updateKeyGroup(int scancode, int action, uint8_t& keyState, const std::map<int, uint8_t>& keyConfig, bool groupEnabled);
};