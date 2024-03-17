#pragma once
#include <cstdint>
#include "GBCore.h"
#include "registers.h"
#include <iostream>
#include <bitset>

//using namespace GBCore;

class Instructions
{
public:
	static void Test()
	{
		GBCore::_CPU.registers.A = 0b10000000;
		std::cout << "A register before RLCA: " << (int)GBCore::_CPU.registers.A << std::endl;

		RLCA();
		std::cout << "A register after RLCA: " << (int)GBCore::_CPU.registers.A << std::endl;

		//std::bitset<8> val(GBCore::_CPU.registers.A);
		//std::cout << "Value: " << val;
	}


	static void increment(uint8_t& reg)
	{
		bool halfCarry = ((reg & 0x0F) + 1) & 0x10;
		reg++;

		GBCore::_CPU.registers.setFlag(FlagType::Zero, reg == 0);
		GBCore::_CPU.registers.setFlag(FlagType::Subtract, false);
		GBCore::_CPU.registers.setFlag(FlagType::HalfCarry, halfCarry);

		GBCore::_CPU.PC++;
		GBCore::_CPU.cycles = 1;
	}
	static void increment(reg16 reg)
	{
		if (reg.reg1 < UINT8_MAX)
			reg.reg1++;
		else 
		{
			reg.reg2++;
			reg.reg1 = 0;
		}

		GBCore::_CPU.PC++;
		GBCore::_CPU.cycles = 2;
	}

	static void decrement(uint8_t& reg)
	{
		bool halfCarry = (reg & 0x0F) == 0;
		reg--;

		GBCore::_CPU.registers.setFlag(FlagType::Zero, reg == 0);
		GBCore::_CPU.registers.setFlag(FlagType::Subtract, true);
		GBCore::_CPU.registers.setFlag(FlagType::HalfCarry, halfCarry);

		GBCore::_CPU.PC++;
		GBCore::_CPU.cycles = 1;
	}
	static void decrement(reg16 reg)
	{
		if (reg.reg1 > 0)
			reg.reg1--;
		else
		{
			reg.reg2--;
			reg.reg1 = UINT8_MAX;
		}

		GBCore::_CPU.PC++;
		GBCore::_CPU.cycles = 2;
	}

	static void loadToReg(reg16 reg, uint16_t val)
	{
		GBCore::_CPU.registers.set16Bit(reg, val);
		GBCore::_CPU.PC += 3;
		GBCore::_CPU.cycles = 3;
	}
	static void loadToReg(uint8_t& reg, uint8_t val)
	{
		reg = val;
		GBCore::_CPU.PC += 2;
		GBCore::_CPU.cycles = 2;
	}
	static void loadToAddr(uint16_t addr, uint16_t val)
	{
		GBCore::_CPU.write16(addr, val);
		GBCore::_CPU.PC += 3;
		GBCore::_CPU.cycles += 5;
	}
	static void loadAtoAddr(reg16View reg) // loads A to memory value reg points to.
	{
		uint16_t addr = GBCore::_CPU.registers.get16Bit(reg);
		GBCore::_CPU.write8(addr, GBCore::_CPU.registers.A);
		GBCore::_CPU.PC++;
		GBCore::_CPU.cycles = 2;
	}
	static void loadAddrToA(reg16View reg) // loads memory addr to A
	{
		uint16_t addr = GBCore::_CPU.registers.get16Bit(reg);
		GBCore::_CPU.registers.A = GBCore::_CPU.read8(addr);
		GBCore::_CPU.PC++;
		GBCore::_CPU.cycles = 2;
	}

	//TO FIX
	static void RLCA()
	{
		std::cout << "A register in RLCA: " << (int)GBCore::_CPU.registers.A << std::endl;
		int carry = GBCore::_CPU.registers.A;
		std::cout << "Carry after assignment: " << (int)carry << std::endl;



		//GBCore::_CPU.registers.setFlag(FlagType::Carry, carry);
		//GBCore::_CPU.registers.A <<= 1;

		//if (carry) GBCore::_CPU.registers.A |= 1;
		//else GBCore::_CPU.registers.A &= ~(0);

		//GBCore::_CPU.registers.setFlag(FlagType::HalfCarry, false);
		//GBCore::_CPU.registers.setFlag(FlagType::Subtract, false);
		//GBCore::_CPU.registers.setFlag(FlagType::Zero, false);

		//GBCore::_CPU.PC++;
		//GBCore::_CPU.cycles = 1;
	}
	static void RRCA()
	{
		uint8_t carry = GBCore::_CPU.registers.A & 1;
		GBCore::_CPU.registers.setFlag(FlagType::Carry, GBCore::_CPU.registers.A & 1);
		GBCore::_CPU.registers.A >>= 1;

		if (carry) GBCore::_CPU.registers.A |= (1 << 0x80);
		else GBCore::_CPU.registers.A &= ~(1 << 0x80);

		GBCore::_CPU.registers.setFlag(FlagType::HalfCarry, false);
		GBCore::_CPU.registers.setFlag(FlagType::Subtract, false);
		GBCore::_CPU.registers.setFlag(FlagType::Zero, false);

		GBCore::_CPU.PC++;
		GBCore::_CPU.cycles = 1;
	}

	static void addToHL(reg16View reg)
	{

	}
};