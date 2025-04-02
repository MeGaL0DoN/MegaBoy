#pragma once
#include "../GBCore.h"

#define A AF.high
#define F AF.low
#define B BC.high
#define C BC.low
#define D DE.high
#define E DE.low
#define H HL.high
#define L HL.low

class CPUInstructions
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
		const uint16_t result = reg.val + add1 + add2;

		cpu->setFlag(Subtract, false);
		cpu->setFlag(HalfCarry, halfCarry8(reg.val, add1, add2));
		cpu->setFlag(Carry, result > 0xFF);

		reg = static_cast<uint8_t>(result);
		cpu->setFlag(Zero, reg.val == 0);
	}
	inline void add16_signed(Register16& reg, int8_t val)
	{
		const uint16_t result = reg.val + val;

		cpu->resetFlags();
		cpu->setFlag(HalfCarry, (reg.val ^ val ^ (result & 0xFFFF)) & 0x10);
		cpu->setFlag(Carry, (reg.val ^ val ^ (result & 0xFFFF)) & 0x100);

		reg = result;
	}

	inline uint8_t cp_base(uint8_t reg, uint8_t sub1, uint8_t sub2)
	{
		const int16_t result = reg - sub1 - sub2;

		cpu->setFlag(Subtract, true);
		cpu->setFlag(HalfCarry, halfBorrow8(reg, sub1, sub2));
		cpu->setFlag(Carry, result < 0);
		cpu->setFlag(Zero, (result & 0xFF) == 0);

		return static_cast<uint8_t>(result);
	}

	inline void and_base(uint8_t& reg, uint8_t val)
	{
		reg &= val;

		cpu->resetFlags();
		cpu->setFlag(Zero, reg == 0);
		cpu->setFlag(HalfCarry, true);
	}
	inline void xor_base(uint8_t& reg, uint8_t val)
	{
		reg ^= val;
		cpu->resetFlags();
		cpu->setFlag(Zero, reg == 0);
	}
	inline void or_base(uint8_t& reg, uint8_t val)
	{
		reg |= val;
		cpu->resetFlags();
		cpu->setFlag(Zero, reg == 0);
	}

	inline void rlc_base(uint8_t& reg)
	{
		const uint8_t carry = reg >> 7;
		reg <<= 1;

		if (carry) reg |= 1;
		else reg &= (~1);

		cpu->resetFlags();
		cpu->setFlag(Carry, carry);
		cpu->setFlag(Zero, reg == 0);
	}
	inline void rrc_base(uint8_t& reg)
	{
		const uint8_t carry = reg & 1;
		reg >>= 1;

		if (carry) reg |= 0x80;
		else reg &= (~0x80);

		cpu->resetFlags();
		cpu->setFlag(Carry, carry);
		cpu->setFlag(Zero, reg == 0);
	}

	inline void rl_base(uint8_t& reg) 
	{
		const uint8_t carry = reg >> 7;
		reg <<= 1;

		if (cpu->getFlag(Carry)) reg |= 1;
		else reg &= (~1);

		cpu->resetFlags();
		cpu->setFlag(Carry, carry);
		cpu->setFlag(Zero, reg == 0);
	}
	inline void rr_base(uint8_t& reg) 
	{
		const uint8_t carry = reg & 1;
		reg >>= 1;

		if (cpu->getFlag(Carry)) reg |= 0x80;
		else reg &= (~0x80);

		cpu->resetFlags();
		cpu->setFlag(Carry, carry);
		cpu->setFlag(Zero, reg == 0);
	}

public:
	CPUInstructions(CPU* cp) : cpu(cp) {};

	inline void INCR(Register16& reg)
	{
		reg.val++;
		cpu->addCycle();
	}
	inline void INCR(uint8_t& reg)
	{
		const bool halfCarry { halfCarry8(reg, 1, 0) };
		reg++;

		cpu->setFlag(Zero, reg == 0);
		cpu->setFlag(Subtract, false);
		cpu->setFlag(HalfCarry, halfCarry);
	}
	inline void INCR_HL()
	{
		uint8_t val { cpu->read8(cpu->registers.HL.val) };
		INCR(val);
		cpu->write8(cpu->registers.HL.val, val);
	}

	inline void ADD(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND) cpu->addCycle();
		const uint8_t& reg { cpu->getRegister(regInd) };
		add8_base(cpu->registers.A, reg, 0);
	}
	inline void ADD(Register8& reg, uint8_t val)
	{
		add8_base(reg, val, 0);
	}
	inline void ADC(Register8& reg, uint8_t val)
	{
		add8_base(reg, val, cpu->getFlag(Carry));
	}
	inline void ADC(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND) cpu->addCycle();
		const uint8_t& reg { cpu->getRegister(regInd) };
		add8_base(cpu->registers.A, reg, cpu->getFlag(Carry));
	}

	inline void ADD_HL(Register16 reg)
	{
		const uint32_t result = cpu->registers.HL.val + reg.val;

		cpu->setFlag(Subtract, 0);
		cpu->setFlag(HalfCarry, halfCarry16(cpu->registers.HL.val, reg.val));
		cpu->setFlag(Carry, result > 0xFFFF);

		cpu->registers.HL = result & 0xFFFF;
		cpu->addCycle();
	}
	inline void ADD_SP(int8_t val) 
	{
		add16_signed(cpu->s.SP, val);
		cpu->addCycle();
		cpu->addCycle();
	}

	inline void DECR(Register16& reg)
	{
		reg.val--;
		cpu->addCycle();
	}
    inline void DECR(uint8_t& reg)
	{
		const bool halfCarry { (reg & 0x0F) == 0 };
		reg--;

		cpu->setFlag(Zero, reg == 0);
		cpu->setFlag(Subtract, true);
		cpu->setFlag(HalfCarry, halfCarry);
	}
	inline void DECR_HL()
	{
		uint8_t val { cpu->read8(cpu->registers.HL.val) };
		DECR(val);
		cpu->write8(cpu->registers.HL.val, val);
	}

	inline void SUB(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND) cpu->addCycle();
		cpu->registers.A = cp_base(cpu->registers.A.val, cpu->getRegister(regInd), 0);
	}
	inline void SUB(Register8& reg, uint8_t val)
	{
		reg = cp_base(reg.val, val, 0);
	}
	inline void CP(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND) cpu->addCycle();
		cp_base(cpu->registers.A.val, cpu->getRegister(regInd), 0);
	}
	inline void CP(Register8& reg, uint8_t val)
	{
		cp_base(reg.val, val, 0);
	}
	inline void SBC(Register8& reg, uint8_t val)
	{
		reg = cp_base(reg.val, val, cpu->getFlag(Carry));
	}
	inline void SBC(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND) cpu->addCycle();
		cpu->registers.A = cp_base(cpu->registers.A.val, cpu->getRegister(regInd), cpu->getFlag(Carry));
	}

	inline void AND(Register8& reg, uint8_t val)
	{
		and_base(reg.val, val);
	}
	inline void AND(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND) cpu->addCycle();
		and_base(cpu->registers.A.val, cpu->getRegister(regInd));
	}

	inline void XOR(Register8& reg, uint8_t val)
	{
		xor_base(reg.val, val);
	}
	inline void XOR(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND) cpu->addCycle();
		xor_base(cpu->registers.A.val, cpu->getRegister(regInd));
	}

	inline void OR(Register8& reg, uint8_t val)
	{
		or_base(reg.val, val);
	}
	inline void OR(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND) cpu->addCycle();
		or_base(cpu->registers.A.val, cpu->getRegister(regInd));
	}

	inline void LD(Register8& reg, uint8_t val)
	{
		reg = val;
	}
	inline void LD(Register16& reg, uint16_t val)
	{
		reg = val;
	}
	inline void LD(Register8& reg, Register16 addr)
	{
		reg = cpu->read8(addr.val);
	}
	inline void LD(Register8& reg, uint16_t addr)
	{
		reg = cpu->read8(addr);
	}
	inline void LD(uint8_t inInd, uint8_t outInd)
	{
		if (outInd == CPU::HL_IND) cpu->addCycle();

		uint8_t& inReg { cpu->getRegister(inInd) };
		const uint8_t outReg { cpu->getRegister(outInd) };

		if (inInd == CPU::HL_IND)
			cpu->write8(cpu->registers.HL.val, outReg);
		else
			inReg = outReg;
	}

	inline void LD_MEM(Register16 addr, Register8 reg)
	{
		cpu->write8(addr.val, reg.val);
	}
	inline void LD_MEM(uint16_t addr, Register8 reg)
	{
		cpu->write8(addr, reg.val);
	}
	inline void LD_MEM(uint16_t addr, uint8_t val)
	{
		cpu->write8(addr, val);
	}
	inline void LD_MEM(uint16_t addr, Register16 reg)
	{
		cpu->write8(addr, reg.val & 0xFF);
		cpu->write8(addr + 1, reg.val >> 8);
	}

	inline void LD_C_A() 
	{
		cpu->write8(0xFF00 + cpu->registers.C.val, cpu->registers.A.val);
	}
	inline void LD_A_C() 
	{
		LD(cpu->registers.A, Register16 { static_cast<uint16_t>(0xFF00 + cpu->registers.C.val) });
	}

	inline void LD_SP_HL()
	{
		cpu->s.SP = cpu->registers.HL;
		cpu->addCycle();
	}
	inline void LD_HL_SP(int8_t val) 
	{
		Register16 result { cpu->s.SP };
		add16_signed(result, val);
		cpu->registers.HL = result;
		cpu->addCycle();
	}

	inline void LD_OFFSET_A(uint8_t addr)
	{
		cpu->write8(0xFF00 + addr, cpu->registers.A.val);
	}
	inline void LD_A_OFFSET(uint8_t addr)
	{
		cpu->registers.A = cpu->read8(0xFF00 + addr);
	}

	inline void LD_HLI_A()
	{
		LD_MEM(cpu->registers.HL, cpu->registers.A);
		cpu->registers.HL.val++;
	}
	inline void LD_HLD_A()
	{
		LD_MEM(cpu->registers.HL, cpu->registers.A);
		cpu->registers.HL.val--;
	}

	inline void LD_A_HLI()
	{
		LD(cpu->registers.A, cpu->registers.HL);
		cpu->registers.HL.val++;
	}
	inline void LD_A_HLD()
	{
		LD(cpu->registers.A, cpu->registers.HL);
		cpu->registers.HL.val--;
	}

	inline void RLCA()
	{
		rlc_base(cpu->registers.A.val);
		cpu->setFlag(Zero, false);
	}
	inline void RLC(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND)
			cpu->addCycle();

		uint8_t& reg { cpu->getRegister(regInd) };
		rlc_base(reg);

		if (regInd == CPU::HL_IND)
			cpu->write8(cpu->registers.HL.val, reg);
	}

	inline void RRCA() 
	{ 
		rrc_base(cpu->registers.A.val); 
		cpu->setFlag(Zero, false);
	}
	inline void RRC(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND)
			cpu->addCycle();

		uint8_t& reg { cpu->getRegister(regInd) };
		rrc_base(reg);

		if (regInd == CPU::HL_IND)
			cpu->write8(cpu->registers.HL.val, reg);
	}

	inline void RLA() 
	{ 
		rl_base(cpu->registers.A.val); 
		cpu->setFlag(Zero, false);
	}
	inline void RL(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND)
			cpu->addCycle();

		uint8_t& reg { cpu->getRegister(regInd) };
		rl_base(reg);

		if (regInd == CPU::HL_IND)
			cpu->write8(cpu->registers.HL.val, reg);
	}

	inline void RRA() 
	{
		rr_base(cpu->registers.A.val);
		cpu->setFlag(Zero, false);
	}
	inline void RR(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND)
			cpu->addCycle();

		uint8_t& reg { cpu->getRegister(regInd) };
		rr_base(reg);

		if (regInd == CPU::HL_IND)
			cpu->write8(cpu->registers.HL.val, reg);
	}

	inline void SLA(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND)
			cpu->addCycle();

		uint8_t& reg { cpu->getRegister(regInd) };
		const uint8_t carry = (reg & 0x80) >> 7;
		reg <<= 1;

		cpu->resetFlags();
		cpu->setFlag(Carry, carry);
		cpu->setFlag(Zero, reg == 0);

		if (regInd == CPU::HL_IND)
			cpu->write8(cpu->registers.HL.val, reg);
	}

	template <bool arithmetic>
	inline void SR(uint8_t regInd) 
	{
		if (regInd == CPU::HL_IND)
			cpu->addCycle();

		uint8_t& reg { cpu->getRegister(regInd) };
		const uint8_t carry = reg & 1;
		reg >>= 1;

		if constexpr (arithmetic) 
		{
			if (reg & 0x40)
				reg |= 0x80;
		}

		cpu->resetFlags();
		cpu->setFlag(Carry, carry);
		cpu->setFlag(Zero, reg == 0);

		if (regInd == CPU::HL_IND)
			cpu->write8(cpu->registers.HL.val, reg);
	}

	inline void SRA(uint8_t regInd) { SR<true>(regInd); }
	inline void SRL(uint8_t regInd) { SR<false>(regInd); }

	inline void SWAP(uint8_t regInd)
	{
		if (regInd == CPU::HL_IND)
			cpu->addCycle();

		uint8_t& reg { cpu->getRegister(regInd) };
		const uint8_t upperNibble = reg & 0xF0;
		const uint8_t lowerNibble = reg & 0x0F;

		reg = (lowerNibble << 4) | (upperNibble >> 4);
		cpu->resetFlags();
		cpu->setFlag(Zero, reg == 0);

		if (regInd == CPU::HL_IND)
			cpu->write8(cpu->registers.HL.val, reg);
	}

	inline void BIT(uint8_t bit, uint8_t regInd)
	{
		if (regInd == CPU::HL_IND)
			cpu->addCycle();

		const uint8_t reg { cpu->getRegister(regInd) };
		const bool set = reg & (1 << bit);

		cpu->setFlag(Zero, !set);
		cpu->setFlag(Subtract, false);
		cpu->setFlag(HalfCarry, true);
	}
	inline void RES(uint8_t bit, uint8_t regInd)
	{
		if (regInd == CPU::HL_IND)
			cpu->addCycle();

		uint8_t& reg { cpu->getRegister(regInd) };
		reg &= ~(1 << bit);

		if (regInd == CPU::HL_IND)
			cpu->write8(cpu->registers.HL.val, reg);
	}
	inline void SET(uint8_t bit, uint8_t regInd)
	{
		if (regInd == CPU::HL_IND)
			cpu->addCycle();

		uint8_t& reg { cpu->getRegister(regInd) };
		reg |= (1 << bit);

		if (regInd == CPU::HL_IND)
			cpu->write8(cpu->registers.HL.val, reg);
	}

	inline void CPL()
	{
		cpu->registers.A.val = ~cpu->registers.A.val;
		cpu->setFlag(Subtract, true);
		cpu->setFlag(HalfCarry, true);
	}
	inline void CCF()
	{
		cpu->setFlag(Carry, !cpu->getFlag(Carry));
		cpu->setFlag(HalfCarry, false);
		cpu->setFlag(Subtract, false);
	}
	inline void SCF()
	{
		cpu->setFlag(Carry, true);
		cpu->setFlag(HalfCarry, false);
		cpu->setFlag(Subtract, false);
	}
	inline void EI()
	{
		cpu->s.shouldSetIME = true;
	}
	inline void DI()
	{
		cpu->s.IME = false;
	}
	inline void DAA()
	{
		if (cpu->getFlag(Subtract)) 
		{
			if (cpu->getFlag(Carry))
				cpu->registers.A.val -= 0x60;

			if (cpu->getFlag(HalfCarry))
				cpu->registers.A.val -= 0x06;
		}
		else 
		{
			if (cpu->getFlag(Carry) || cpu->registers.A.val > 0x99)
			{
				cpu->registers.A.val += 0x60;
				cpu->setFlag(Carry, true);
			}
			if (cpu->getFlag(HalfCarry) || (cpu->registers.A.val & 0x0F) > 0x09)
				cpu->registers.A.val += 0x06;
		}

		cpu->setFlag(Zero, cpu->registers.A.val == 0);
		cpu->setFlag(HalfCarry, false);
	}

	inline void STOP() 
	{
		if (cpu->s.prepareSpeedSwitch && System::Current() == GBSystem::CGB)
		{
			cpu->s.cgbDoubleSpeed = !cpu->s.cgbDoubleSpeed;
			cpu->tCyclesPerM = cpu->s.cgbDoubleSpeed ? 2 : 4;
			cpu->s.prepareSpeedSwitch = false;

			cpu->s.divCounter = 0;

			if (cpu->pendingInterrupt() && cpu->s.IME)
				return; // if interrupts and IME, then stop is 1 byte opcode

			cpu->s.halted = true;
			cpu->s.stopState = true;
			cpu->haltStartCycles = cpu->gb.cycleCount();
		}

		cpu->s.PC++;
	}
	inline void HALT() 
	{
		cpu->s.halted = true;
		cpu->haltStartCycles = cpu->gb.cycleCount();

		if (!cpu->s.IME && cpu->pendingInterrupt())
			cpu->s.haltBug = true;
	}

	inline void JR(int8_t val) 
	{
		cpu->s.PC += val;
		cpu->addCycle();
	}
	inline void JR_CON(bool cond, int8_t val) 
	{
		if (cond)
			JR(val);
	}

	inline void JP_HL()
	{
		cpu->s.PC = cpu->registers.HL.val;

		if (cpu->retEvent != nullptr)
			cpu->retEvent(); // In case game uses pop hl + jp hl instead of ret.
	}
	inline void JP(uint16_t addr) 
	{
		cpu->s.PC = addr;
		cpu->addCycle();
	}
	inline void JP_CON(bool cond, uint16_t addr) 
	{
		if (cond) 
			JP(addr);
	}

	inline void POP(uint16_t& val) 
	{
		val = static_cast<uint16_t>(cpu->read8(cpu->s.SP.val++));
		val |= (cpu->read8(cpu->s.SP.val++) << 8);
	}
	inline void POP_AF()
	{
		uint16_t val;
		POP(val);
		cpu->registers.AF = val & 0xFFF0;
	}
	inline void PUSH(uint16_t val) 
	{
		cpu->addCycle();
		cpu->write8(--cpu->s.SP.val, val >> 8);
		cpu->write8(--cpu->s.SP.val, val & 0xFF);
	}

	inline void RET()
	{
		POP(cpu->s.PC);
		cpu->addCycle();

		if (cpu->retEvent != nullptr)
			cpu->retEvent();
	}
	inline void RET_CON(bool cond) 
	{
		cpu->addCycle();
		if (cond) RET();
	}
	inline void RET1()
	{
		RET();
		cpu->s.IME = true;
	}

	inline void CALL(uint16_t addr)
	{
		PUSH(cpu->s.PC);
		cpu->s.PC = addr;
	}
	inline void CALL_CON(bool cond, uint16_t addr) 
	{
		if (cond)
			CALL(addr);
	}
	inline void RST(uint16_t addr)
	{
		PUSH(cpu->s.PC);
		cpu->s.PC = addr;
	}
};