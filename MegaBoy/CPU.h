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
namespace fs = std::filesystem;

class InstructionsEngine;

class CPU
{
public:
	uint8_t execute();
	void handleInterrupts();

	CPU(MMU& mmu);
	friend class InstructionsEngine;

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

	uint16_t fromHex(const json& js)
	{
		return std::stoul(std::string(js), 0, 16);
	}

	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	void runTest( std::string_view jsonPath, int amount = 50)
	{
		std::ifstream s(jsonPath.data());
		json data = json::parse(s);

		uint8_t opcode = std::stoul(std::string(data[0]["name"]).substr(0, 2), 0, 16);
		SetConsoleTextAttribute(hConsole, 14);
		std::cout << "Testing: 0x" << std::uppercase << std::hex << +opcode << "\n\n";
		SetConsoleTextAttribute(hConsole, 15);

		for (int i = 0; i < amount; i++)
		{
			json& test = data[i];
			json& initial = test["initial"];
			json& result = test["final"];

			//auto a = std::string(data[0]["name"]).substr(3, 2);
			//uint8_t opcode = std::stoul(std::string(data[0]["name"]).substr(3, 4), 0, 16);
			//SetConsoleTextAttribute(hConsole, 14);
			//std::cout << "Testing: 0x" << std::uppercase << std::hex << +opcode << "\n\n";
			//SetConsoleTextAttribute(hConsole, 15);

			registers.A = fromHex(initial["cpu"]["a"]);
			registers.B = fromHex(initial["cpu"]["b"]);
			registers.C = fromHex(initial["cpu"]["c"]);
			registers.D = fromHex(initial["cpu"]["d"]);
			registers.E = fromHex(initial["cpu"]["e"]);
			registers.F = fromHex(initial["cpu"]["f"]);
			registers.H = fromHex(initial["cpu"]["h"]);
			registers.L = fromHex(initial["cpu"]["l"]);
			PC = fromHex(initial["cpu"]["pc"]);
			SP = fromHex(initial["cpu"]["sp"]);

			std::memset(mmu.MEM, 0, sizeof(mmu.MEM));

			for (json& ram : initial["ram"])
			{
				write8(fromHex(ram[0]), fromHex(ram[1]));
			}

			int cycles = execute();
			printState();

			bool passed{true};

			if (registers.A.val != fromHex(result["cpu"]["a"]) || registers.B.val != fromHex(result["cpu"]["b"]) ||
				registers.C.val != fromHex(result["cpu"]["c"]) || registers.D.val != fromHex(result["cpu"]["d"]) ||
				registers.E.val != fromHex(result["cpu"]["e"]) || registers.F.val != fromHex(result["cpu"]["f"]) ||
				registers.H.val != fromHex(result["cpu"]["h"]) || registers.L.val != fromHex(result["cpu"]["l"]) ||
				PC != fromHex(result["cpu"]["pc"]) || SP.val != fromHex(result["cpu"]["sp"]))
			{
				passed = false;
			}

			for (json& ram : result["ram"])
			{
				if (read8(fromHex(ram[0])) != fromHex(ram[1]))
				{
					passed = false;
					break;
				}
			}

			//int finalCycles = 0;
			//for (json& cycl : test["cycles"])
			//{
			//	finalCycles++;
			//}

			//if (cycles != finalCycles) passed = false;

			SetConsoleTextAttribute(hConsole, passed ? 10 : 4);
			std::cout << test["name"] << " " << (passed ? "PASSED!" : "NOT PASSED") << "\n";
			SetConsoleTextAttribute(hConsole, 15);
		}
	}

	void runTests()
	{
		for (const auto& entry : fs::directory_iterator("tests"))
		{
			runTest(entry.path().string(), 20);
			std::cout << "\n<----------------------------------------->\n\n";
		}
	}

private:
	void executePrefixed();
	void executeUnprefixed();

	static constexpr uint8_t HL_IND = 6;
	uint8_t& getRegister(uint8_t ind);

	void reset()
	{
		registers.resetRegisters();
		PC = 0x1000;
		SP = 0xFFFE;
	}

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

	bool stopped { false };
	bool halted { false };

	bool IME { false };
	bool toSetIME { false };
};