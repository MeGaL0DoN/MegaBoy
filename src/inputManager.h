#pragma once

#include <fstream>
#include <map>

#include "CPU/CPU.h"
#include "defines.h"

class MMU;

class inputManager
{
public:
	explicit inputManager(CPU& cpu) : cpu(cpu)
	{}
	           
	void update(int scancode, int action);
	void reset();

	uint8_t readJoypadReg() const;
	void setJoypadReg(uint8_t val);

	inline void saveState(std::ofstream& st) const { ST_WRITE(readButtons), ST_WRITE(readDpad); }
	inline void loadState(std::ifstream& st) { ST_READ(readButtons), ST_READ(readDpad); }
private:
	CPU& cpu;

	bool readButtons { true };
	bool readDpad { true };

	uint8_t dpadState { 0xF };
	uint8_t buttonState { 0xF };

	void updateKeyGroup(int scancode, int action, uint8_t& keyState, const std::map<int, uint8_t>& keyConfig, bool groupEnabled);
};