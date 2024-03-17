#pragma once
#include <cstdint>
#include <cstring>
#include "registers.h"

class Instructions;

class CPU
{
public:
	uint8_t execute();
	friend class Instructions;

private:
	void executePrefixed();
	void executeUnprefixed();

	uint8_t getRegister(uint8_t ind);
	void setRegister(uint8_t ind, uint8_t val);

	void init()
	{
		registers = {};
		PC = 0;
		SP = 0;
		std::memset(RAM, 0, sizeof(RAM));
	}

	constexpr void write8(uint16_t addr, uint8_t val)
	{
		RAM[addr] = val;
	}
	constexpr uint8_t read8(uint16_t addr)
	{
		return RAM[addr];
	}

	constexpr void write16(uint16_t addr, uint16_t val)
	{
		write8(addr, val & 0xFF);
		write8(addr + 1, val >> 8);
	}
	constexpr uint16_t read16(uint16_t addr) 
	{
		return (read8(addr + 1) << 8) | read8(addr);
	}

	uint8_t RAM[8192];
	registerCollection registers;

	uint8_t opcode;
	uint8_t cycles;
	uint16_t PC;
	uint16_t SP;
};