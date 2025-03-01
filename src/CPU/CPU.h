#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include "registers.h"
#include "../Utils/bitOps.h"

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
	friend CPUInstructions;
	friend class MMU;
	friend class debugUI;

public:
	uint8_t execute();
	void requestInterrupt(Interrupt interrupt);
	void updateTimer();

	std::string disassemble(uint16_t addr, uint8_t(*readFunc)(uint16_t), uint8_t* instrLen);

	explicit CPU(GBCore& gbCore);
	~CPU();

	void reset();

	constexpr uint16_t getPC() const { return s.PC; }
	constexpr void resetPC() { s.PC = 0x00; }

	void setRetOpcodeEvent(void(*event)()) { retEvent = event; }
	void setHaltExitEvent(void(*event)()) { haltExitEvent = event; }

	constexpr uint8_t TcyclesPerM() const { return tCyclesPerM; }
	constexpr bool doubleSpeedMode() const { return s.GBCdoubleSpeed; }

	void saveState(std::ostream& st) const;
	void loadState(std::istream& st);
private:
	GBCore& gb;

	void executeMain();
	void executePrefixed();

	void handleInterrupts();
	bool handleHaltedState();

	inline uint8_t pendingInterrupt()
	{
		return s.IE & s.IF & 0x1F;
	}

	static constexpr uint8_t HL_IND = 6;
	uint8_t& getRegister(uint8_t ind);

	void addCycle();

	void write8(uint16_t addr, uint8_t val);
	uint8_t read8(uint16_t addr);

	inline uint8_t fetch8()
	{
		return read8(s.PC++);
	}
	inline uint16_t fetch16()
	{
		const uint8_t low { read8(s.PC++) };
		return (read8(s.PC++) << 8) | low;
	}

	inline bool getFlag(FlagType flag)
	{
		return getBit(registers.AF.low.val, flag);
	}
	inline void setFlag(FlagType flag, bool value)
	{
		registers.AF.low = setBit(registers.AF.low.val, flag, value);
	}
	inline void resetFlags()
	{
		registers.AF.low.val &= 0x0F;
	}

	inline void exitHalt()
	{
		if (s.halted)
		{
			s.halted = false;
			s.stopState = false;

			if (haltExitEvent != nullptr)
				haltExitEvent();
		}
	}

	struct cpuState
	{
		uint16_t PC{ 0x100 };
		Register16 SP{ 0xFFFE };

		uint8_t DIV_reg{ 0xAC };
		uint8_t TIMA_reg{ 0x00 };
		uint8_t TMA_reg{ 0x00 };
		uint8_t TAC_reg{ 0xF8 };

		uint16_t DIV_COUNTER{ 0 };
		uint16_t TIMA_COUNTER{ 0 };

		uint8_t IE{ 0x00 };
		uint8_t IF{ 0xE1 };

		bool halted{ false };
		bool halt_bug{ false };

		bool stopState{ false };
		uint16_t stopCycleCounter{ 0 };

		bool IME{ false };
		bool shouldSetIME{ false };

		bool GBCdoubleSpeed{ false };
		bool prepareSpeedSwitch{ false };
	};

	cpuState s{};
	registerCollection registers{};

	uint8_t opcode { 0 };
	uint8_t cycles { 0 };
	uint8_t HL_val {};

	uint8_t tCyclesPerM { 0 };

	std::unique_ptr<CPUInstructions> instructions;

	void(*retEvent)();
	void(*haltExitEvent)();
};