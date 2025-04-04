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
	void executeTimer();

	std::string disassemble(uint16_t addr, uint8_t(*readFunc)(uint16_t), uint8_t* instrLen);

	explicit CPU(GBCore& gbCore);
	~CPU();

	void reset();

	constexpr uint16_t getPC() const { return s.PC; }
	constexpr void resetPC() { s.PC = 0x00; }

	void setRetOpcodeEvent(void(*event)()) { retEvent = event; }
	void setHaltExitEvent(void(*event)()) { haltExitEvent = event; }

	constexpr uint8_t TcyclesPerM() const { return tCyclesPerM; }
	constexpr bool doubleSpeedMode() const { return s.cgbDoubleSpeed; }

	constexpr uint64_t haltCycleCount() const { return haltCycleCounter; }
	constexpr void resetHaltCycleCount() { haltCycleCounter = 0; }

	void saveState(std::ostream& st) const;
	void loadState(std::istream& st);
private:
	GBCore& gb;

	void executeMain();
	void executePrefixed();

	bool detectTimaOverflow();
	void writeTacReg(uint8_t val);

	void handleInterrupts();
	bool handleHaltedState();
	void exitHalt();

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

	struct cpuState
	{
		uint16_t PC { 0x100 };
		Register16 SP { 0xFFFE };

		uint16_t divCounter { 0x00 };
		uint8_t timaReg { 0x00 };
		uint8_t tmaReg { 0x00 };
		uint8_t tacReg { 0xF8 };

		bool oldDivBit { false };
		bool timaOverflowDelay { false };
		bool timaOverflowed { false };

		uint8_t IE { 0x00 };
		uint8_t IF { 0xE1 };

		bool halted { false };
		bool haltBug { false };

		bool stopState { false };
		uint16_t stopCycleCounter { 0 };

		bool IME { false };
		bool shouldSetIME { false };

		bool cgbDoubleSpeed { false };
		bool prepareSpeedSwitch { false };
	};

	cpuState s{};
	registerCollection registers{};

	uint8_t opcode { 0 };
	uint8_t cycles { 0 };
	uint8_t HLval{};

	uint8_t tCyclesPerM { 0 };

	std::unique_ptr<CPUInstructions> instructions;

	uint64_t haltStartCycles{};
	uint64_t haltCycleCounter{};

	void(*retEvent)();
	void(*haltExitEvent)();
};