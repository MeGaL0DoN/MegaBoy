#pragma once
#include <cstdint>
#include "CPU.h"
#include "GBCore.h"

class InstructionsEngine
{
private:
	CPU* cpu;

	constexpr bool halfCarry8(uint8_t a, uint8_t b, uint8_t c)
	{
		return ((a & 0xF) + (b & 0xF) + c) & 0x10;
	}
	constexpr bool halfCarry16(uint16_t a, uint16_t b)
	{
		return ((a & 0xFFF) + (b & 0xFFF)) & 0x1000;
	}
	
	constexpr bool halfBorrow8(uint8_t a, uint8_t b, uint8_t c)
	{
		return ((a & 0xF) < (b & 0xF) + c);
	}
	constexpr bool halfBorrow16(uint16_t a, uint16_t b)
	{
		return ((a & 0xFFF) < (b & 0xFFF));
	}

	inline void add8_base(Register8& reg, uint8_t add1, uint8_t add2)
	{
		uint16_t result = reg.val + add1 + add2;

		cpu->s.registers.setFlag(Subtract, false);
		cpu->s.registers.setFlag(HalfCarry, halfCarry8(reg.val, add1, add2));
		cpu->s.registers.setFlag(Carry, result > 0xFF);

		reg = static_cast<uint8_t>(result);
		cpu->s.registers.setFlag(Zero, reg.val == 0);
	}
	inline void add16_signed(Register16& reg, int8_t val)
	{
		uint16_t result = reg.val + val;

		cpu->s.registers.resetFlags();
		cpu->s.registers.setFlag(HalfCarry, (reg.val ^ val ^ (result & 0xFFFF)) & 0x10);
		cpu->s.registers.setFlag(Carry, (reg.val ^ val ^ (result & 0xFFFF)) & 0x100);

		reg = result;
	}

	inline uint8_t cp_base(uint8_t reg, uint8_t sub1, uint8_t sub2)
	{
		int16_t result = reg - sub1 - sub2;

		cpu->s.registers.setFlag(Subtract, true);
		cpu->s.registers.setFlag(HalfCarry, halfBorrow8(reg, sub1, sub2));
		cpu->s.registers.setFlag(Carry, result < 0);
		cpu->s.registers.setFlag(Zero, (result & 0xFF) == 0);

		return static_cast<uint8_t>(result);
	}

	inline void and_base(uint8_t& reg, uint8_t val)
	{
		reg &= val;

		cpu->s.registers.resetFlags();
		cpu->s.registers.setFlag(Zero, reg == 0);
		cpu->s.registers.setFlag(HalfCarry, true);
	}
	inline void xor_base(uint8_t& reg, uint8_t val)
	{
		reg ^= val;
		cpu->s.registers.resetFlags();
		cpu->s.registers.setFlag(Zero, reg == 0);
	}
	inline void or_base(uint8_t& reg, uint8_t val)
	{
		reg |= val;
		cpu->s.registers.resetFlags();
		cpu->s.registers.setFlag(Zero, reg == 0);
	}

	inline void rlc_base(uint8_t& reg)
	{
		uint8_t carry = (reg & 0x80) >> 7;
		reg <<= 1;

		if (carry) reg |= 1;
		else reg &= (~1);

		cpu->s.registers.resetFlags();
		cpu->s.registers.setFlag(Carry, carry);
		cpu->s.registers.setFlag(Zero, reg == 0);
	}
	inline void rrc_base(uint8_t& reg)
	{
		uint8_t carry = reg & 1;
		reg >>= 1;

		if (carry) reg |= 0x80;
		else reg &= (~0x80);

		cpu->s.registers.resetFlags();
		cpu->s.registers.setFlag(Carry, carry);
		cpu->s.registers.setFlag(Zero, reg == 0);
	}

	inline void rl_base(uint8_t& reg) 
	{
		uint8_t carry = reg >> 7;
		reg <<= 1;

		if (cpu->s.registers.getFlag(Carry)) reg |= 1;
		else reg &= (~1);

		cpu->s.registers.resetFlags();
		cpu->s.registers.setFlag(Carry, carry);
		cpu->s.registers.setFlag(Zero, reg == 0);
	}
	inline void rr_base(uint8_t& reg) 
	{
		uint8_t carry = reg & 1;
		reg >>= 1;

		if (cpu->s.registers.getFlag(Carry)) reg |= 0x80;
		else reg &= (~0x80);

		cpu->s.registers.resetFlags();
		cpu->s.registers.setFlag(Carry, carry);
		cpu->s.registers.setFlag(Zero, reg == 0);
	}

public:
	InstructionsEngine() = default;
	InstructionsEngine(CPU* cp) : cpu(cp) {};

	template <bool step = true>
	void INCR(Register16& reg)
	{
		reg.val++;
		if constexpr (step) cpu->addCycle();
		cpu->s.PC++;
	}
	void INCR(uint8_t& reg)
	{
		bool halfCarry = halfCarry8(reg, 1, 0);
		reg++;

		cpu->s.registers.setFlag(Zero, reg == 0);
		cpu->s.registers.setFlag(Subtract, false);
		cpu->s.registers.setFlag(HalfCarry, halfCarry);

		cpu->s.PC++;
	}
	void INCR_HL()
	{
		uint8_t val = cpu->read8(cpu->s.registers.HL.val);
		INCR(val);
		cpu->write8(cpu->s.registers.HL.val, val);
	}

	void ADD(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		add8_base(cpu->s.registers.A, reg, 0);

		if (regInd == cpu->HL_IND) cpu->addCycle();
		cpu->s.PC++;
	}
	void ADD(Register8& reg, uint8_t val)
	{
		add8_base(reg, val, 0);
		cpu->s.PC += 2;
	}
	void ADC(Register8& reg, uint8_t val)
	{
		add8_base(reg, val, cpu->s.registers.getFlag(Carry));
		cpu->s.PC += 2;
	}
	void ADC(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		add8_base(cpu->s.registers.A, reg, cpu->s.registers.getFlag(Carry));

		if (regInd == cpu->HL_IND) cpu->addCycle();
		cpu->s.PC++;
	}

	void addToHL(Register16 reg)
	{
		uint32_t result = cpu->s.registers.HL.val + reg.val;

		cpu->s.registers.setFlag(Subtract, 0);
		cpu->s.registers.setFlag(HalfCarry, halfCarry16(cpu->s.registers.HL.val, reg.val));
		cpu->s.registers.setFlag(Carry, result > 0xFFFF);

		cpu->s.registers.HL = result & 0xFFFF;
		cpu->addCycle();

		cpu->s.PC++;
	}
	void addToSP(int8_t val) 
	{
		add16_signed(cpu->s.SP, val);
		cpu->addCycle();
		cpu->addCycle();
		cpu->s.PC += 2;
	}

    void DECR(uint8_t& reg)
	{
		bool halfCarry = (reg & 0x0F) == 0;
		reg--;

		cpu->s.registers.setFlag(Zero, reg == 0);
		cpu->s.registers.setFlag(Subtract, true);
		cpu->s.registers.setFlag(HalfCarry, halfCarry);

		cpu->s.PC++;
	}

	template<bool step = true>
    void DECR(Register16& reg)
	{
		reg.val--;
		if constexpr (step) cpu->addCycle();
		cpu->s.PC++;
	}
	void DECR_HL()
	{
		uint8_t val = cpu->read8(cpu->s.registers.HL.val);
		DECR(val);
		cpu->write8(cpu->s.registers.HL.val, val);
	}

	void SUB(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		cpu->s.registers.A = cp_base(cpu->s.registers.A.val, reg, 0);

		if (regInd == cpu->HL_IND) cpu->addCycle();
		cpu->s.PC++;
	}
	void SUB(Register8& reg, uint8_t val)
	{
		reg = cp_base(reg.val, val, 0);
		cpu->s.PC += 2;
	}
	void CP(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		cp_base(cpu->s.registers.A.val, reg, 0);

		if (regInd == cpu->HL_IND) cpu->addCycle();
		cpu->s.PC++;
	}
	void CP(Register8& reg, uint8_t val)
	{
		cp_base(reg.val, val, 0);
		cpu->s.PC += 2;
	}
	void SBC(Register8& reg, uint8_t val)
	{
		reg = cp_base(reg.val, val, cpu->s.registers.getFlag(Carry));
		cpu->s.PC += 2;
	}
	void SBC(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		cpu->s.registers.A = cp_base(cpu->s.registers.A.val, reg, cpu->s.registers.getFlag(Carry));

		if (regInd == cpu->HL_IND) cpu->addCycle();
		cpu->s.PC += 1;
	}

	void AND(Register8& reg, uint8_t val)
	{
		and_base(reg.val, val);
		cpu->s.PC += 2;
	}
	void AND(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		and_base(cpu->s.registers.A.val, reg);

		if (regInd == cpu->HL_IND) cpu->addCycle();
		cpu->s.PC++;
	}

	void XOR(Register8& reg, uint8_t val)
	{
		xor_base(reg.val, val);
		cpu->s.PC += 2;
	}
	void XOR(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		xor_base(cpu->s.registers.A.val, reg);

		if (regInd == cpu->HL_IND) cpu->addCycle();
		cpu->s.PC++;
	}

	void OR(Register8& reg, uint8_t val)
	{
		or_base(reg.val, val);
		cpu->s.PC += 2;
	}
	void OR(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		or_base(cpu->s.registers.A.val, reg);

		if (regInd == cpu->HL_IND) cpu->addCycle();
		cpu->s.PC++;
	}

	void loadToReg(Register8& reg, uint8_t val)
	{
		reg = val;
		cpu->s.PC += 2;
	}
	void loadToReg(Register16& reg, uint16_t val)
	{
		reg = val;
		cpu->s.PC += 3;
	}
	void loadToReg(Register8& reg, Register16 addr)
	{
		reg = cpu->read8(addr.val);
		cpu->s.PC++;
	}
	void loadToReg(Register8& reg, uint16_t addr)
	{
		reg = cpu->read8(addr);
		cpu->s.PC += 3;
	}
	void loadToReg(uint8_t inInd, uint8_t outInd)
	{
		uint8_t& inReg = cpu->getRegister(inInd);
		uint8_t outReg = cpu->getRegister(outInd);

		if (inInd == cpu->HL_IND)
			cpu->write8(cpu->s.registers.HL.val, outReg);
		else
		{
			if (outInd == cpu->HL_IND) cpu->addCycle();
			inReg = outReg;
		}

		cpu->s.PC++;
	}

	void loadToAddr(Register16 addr, Register8 reg)
	{
		cpu->write8(addr.val, reg.val);
		cpu->s.PC++;
	}
	void loadToAddr(uint16_t addr, uint8_t val)
	{
		cpu->write8(addr, val);
		cpu->s.PC += 2;
	}
	void loadToAddr(uint16_t addr, Register16 reg)
	{
		cpu->write16(addr, reg.val);
		cpu->s.PC += 3;
	}
	void loadToAddr(uint16_t addr, Register8 reg)
	{
		cpu->write8(addr, reg.val);
		cpu->s.PC += 3;
	}

	void LD_C_A() 
	{
		cpu->write8(0xFF00 + cpu->s.registers.C.val, cpu->s.registers.A.val);
		cpu->s.PC++;
	}
	void LD_A_C() 
	{
		loadToReg(cpu->s.registers.A, Register16 { static_cast<uint16_t>(0xFF00 + cpu->s.registers.C.val) });
	}

	void LD_SP_HL()
	{
		cpu->s.SP = cpu->s.registers.HL;
		cpu->addCycle();
		cpu->s.PC++;
	}
	void LD_HL_SP(int8_t val) 
	{
		Register16 result { cpu->s.SP };
		add16_signed(result, val);
		cpu->s.registers.HL = result;
		cpu->addCycle();
		cpu->s.PC += 2;
	}

	void LD_OFFSET_A(uint8_t addr)
	{
		cpu->write8(0xFF00 + addr, cpu->s.registers.A.val);
		cpu->s.PC += 2;
	}
	void LD_A_OFFSET(uint8_t addr)
	{
		cpu->s.registers.A = cpu->read8(0xFF00 + addr);
		cpu->s.PC += 2;
	}

	void LD_HLI_A()
	{
		loadToAddr(cpu->s.registers.HL, cpu->s.registers.A);
		INCR<false>(cpu->s.registers.HL);
		cpu->s.PC--;
	}
	void LD_HLD_A()
	{
		loadToAddr(cpu->s.registers.HL, cpu->s.registers.A);
		DECR<false>(cpu->s.registers.HL);
		cpu->s.PC--;
	}

	void LD_A_HLI()
	{
		loadToReg(cpu->s.registers.A, cpu->s.registers.HL);
		INCR<false>(cpu->s.registers.HL);
		cpu->s.PC--;
	}
	void LD_A_HLD()
	{
		loadToReg(cpu->s.registers.A, cpu->s.registers.HL);
		DECR<false>(cpu->s.registers.HL);
		cpu->s.PC--;
	}

	void RLCA()
	{
		rlc_base(cpu->s.registers.A.val);
		cpu->s.registers.setFlag(Zero, false);
		cpu->s.PC++;
	}
	void RLC(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		rlc_base(reg);

		if (regInd == cpu->HL_IND)
		{
			cpu->addCycle();
			cpu->write8(cpu->s.registers.HL.val, reg);
		}
		cpu->s.PC++;
	}

	void RRCA() 
	{ 
		rrc_base(cpu->s.registers.A.val); 
		cpu->s.registers.setFlag(Zero, false);
		cpu->s.PC++;
	}
	void RRC(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		rrc_base(reg);

		if (regInd == cpu->HL_IND)
		{
			cpu->addCycle();
			cpu->write8(cpu->s.registers.HL.val, reg);
		}
		cpu->s.PC++;
	}

	void RLA() 
	{ 
		rl_base(cpu->s.registers.A.val); 
		cpu->s.registers.setFlag(Zero, false);
		cpu->s.PC++;
	}
	void RL(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		rl_base(reg);

		if (regInd == cpu->HL_IND)
		{
			cpu->addCycle();
			cpu->write8(cpu->s.registers.HL.val, reg);
		}
		cpu->s.PC++;
	}

	void RRA() 
	{
		rr_base(cpu->s.registers.A.val);
		cpu->s.registers.setFlag(Zero, false);
		cpu->s.PC++;
	}
	void RR(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		rr_base(reg);

		if (regInd == cpu->HL_IND)
		{
			cpu->addCycle();
			cpu->write8(cpu->s.registers.HL.val, reg);
		}
		cpu->s.PC++;
	}

	void SLA(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		uint8_t carry = (reg & 0x80) >> 7;
		reg <<= 1;

		cpu->s.registers.resetFlags();
		cpu->s.registers.setFlag(Carry, carry);
		cpu->s.registers.setFlag(Zero, reg == 0);

		if (regInd == cpu->HL_IND)
		{
			cpu->addCycle();
			cpu->write8(cpu->s.registers.HL.val, reg);
		}
		cpu->s.PC++;
	}
	void SRA(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		uint8_t carry = reg & 1;
		reg >>= 1;

		if (reg & 0x40) 
			reg |= 0x80;

		cpu->s.registers.resetFlags();
		cpu->s.registers.setFlag(Carry, carry);
		cpu->s.registers.setFlag(Zero, reg == 0);

		if (regInd == cpu->HL_IND)
		{
			cpu->addCycle();
			cpu->write8(cpu->s.registers.HL.val, reg);
		}
		cpu->s.PC++;
	}
	void SRL(uint8_t regInd)
	{
		SRA(regInd);
		uint8_t reg = (cpu->getRegister(regInd) &= 0x7F);

		if (regInd == cpu->HL_IND)
			cpu->gbCore.mmu.write8(cpu->s.registers.HL.val, reg); // Already 4 cycles
	}

	void SWAP(uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		uint8_t upperNibble = reg & 0xF0;
		uint8_t lowerNibble = reg & 0x0F;

		reg = (lowerNibble << 4) | (upperNibble >> 4);
		cpu->s.registers.resetFlags();
		cpu->s.registers.setFlag(Zero, reg == 0);

		if (regInd == cpu->HL_IND)
		{
			cpu->addCycle();
			cpu->write8(cpu->s.registers.HL.val, reg);
		}
		cpu->s.PC++;
	}

	void BIT(uint8_t bit, uint8_t regInd)
	{
		uint8_t reg = cpu->getRegister(regInd);
		bool bitSet = reg & (1 << bit);

		cpu->s.registers.setFlag(Zero, !bitSet);
		cpu->s.registers.setFlag(Subtract, false);
		cpu->s.registers.setFlag(HalfCarry, true);

		if (regInd == cpu->HL_IND)
			cpu->addCycle();

		cpu->s.PC++;
	}
	void RES(uint8_t bit, uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		reg &= ~(1 << bit);
		if (regInd == cpu->HL_IND)
		{
			cpu->addCycle();
			cpu->write8(cpu->s.registers.HL.val, reg);
		}

		cpu->s.PC++;
	}
	void SET(uint8_t bit, uint8_t regInd)
	{
		uint8_t& reg = cpu->getRegister(regInd);
		reg |= (1 << bit);
		if (regInd == cpu->HL_IND)
		{
			cpu->addCycle();
			cpu->write8(cpu->s.registers.HL.val, reg);
		}

		cpu->s.PC++;
	}

	void CPL()
	{
		cpu->s.registers.A.val = ~cpu->s.registers.A.val;
		cpu->s.registers.setFlag(Subtract, true);
		cpu->s.registers.setFlag(HalfCarry, true);

		cpu->s.PC++;
	}
	void CCF()
	{
		cpu->s.registers.setFlag(Carry, !cpu->s.registers.getFlag(Carry));
		cpu->s.registers.setFlag(HalfCarry, 0);
		cpu->s.registers.setFlag(Subtract, 0);

		cpu->s.PC++;
	}
	void SCF()
	{
		cpu->s.registers.setFlag(Carry, true);
		cpu->s.registers.setFlag(HalfCarry, false);
		cpu->s.registers.setFlag(Subtract, false);

		cpu->s.PC++;
	}
	void EI()
	{
		cpu->s.shouldSetIME = true;
		cpu->s.PC++;
	}
	void DI()
	{
		cpu->s.IME = false;
		cpu->s.PC++;
	}
	void DAA() // TO CHECK
	{
		if (cpu->s.registers.getFlag(Subtract)) 
		{
			if (cpu->s.registers.getFlag(Carry))
				cpu->s.registers.A.val -= 0x60;

			if (cpu->s.registers.getFlag(HalfCarry))
				cpu->s.registers.A.val -= 0x06;
		}
		else 
		{
			if (cpu->s.registers.getFlag(Carry) || cpu->s.registers.A.val > 0x99)
			{
				cpu->s.registers.A.val += 0x60;
				cpu->s.registers.setFlag(Carry, true);
			}
			if (cpu->s.registers.getFlag(HalfCarry) || (cpu->s.registers.A.val & 0x0F) > 0x09)
				cpu->s.registers.A.val += 0x06;
		}

		cpu->s.registers.setFlag(Zero, cpu->s.registers.A.val == 0);
		cpu->s.registers.setFlag(HalfCarry, false);

		cpu->s.PC++;
	}

	void STOP() 
	{
		cpu->s.stopped = true;
		cpu->s.PC++;
	}
	void HALT() 
	{
		cpu->s.halted = true;
		cpu->s.PC++;

		if (!cpu->s.IME && cpu->interruptsPending())
			cpu->s.halt_bug = true;
	}

	inline void JR(int8_t val) 
	{
		cpu->s.PC += val + 2;
		cpu->addCycle();
	}
	void JR_CON(bool cond, int8_t val) 
	{
		if (cond)
			JR(val);
		else
			cpu->s.PC += 2;
	}

	inline void JP(Register16 addr)
	{
		cpu->s.PC = addr.val;
	}
	inline void JP(uint16_t addr) 
	{
		cpu->s.PC = addr;
		cpu->addCycle();
	}
	void JP_CON(bool cond, uint16_t addr) 
	{
		if (cond) 
			JP(addr);
		else
			cpu->s.PC += 3;
	}

	void POP(uint16_t& val) 
	{
		cpu->s.PC++;
		val = cpu->read16(cpu->s.SP.val);
		cpu->s.SP.val += 2;
	}
	void POP_AF()
	{
		uint16_t val;
		POP(val);
		cpu->s.registers.AF = val & 0xFFF0;
	}
	void PUSH(uint16_t val) 
	{
		cpu->s.SP.val -= 2;
		cpu->write16(cpu->s.SP.val, val);
		cpu->s.PC++;
		cpu->addCycle();
	}

	void RET()
	{
		POP(cpu->s.PC);
		cpu->addCycle();
	}
	void RET_CON(bool cond) 
	{
		cpu->addCycle();

		if (cond)
		{
			POP(cpu->s.PC);
			cpu->addCycle();
		}
		else
			cpu->s.PC++;
	}
	void RET1()
	{
		RET();
		cpu->s.IME = true;
	}

	inline void CALL(uint16_t addr)
	{
		PUSH(cpu->s.PC + 3);
		cpu->s.PC = addr;
	}
	void CALL_CON(bool cond, uint16_t addr) 
	{
		if (cond)
			CALL(addr);
		else
			cpu->s.PC += 3;
	}
	void RST(uint16_t addr)
	{
		PUSH(cpu->s.PC + 1);
		cpu->s.PC = addr;
	}
};