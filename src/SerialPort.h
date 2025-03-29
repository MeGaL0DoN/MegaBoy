#pragma once

#include <iostream>
#include "CPU/CPU.h"
#include "defines.h"

class SerialPort
{
public:
	friend class MMU;

	explicit SerialPort(CPU& cpu) : cpu(cpu) { }

	void writeSerialControl(uint8_t val);
	uint8_t readSerialControl();

	void execute();

	inline void reset() { s = {}; }

	void saveState(std::ostream& st) const { ST_WRITE(s);}
	void loadState(std::istream& st) { ST_READ(s); }
private:
	CPU& cpu;

	struct serialState
	{
		uint8_t serialControl { System::Current() == GBSystem::CGB ? static_cast<uint8_t>(0x7F) : static_cast<uint8_t>(0x7E) };
		uint8_t serialReg { 0x0 };

		uint16_t serialCycles { 0x0 };
		uint8_t transferredBits { 0x0 };
	};

	serialState s{};
};