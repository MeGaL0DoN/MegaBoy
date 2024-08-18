#pragma once
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
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
	void requestInterrupt(Interrupt interrupt);
	void updateTimer();

	CPU(GBCore& gbCore);
	~CPU();

	friend InstructionsEngine;
	friend class MMU;
	friend class debugUI;

	void reset();

	constexpr bool isExecutingBootROM() { return executingBootROM; }

	constexpr void enableBootROM()
	{
		s.PC = 0x0;
		executingBootROM = true;
	}
	constexpr void disableBootROM()
	{
		executingBootROM = false;
	}

	void saveState(std::ofstream& st);
	void loadState(std::ifstream& st);

	constexpr bool doubleSpeed() { return s.GBCdoubleSpeed; }
private:
	GBCore& gbCore;

	uint8_t handleInterrupts();
	uint8_t handleHaltedState();
	bool handleGHDMA();

	void executeMain();
	void executePrefixed();
	bool interruptsPending();

	static constexpr uint8_t HL_IND = 6;
	uint8_t& getRegister(uint8_t ind);

	void addCycle();
	inline void addCycles(uint8_t _cycles)
	{
		for (int i = 0; i < _cycles; i++)
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

	struct cpuState
	{
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
		
		bool halted;
		bool halt_bug;

		bool stopState;
		uint16_t stopCycleCounter;

		bool IME;
		bool shouldSetIME;
		bool interruptLatch;

		bool GBCdoubleSpeed;
		bool prepareSpeedSwitch;

		cpuState()
		{
			reset();
		}

		inline void reset()
		{
			PC = 0x100;
			SP = 0xFFFE;
			DIV_reg = 0xAB;
			TIMA_reg = 0x00;
			TMA_reg = 0x00;
			TAC_reg = 0xF8;
			DIV_COUNTER = 0;
			TIMA_COUNTER = 0;
			IE = 0x00;
			IF = 0xE1;
			
			stopState = false;
			stopCycleCounter = 0;
			halted = false;
			halt_bug = false;
			IME = false;
			shouldSetIME = false;

			GBCdoubleSpeed = false;
			prepareSpeedSwitch = false;
		}
	};

	cpuState s{};
	registerCollection registers{};

	uint8_t opcode;
	uint8_t cycles;
	bool executingBootROM;

	std::unique_ptr<InstructionsEngine> instructions;
};