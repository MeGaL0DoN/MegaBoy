#pragma once

#include <iostream>
#include "CPU/CPU.h"
#include "defines.h"

class SerialPort
{
public:
	friend class MMU;

	explicit SerialPort(CPU& cpu) : cpu(cpu) { reset(); }
	void writeSerialControl(uint8_t val);
	void execute();

	constexpr void reset() { s = {}; }

	void saveState(std::ostream& st) const { ST_WRITE(s);}
	void loadState(std::istream& st) { ST_READ(s); }
private:
	CPU& cpu;
	static constexpr uint16_t SERIAL_TRANSFER_CYCLES = 128;

	struct serialState
	{
		uint8_t serial_control{0x7E};
		uint8_t serial_reg{0x0};

		uint16_t serialCycles{0x0};
		uint8_t transferredBits{0x0};
	};

	serialState s{};
};