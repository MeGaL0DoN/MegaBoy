#pragma once

#include <fstream>
#include <map>
#include "defines.h"

class CPU;

class gbInputManager
{
public:
	explicit gbInputManager(CPU& cpu) : cpu(cpu)
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
};