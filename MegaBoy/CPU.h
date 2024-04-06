#pragma once
#include <cstdint>
#include <cstring>

#include "registers.h"
#include "MMU.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include "Windows.h"
using json = nlohmann::json;
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;

enum class Interrupt : uint8_t
{
	VBlank = 0,
	LCD = 1,
	Timer = 2,
	Serial = 3,
	Joypad = 4
};

class InstructionsEngine;
class MMU;

class CPU
{
public:
	uint8_t execute();
	uint8_t handleInterrupts();
	void requestInterrupt(Interrupt interrupt);
	void updateTimer(uint8_t cycles);

	CPU(MMU& mmu);
	friend class InstructionsEngine;
	friend class MMU;

	void printState()
	{
		std::cout << "A: " << std::hex << +registers.A.val << " "
			<< "F: " << std::hex << +registers.F.val << " "
			<< "B: " << std::hex << +registers.B.val << " "
			<< "C: " << std::hex << +registers.C.val << " "
			<< "D: " << std::hex << +registers.D.val << " "
			<< "E: " << std::hex << +registers.E.val << " "
			<< "H: " << std::hex << +registers.H.val << " "
			<< "L: " << std::hex << +registers.L.val << " "
			<< "SP: " << std::hex << SP.val << " "
			<< "PC: " << std::hex << PC << " "
			<< "(" << std::hex << +read8(PC) << " "
			<< std::hex << +read8(PC + 1) << " "
			<< std::hex << +read8(PC + 2) << " "
			<< std::hex << +read8(PC + 3) << ")"
			<< std::endl;
	}

private:
	void executePrefixed();
	void executeUnprefixed();
	bool interruptsPending();

	static constexpr uint8_t HL_IND = 6;
	uint8_t& getRegister(uint8_t ind);

	void reset();

	inline void write8(uint16_t addr, uint8_t val)
	{
		mmu.write8(addr, val);
	}
	inline uint8_t read8(uint16_t addr)
	{
		return mmu.read8(addr);
	}
	inline void write16(uint16_t addr, uint16_t val)
	{
		mmu.write16(addr, val);
	}
	inline uint16_t read16(uint16_t addr) 
	{
		return mmu.read16(addr);
	}

	registerCollection registers {};
	MMU& mmu;

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
};