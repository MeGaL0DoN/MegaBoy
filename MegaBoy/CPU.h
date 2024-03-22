#pragma once
#include <cstdint>
#include <cstring>
#include "registers.h"

class InstructionsEngine;

class CPU
{
public:
	uint8_t MEM[0xFFFF];
	uint8_t execute();
	void loadROM(std::string_view path);

    CPU();
	friend class InstructionsEngine;

private:
	void executePrefixed();
	void executeUnprefixed();

	static constexpr uint8_t HL_IND = 6;
	uint8_t& getRegister(uint8_t ind);
	//void setRegister(uint8_t ind, uint8_t val);

	void init()
	{
		registers.resetRegisters();
		PC = 0;
		SP = 0;
		std::memset(MEM, 0, sizeof(MEM));
	}

	constexpr void write8(uint16_t addr, uint8_t val)
	{
		MEM[addr] = val;
	}
	constexpr uint8_t& read8(uint16_t addr)
	{
		return MEM[addr];
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


	registerCollection registers {};

	uint8_t opcode {};
	uint8_t cycles {};
	uint16_t PC {};
	Register16 SP;

	bool stopped{ false };
	bool halted{ false };

	bool IME { false };
	bool setIME { false };

	void loadProgram(std::initializer_list<uint8_t> values)
	{
		std::memset(MEM, 0, sizeof(MEM));
		int i = 0;

		for (int item : values)
		{
			MEM[i] = item;
			i++;
		}
	}
};