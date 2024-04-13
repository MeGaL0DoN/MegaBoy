#pragma once
#include <cstdint>
#include <cstring>
#include "registers.h"

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
	uint8_t execute();
	uint8_t handleInterrupts();
	void requestInterrupt(Interrupt interrupt);
	void updateTimer(uint8_t cycles);

	CPU(GBCore& gbCore);
	friend class InstructionsEngine;
	friend class MMU;

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

	void reset();

	void addCycle(uint8_t cycles = 1);
	void write8(uint16_t addr, uint8_t val);
	uint8_t read8(uint16_t addr);

	inline void write16(uint16_t addr, uint16_t val)
	{
		write8(addr, val & 0xFF);
		write8(addr + 1, val >> 8);
	}
	inline uint16_t read16(uint16_t addr)
	{
		return static_cast<uint16_t>(read8(addr + 1) << 8) | read8(addr);
	}

	registerCollection registers {};
	GBCore& gbCore;

	uint8_t opcode {};
	uint8_t cycles {};

	uint16_t PC { 0x0101 };
	Register16 SP { 0xFFFE };

	uint16_t DIV{};
	uint16_t TIMA{};

	bool stopped { false };
	bool halted { false };
	bool halt_bug{ false };

	bool IME { false };
	bool shouldSetIME { false };

	bool executingBootROM { false };
};