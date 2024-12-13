#pragma once

#include <cstdint>
#include <iostream>
#include "defines.h"

class CPU;

class Joypad
{
public:
	explicit Joypad(CPU& cpu) : cpu(cpu)
	{}
	           
	void update(int scancode, int action);
	void reset();

	uint8_t readInputReg() const;
	void writeInputReg(uint8_t val);

	inline void saveState(std::ostream& st) const { ST_WRITE(readButtons), ST_WRITE(readDpad); }
	inline void loadState(std::istream& st) { ST_READ(readButtons), ST_READ(readDpad); }
private:
	CPU& cpu;

	bool readButtons { true };
	bool readDpad { true };

	uint8_t dpadState { 0xF };
	uint8_t buttonState { 0xF };
};