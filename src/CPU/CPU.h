#pragma once
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <array>
#include "registers.h"

enum class Interrupt : uint8_t
{
	VBlank = 0,
	STAT = 1,
	Timer = 2,
	Serial = 3,
	Joypad = 4
};

class CPUInstructions;
class GBCore;

class CPU
{
public:
	uint8_t execute();
	void requestInterrupt(Interrupt interrupt);
	void updateTimer();

	std::string disassemble(uint16_t addr, uint8_t(*readFunc)(uint16_t), uint8_t* instrLen);

	explicit CPU(GBCore& gbCore);
	~CPU();

	friend CPUInstructions;
	friend class MMU;
	friend class debugUI;

	void reset();

	constexpr uint16_t getPC() const { return s.PC; }
	constexpr bool isExecutingBootROM() const { return executingBootROM; }
	constexpr uint8_t TcyclesPerM() const { return tCyclesPerM; }

	constexpr void enableBootROM()
	{
		s.PC = 0x0;
		executingBootROM = true;
	}
	constexpr void disableBootROM()
	{
		executingBootROM = false;
	}

	void saveState(std::ofstream& st) const;
	void loadState(std::ifstream& st);
private:
	GBCore& gbCore;

	uint8_t handleInterrupts();
	bool handleHaltedState();

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

	inline uint8_t fetch8()
	{
		return read8(s.PC++);
	}
	inline uint16_t fetch16()
	{
		uint8_t low = read8(s.PC++);
		return (read8(s.PC++) << 8) | low;
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

	uint8_t opcode { 0 };
	uint8_t cycles { 0 };

	uint8_t tCyclesPerM { 0 };
	bool executingBootROM { false };

	std::unique_ptr<CPUInstructions> instructions;
};