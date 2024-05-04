#pragma once
#include <cstdint>
#include <cstring>
#include "registers.h"

enum class StatIRQ
{
	Mode0,
	Mode1,
	Mode2,
	LYC
};

enum class Interrupt : uint8_t
{
	VBlank = 0,
	STAT = 1,
	Timer = 2,
	Serial = 3,
	Joypad = 4
};

class InstructionsEngine;
class GBCore;

class CPU
{
public:
	int opcodeNum{ 0 };

	uint8_t execute();
	uint8_t handleInterrupts();
	void requestInterrupt(Interrupt interrupt);
	void updateTimer();

	CPU(GBCore& gbCore);
	friend class InstructionsEngine;
	friend class MMU;

	void reset();

	inline void enableBootROM()
	{
		PC = 0x0;
		executingBootROM = true;
	}

private:
	void executeMain();
	void executePrefixed();
	bool interruptsPending();

	static constexpr uint8_t HL_IND = 6;
	uint8_t& getRegister(uint8_t ind);

	void addCycle();
	inline void addCycles(uint8_t cycles)
	{
		for (int i = 0; i < cycles; i++)
			addCycle();
	}

	void write8(uint16_t addr, uint8_t val);
	uint8_t read8(uint16_t addr);

	inline void write16(uint16_t addr, uint16_t val)
	{
		write8(addr, val & 0xFF);
		write8(addr + 1, val >> 8);
	}
	inline uint16_t read16(uint16_t addr)
	{
		return read8(addr + 1) << 8 | read8(addr);
	}

	registerCollection registers {};
	GBCore& gbCore;

	uint8_t opcode;
	uint8_t cycles;

	uint16_t PC;
	Register16 SP;

	uint8_t DIV_reg;
	uint8_t TIMA_reg;
	uint8_t TMA_reg;
	uint8_t TAC_reg;

	uint16_t DIV_COUNTER;
	uint16_t TIMA_COUNTER;

	uint8_t IE;
	uint8_t IF;

	bool stopped;
	bool halted;
	bool halt_bug;

	bool IME;
	bool shouldSetIME;

	bool executingBootROM;
};