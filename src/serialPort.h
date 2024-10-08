#pragma once
#include "CPU/CPU.h"
#include <fstream>

class serialPort
{
public:
	friend class MMU;

	explicit serialPort(CPU& cpu) : cpu(cpu) { reset(); }
	void writeSerialControl(uint8_t val);
	void execute();

	constexpr void reset() { s = {}; }

	void saveState(std::ofstream& st) const;
	void loadState(std::ifstream& st);

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