#pragma once
#include <cstdint>
#include "CPU.h"

#include <iostream>
#include <bitset>

class InstructionsEngine
{
private:
	CPU* cpu;

	constexpr bool halfCarry8(uint8_t a, uint8_t b)
	{
		return ((a & 0xF) + (b & 0xF)) & 0x10;
	}
	constexpr bool halfCarry16(uint16_t a, uint16_t b)
	{
		return ((a & 0xFFF) + (b & 0xFFF)) & 0x1000;
	}

	constexpr bool halfBorrow8(uint8_t a, uint8_t b)
	{
		return ((a & 0xF) < (b & 0xF));
	}
	constexpr bool halfBorrow16(uint16_t a, uint16_t b)
	{
		return ((a & 0xFFF) < (b & 0xFFF));
	}

	inline void add8_base(uint8_t& reg, uint8_t val, uint8_t cycles, uint8_t pc)
	{
		uint16_t result = reg + val;

		cpu->registers.setFlag(FlagType::Subtract, false);
		cpu->registers.setFlag(FlagType::HalfCarry, halfCarry8(reg, val));
		cpu->registers.setFlag(FlagType::Carry, result > 0xFF);

		reg = result;
		cpu->registers.setFlag(FlagType::Zero, reg == 0);

		cpu->PC += pc;
		cpu->cycles = cycles;
	}
	inline void sub8_base(uint8_t& reg, uint8_t val, uint8_t cycles, uint8_t pc)
	{
		reg = cp_base(reg, val, cycles, pc);
	}
	inline uint8_t cp_base(uint8_t reg, uint8_t val, uint8_t cycles, uint8_t pc)
	{
		cpu->registers.setFlag(FlagType::Subtract, true);
		cpu->registers.setFlag(FlagType::HalfCarry, halfBorrow8(reg, val));
		cpu->registers.setFlag(FlagType::Carry, val > reg);

		reg -= val;
		cpu->registers.setFlag(FlagType::Zero, reg == 0);

		cpu->PC += pc;
		cpu->cycles = cycles;

		return reg;
	}

	inline void and_base(uint8_t& reg, uint8_t val, uint8_t cycles, uint8_t pc)
	{
		reg &= val;

		cpu->registers.resetFlags();
		cpu->registers.setFlag(FlagType::Zero, reg == 0);
		cpu->registers.setFlag(FlagType::HalfCarry, true);

		cpu->PC += pc;
		cpu->cycles = cycles;
	}
	inline void xor_base(uint8_t& reg, uint8_t val, uint8_t cycles, uint8_t pc)
	{
		reg ^= val;
		cpu->registers.resetFlags();
		cpu->registers.setFlag(FlagType::Zero, reg == 0);

		cpu->PC += pc;
		cpu->cycles = cycles;
	}
	inline void or_base(uint8_t& reg, uint8_t val, uint8_t cycles, uint8_t pc)
	{
		reg |= val;
		cpu->registers.resetFlags();
		cpu->registers.setFlag(FlagType::Zero, reg == 0);

		cpu->PC += pc;
		cpu->cycles = cycles;
	}

public:
	InstructionsEngine() = default;
	InstructionsEngine(CPU* cp) : cpu(cp) {};

	void assertEqual(int expected, int actual, const std::string& test_name) {
		if (expected == actual) {
			std::cout << "Test " << test_name << " passed.\n";
		}
		else {
			std::cout << "Test " << test_name << " failed: expected " << expected << ", but got " << actual << ".\n";
		}
	}

	void TEST() {
		// Test INC D opcode (0x14)
		cpu->PC = 0;
		cpu->registers.D = 0x12; // Set D to 0x12
		cpu->loadProgram({ 0x14 }); // INC D opcode
		cpu->execute();
		assertEqual(0x13, cpu->registers.D.val, "INC D D");
		assertEqual(1, cpu->PC, "INC D PC");
		assertEqual(1, cpu->cycles, "INC D cycles");

		// Test DEC D opcode (0x15)
		cpu->PC = 0;
		cpu->registers.D = 0x12; // Set D to 0x12
		cpu->loadProgram({ 0x15 }); // DEC D opcode
		cpu->execute();
		assertEqual(0x11, cpu->registers.D.val, "DEC D D");
		assertEqual(1, cpu->PC, "DEC D PC");
		assertEqual(1, cpu->cycles, "DEC D cycles");

		// Test LD D,d8 opcode (0x16)
		cpu->PC = 0;
		cpu->loadProgram({ 0x16, 0x34 }); // LD D,d8 opcode with immediate value 0x34
		cpu->execute();
		assertEqual(0x34, cpu->registers.D.val, "LD D,d8 D");
		assertEqual(2, cpu->PC, "LD D,d8 PC");
		assertEqual(2, cpu->cycles, "LD D,d8 cycles");

		// Test RLA opcode (0x17)
		cpu->PC = 0;
		cpu->registers.A = 0x85; // Set A to 0x85
		cpu->loadProgram({ 0x17 }); // RLA opcode
		cpu->execute();
		assertEqual(0x0A, cpu->registers.A.val, "RLA A");
		assertEqual(1, cpu->PC, "RLA PC");
		assertEqual(1, cpu->cycles, "RLA cycles");

		// Test JR d8 opcode (0x18)
		cpu->PC = 0;
		cpu->loadProgram({ 0x18, 0x02 }); // JR d8 opcode with immediate value 0x02
		cpu->execute();
		assertEqual(2, cpu->PC, "JR d8 PC");
		assertEqual(3, cpu->cycles, "JR d8 cycles");

		// Test ADD HL,DE opcode (0x19)
		cpu->PC = 0;
		cpu->registers.HL.val = 0x1234; // Set HL to 0x1234
		cpu->registers.DE.val = 0x1111; // Set DE to 0x1111
		cpu->loadProgram({ 0x19 }); // ADD HL,DE opcode
		cpu->execute();
		assertEqual(0x2345, cpu->registers.HL.val, "ADD HL,DE HL");
		assertEqual(1, cpu->PC, "ADD HL,DE PC");
		assertEqual(2, cpu->cycles, "ADD HL,DE cycles");

		// Test LD A,(DE) opcode (0x1A)
		cpu->PC = 0;
		cpu->registers.DE.val = 0x2000; // Set DE to 0x2000
		cpu->MEM[0x2000] = 0x12; // Set memory at address DE to 0x12
		cpu->loadProgram({ 0x1A }); // LD A,(DE) opcode
		cpu->execute();
		assertEqual(0x12, cpu->registers.A.val, "LD A,(DE) A");
		assertEqual(1, cpu->PC, "LD A,(DE) PC");
		assertEqual(2, cpu->cycles, "LD A,(DE) cycles");

		// ... continue for all opcodes
	}



	void INCR(Register16& reg)
	{
		reg.val++;
		cpu->PC++;
		cpu->cycles = 2;
	}
	void INCR(Register8& reg)
	{
		bool halfCarry = halfCarry8(reg.val, 1);
		reg.val++;

		cpu->registers.setFlag(FlagType::Zero, reg.val == 0);
		cpu->registers.setFlag(FlagType::Subtract, false);
		cpu->registers.setFlag(FlagType::HalfCarry, halfCarry);

		cpu->PC++;
		cpu->cycles = 1;
	}

	void ADD(uint8_t regInd)
	{
		uint8_t cycles = 1;
		if (regInd == cpu->HL_IND) cycles++;
		add8_base(cpu->registers.A.val, cpu->getRegister(regInd), regInd == cpu->HL_IND ? 2 : 1, 1);
	}
	void ADD(Register8& reg, uint8_t val)
	{
		add8_base(reg.val, val, 2, 2);
	}
	void ADC(Register8& reg, uint8_t val)
	{
		add8_base(reg.val, val + cpu->registers.getFlag(FlagType::Carry), 2, 2);
	}
	void ADC(uint8_t regInd)
	{
		add8_base(cpu->registers.A.val, cpu->getRegister(regInd) + cpu->registers.getFlag(FlagType::Carry), regInd == cpu->HL_IND ? 2 : 1, 1);
	}

	void addToHL(Register16 reg)
	{
		uint32_t result = cpu->registers.HL.val + reg.val;

		cpu->registers.setFlag(FlagType::Subtract, 0);
		cpu->registers.setFlag(FlagType::HalfCarry, halfCarry16(cpu->registers.HL.val, reg.val));
		cpu->registers.setFlag(FlagType::Carry, result > 0xFFFF);

		cpu->registers.HL = result & 0xFFFF;
		cpu->PC++;
		cpu->cycles = 2;
	}
	void addToSP(int8_t val) // TO CHECK
	{
		uint16_t result = cpu->SP.val + val;

		cpu->registers.resetFlags();
		cpu->registers.setFlag(FlagType::HalfCarry, halfCarry8(cpu->SP.val, val));
		cpu->registers.setFlag(FlagType::Carry, result > 0xFF);

		cpu->SP = result;
		cpu->PC += 2;
		cpu->cycles = 4;
	}

    void DECR(Register8& reg)
	{
		bool halfCarry = (reg.val & 0x0F) == 0;
		reg.val--;

		cpu->registers.setFlag(FlagType::Zero, reg.val == 0);
		cpu->registers.setFlag(FlagType::Subtract, true);
		cpu->registers.setFlag(FlagType::HalfCarry, halfCarry);

		cpu->PC++;
		cpu->cycles = 1;
	}
    void DECR(Register16& reg)
	{
		reg.val--;
		cpu->PC++;
		cpu->cycles = 2;
	}

	void SUB(uint8_t regInd)
	{
		sub8_base(cpu->registers.A.val, cpu->getRegister(regInd), regInd == cpu->HL_IND ? 2 : 1, 1);
	}
	void SUB(Register8& reg, uint8_t val)
	{
		sub8_base(reg.val, val, 2, 2);
	}
	void CP(uint8_t regInd)
	{
		cp_base(cpu->registers.A.val, cpu->getRegister(regInd), regInd == cpu->HL_IND ? 2 : 1, 1);
	}
	void CP(Register8& reg, uint8_t val)
	{
		cp_base(reg.val, val, 2, 2);
	}
	void SBC(Register8& reg, uint8_t val)
	{
		sub8_base(reg.val, val + cpu->registers.getFlag(FlagType::Carry), 2, 2);
	}
	void SBC(uint8_t regInd)
	{
		sub8_base(cpu->registers.A.val, cpu->getRegister(regInd) + cpu->registers.getFlag(FlagType::Carry), regInd == cpu->HL_IND ? 2 : 1, 1);
	}

	void AND(Register8& reg, uint8_t val)
	{
		and_base(reg.val, val, 2, 2);
	}
	void AND(uint8_t regInd)
	{
		and_base(cpu->registers.A.val, cpu->getRegister(regInd), regInd == cpu->HL_IND ? 2 : 1, 1);
	}

	void XOR(Register8& reg, uint8_t val)
	{
		xor_base(reg.val, val, 2, 2);
	}
	void XOR(uint8_t regInd)
	{
		xor_base(cpu->registers.A.val, cpu->getRegister(regInd), regInd == cpu->HL_IND ? 2 : 1, 1);
	}

	void OR(Register8& reg, uint8_t val)
	{
		or_base(reg.val, val, 2, 2);
	}
	void OR(uint8_t regInd)
	{
		or_base(cpu->registers.A.val, cpu->getRegister(regInd), regInd == cpu->HL_IND ? 2 : 1, 1);
	}


	void loadToReg(Register8& reg, uint8_t val)
	{
		reg = val;
		cpu->PC += 2;
		cpu->cycles = 2;
	}
	void loadToReg(Register16& reg, uint16_t val)
	{
		reg = val;
		cpu->PC += 3;
		cpu->cycles = 3;
	}
	void loadToReg(Register8& reg, Register16 addr)
	{
		reg = cpu->read8(addr.val);
		cpu->PC++;
		cpu->cycles = 2;
	}
	//void loadToReg(Register8& reg, uint16_t addr)
	//{
	//	reg = cpu->read8(addr);
	//	cpu->PC += 2;
	//	cpu->cycles = 3;
	//}
	void loadToReg(uint8_t inInd, uint8_t outInd)
	{
		uint8_t& inReg = cpu->getRegister(inInd);
		uint8_t outReg = cpu->getRegister(outInd);
		inReg = outReg;

		cpu->PC++;
		cpu->cycles = (inInd == cpu->HL_IND || outInd == cpu->HL_IND) ? 2 : 1;
	}

	void loadToAddr(Register16 addr, Register8 reg)
	{
		cpu->write8(addr.val, reg.val);
		cpu->PC++;
		cpu->cycles = 2;
	}
	void loadToAddr(uint16_t addr, uint8_t val)
	{
		cpu->write8(addr, val);
		cpu->PC += 2;
		cpu->cycles = 3;
	}
	void loadToAddr(uint16_t addr, Register16 reg)
	{
		cpu->write16(addr, reg.val);
		cpu->PC += 3;
		cpu->cycles = 5;
	}
	void loadToAddr(uint16_t addr, Register8 reg)
	{
		cpu->write8(addr, reg.val);
		cpu->PC += 3;
		cpu->cycles = 4;
	}

	void LD_C_A() // TO CHECK
	{
		cpu->write16(0xFF00 + cpu->registers.getFlag(FlagType::Carry), cpu->registers.A.val);
		cpu->PC++;
		cpu->cycles = 2;
	}
	void LD_A_C() // TO CHECK
	{
		//loadToReg(cpu->registers.A, Register16{ 0xFF00 + cpu->registers.getFlag(FlagType::Carry) });
	}

	void LD_HLI_A()
	{
		loadToAddr(cpu->registers.HL, cpu->registers.A);
		INCR(cpu->registers.HL);
		cpu->PC--;
	}
	void LD_HLD_A()
	{
		loadToAddr(cpu->registers.HL, cpu->registers.A);
		DECR(cpu->registers.HL);
		cpu->PC--;
	}

	void LD_A_HLI()
	{
		loadToReg(cpu->registers.A, cpu->registers.HL);
		INCR(cpu->registers.HL);
		cpu->PC--;
	}
	void LD_A_HLD()
	{
		loadToReg(cpu->registers.A, cpu->registers.HL);
		DECR(cpu->registers.HL);
		cpu->PC--;
	}

	void RLC(Register8& reg, uint8_t len = 2)
	{
		uint8_t carry = (reg.val & 0x80) >> 7;
		cpu->registers.resetFlags();
		cpu->registers.setFlag(FlagType::Carry, carry);
		reg.val <<= 1;

		if (carry) reg.val |= 1;
		else reg.val &= (~1);

		cpu->PC += len;
		cpu->cycles = len;
	}
	void RRC(Register8& reg, uint8_t len = 2)
	{
		uint8_t carry = reg.val & 1;
		cpu->registers.resetFlags();
		cpu->registers.setFlag(FlagType::Carry, carry);
		reg.val >>= 1;

		if (carry) reg.val |= 0x80;
		else reg.val &= (~0x80);

		cpu->PC += len;
		cpu->cycles = len;
	}

    void RLCA() { RLC(cpu->registers.A, 1); }
	void RRCA() { RRC(cpu->registers.A, 1); }

	void RL(Register8& reg, uint8_t len = 2) // TO CHECK
	{
		uint8_t carry = reg.val >> 7;
		reg.val <<= 1;

		if (cpu->registers.getFlag(FlagType::Carry)) reg.val |= 1;
		else reg .val&= (~1);

		cpu->registers.resetFlags();
		cpu->registers.setFlag(FlagType::Carry, carry);

		cpu->PC += len;
		cpu->cycles += len;
	}
	void RR(Register8& reg, uint8_t len = 2) // TO CHECK
	{
		uint8_t carry = reg.val & 0x01;
		reg.val >>= 1;

		if (cpu->registers.getFlag(FlagType::Carry)) reg.val |= 0x80;
		else reg.val &= (~0x80);

		cpu->registers.resetFlags();
		cpu->registers.setFlag(FlagType::Carry, carry);

		cpu->PC += len;
		cpu->cycles = len;
	}

	void RLA() { RL(cpu->registers.A, 1); }
	void RRA() { RR(cpu->registers.A, 1); }

	void CPL()
	{
		cpu->registers.A.val = ~cpu->registers.A.val;
		cpu->registers.setFlag(FlagType::Subtract, true);
		cpu->registers.setFlag(FlagType::HalfCarry, true);

		cpu->PC++;
		cpu->cycles = 1;
	}
	void CCF()
	{
		cpu->registers.setFlag(FlagType::Carry, !cpu->registers.getFlag(FlagType::Carry));
		cpu->registers.setFlag(FlagType::HalfCarry, 0);
		cpu->registers.setFlag(FlagType::Subtract, 0);

		cpu->PC++;
		cpu->cycles = 1;
	}
	void SCF()
	{
		cpu->registers.setFlag(FlagType::Carry, true);
		cpu->registers.setFlag(FlagType::HalfCarry, false);
		cpu->registers.setFlag(FlagType::Subtract, false);

		cpu->PC++;
		cpu->cycles = 1;
	}
	void EI()
	{
		cpu->setIME = true;
		cpu->PC++;
		cpu->cycles = 1;
	}
	void DI()
	{
		cpu->IME = false;
		cpu->PC++;
		cpu->cycles = 1;
	}

	void STOP() // TODO
	{
		cpu->stopped = true;
		cpu->PC++;
		cpu->cycles = 1;
	}
	void HALT() //TODO
	{
		cpu->halted = true;
		cpu->PC++;
		cpu->cycles = 1;
	}

	inline void JR(int8_t val) 
	{
		cpu->PC += val + 2;
		cpu->cycles = 3;
	}
	void JR_CON(bool cond, int8_t val) 
	{
		if (cond)
			JR(val);
		else
		{
			cpu->cycles = 2;
			cpu->PC += 2;
		}
	}

	inline void JP(Register16 addr)
	{
		cpu->PC = addr.val;
		cpu->cycles = 1;
	}
	inline void JP(uint16_t addr) 
	{
		cpu->PC = addr;
		cpu->cycles = 4;
	}
	void JP_CON(bool cond, uint16_t addr) 
	{
		if (cond) 
			JP(addr);
		else
		{
			cpu->cycles = 3;
			cpu->PC += 3;
		}
	}

	void POP(uint16_t& val) 
	{
		val = cpu->read16(cpu->SP.val);
		cpu->SP.val += 2;
		cpu->cycles = 3;
	}
	void PUSH(uint16_t val) 
	{
		cpu->SP.val -= 2;
		cpu->write16(cpu->SP.val, val);
		cpu->cycles = 4;
	}

	void RET()
	{
		POP(cpu->PC);
		cpu->cycles = 4;
	}
	void RET_CON(bool cond) 
	{
		if (cond)
		{
			POP(cpu->PC);
			cpu->cycles = 5;
		}
		else
			cpu->cycles = 2;
	}

	inline void CALL(uint16_t addr)
	{
		PUSH(cpu->PC + 3);
		cpu->PC = addr;
		cpu->cycles = 6;
	}
	void CALL_CON(bool cond, uint16_t addr) 
	{
		if (cond)
			CALL(addr);
		else
		{
			cpu->PC += 3;
			cpu->cycles = 3;
		}
	}
	void RST(uint16_t addr)
	{
		PUSH(cpu->PC + 1);
		cpu->PC = addr;
	}
};