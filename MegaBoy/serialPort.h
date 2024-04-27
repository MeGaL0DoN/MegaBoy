#pragma once
#include "CPU.h"

class serialPort
{
public:
	serialPort(CPU& cpu) : cpu(cpu) { reset(); }
	void writeSerialControl(uint8_t val);
	void execute();

	constexpr void reset() { serial_reg = 0x0; serial_control = 0x7E; serialCycles = 0; transferredBits = 0; }
	uint8_t serial_control;
	uint8_t serial_reg;
private:
	CPU& cpu;
	uint16_t serialCycles;
	uint8_t transferredBits;
	static constexpr uint16_t SERIAL_TRANSFER_CYCLES = 128;
};