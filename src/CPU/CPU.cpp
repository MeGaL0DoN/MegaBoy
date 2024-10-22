#include <cassert>
#include "CPU.h"
#include "instructionsEngine.h"
#include "../defines.h"
#include "../GBCore.h"

CPU::CPU(GBCore& gbCore) : gbCore(gbCore), instructions(std::make_unique<InstructionsEngine>(this))
{
	reset();
}

CPU::~CPU() = default;

void CPU::reset()
{
	s.reset();
	registers.reset();

	executingBootROM = false;
	cycles = 0;
}

void CPU::saveState(std::ofstream& st) const
{
	ST_WRITE(s);
	ST_WRITE(registers.AF.val);
	ST_WRITE(registers.BC.val);
	ST_WRITE(registers.DE.val);
	ST_WRITE(registers.HL.val);
}

void CPU::loadState(std::ifstream& st)
{
	ST_READ(s);
	ST_READ(registers.AF.val);
	ST_READ(registers.BC.val);
	ST_READ(registers.DE.val);
	ST_READ(registers.HL.val);
}

void CPU::addCycle()
{
	cycles++;
	gbCore.stepComponents();
	s.interruptLatch = interruptsPending();
}

void CPU::write8(uint16_t addr, uint8_t val)
{
	gbCore.mmu.write8(addr, val);
	addCycle();
}
uint8_t CPU::read8(uint16_t addr)
{
	uint8_t val = gbCore.mmu.read8(addr);
	addCycle();
	return val;
}

uint8_t HL_ref;
uint8_t& CPU::getRegister(uint8_t ind)
{
	switch (ind)
	{
		case 0: return registers.B.val;
		case 1: return registers.C.val;
		case 2: return registers.D.val;
		case 3: return registers.E.val;
		case 4: return registers.H.val;
		case 5: return registers.L.val;
		case 6: 
		{
			HL_ref = gbCore.mmu.read8(registers.HL.val);
			return HL_ref;
		}
		case 7: return registers.A.val;
	}

	assert(false);
	return (uint8_t&)s;
}

constexpr uint16_t STOP_PERIOD_CYCLES = 32768;

bool CPU::handleHaltedState()
{
	if (!s.halted) [[likely]]
		return false;

	addCycle();
	cycles += handleInterrupts();

	if (s.stopState)
	{
		s.stopCycleCounter += cycles;
		if (s.stopCycleCounter >= STOP_PERIOD_CYCLES)
		{
			s.halted = false;
			s.stopState = false;
			s.stopCycleCounter = 0;
		}
	}

	return true;
}

bool CPU::handleGHDMA()
{
	if (gbCore.mmu.gbc.hdma.status != GHDMAStatus::None) [[unlikely]]
	{
		if (gbCore.mmu.gbc.hdma.status == GHDMAStatus::GDMA || (gbCore.ppu->getMode() == PPUMode::HBlank && gbCore.ppu->getCycles() < MMU::GHDMA_BLOCK_CYCLES))
		{
			gbCore.mmu.executeGHDMA();
			return true;
		}
	}

	return false;
}

uint8_t CPU::execute()
{
	cycles = 0;

	if (s.shouldSetIME) [[unlikely]]
	{
		s.IME = true;
		s.shouldSetIME = false;
	}

	if (handleHaltedState()) [[unlikely]]
		return cycles;

	if (handleGHDMA()) [[unlikely]]
		return 1;

	opcode = read8(s.PC);

	if (s.halt_bug) [[unlikely]]
	{
		s.PC--;
		s.halt_bug = false;
	}

	executeMain();
	cycles += handleInterrupts();
	return cycles;
}

void CPU::executeMain()
{
	uint8_t outRegInd = opcode & 0x07;
	uint8_t inRegInd = (opcode >> 3) & 0x07;

	switch (opcode)
	{
	// 0x00: NOP
	case 0x00:
		s.PC++;
		cycles = 1;
		break;
	case 0x01: 
		instructions->loadToReg(registers.BC, read16(s.PC + 1));
		break;
	case 0x02: 
		instructions->loadToAddr(registers.BC, registers.A);
		break;
	case 0x03: 
		instructions->INCR(registers.BC);
		break;
	case 0x04: 
		instructions->INCR(registers.B.val);
		break;
	case 0x05:
		instructions->DECR(registers.B.val);
		break;
	case 0x06: 
		instructions->loadToReg(registers.B, read8(s.PC + 1));
		break;
	case 0x07:
		instructions->RLCA();
		break;
	case 0x08:
		instructions->loadToAddr(read16(s.PC + 1), s.SP);
		break;
	case 0x09:
		instructions->addToHL(registers.BC);
		break;
	case 0x0A:
		instructions->loadToReg(registers.A, registers.BC); 
		break;
	case 0x0B:
		instructions->DECR(registers.BC);
		break;
	case 0x0C:
		instructions->INCR(registers.C.val);
		break;
	case 0x0D:
		instructions->DECR(registers.C.val);
		break;
	case 0x0E:
		instructions->loadToReg(registers.C, read8(s.PC + 1));
		break;
	case 0x0F:
		instructions->RRCA();
		break;
	case 0x10: 
		instructions->STOP();
		break;
	case 0x11:
		instructions->loadToReg(registers.DE, read16(s.PC + 1));
		break;
	case 0x12:  
		instructions->loadToAddr(registers.DE, registers.A); 
		break;
	case 0x13:
		instructions->INCR(registers.DE);
		break;
	case 0x14:
		instructions->INCR(registers.D.val);
		break;
	case 0x15:
		instructions->DECR(registers.D.val);
		break;
	case 0x16:
		instructions->loadToReg(registers.D, read8(s.PC + 1));
		break;
	case 0x17:
		instructions->RLA();
		break;
	case 0x18:
		instructions->JR(read8(s.PC + 1));
		break;
	case 0x19:
		instructions->addToHL(registers.DE);
		break;
	case 0x1A:
		instructions->loadToReg(registers.A, registers.DE);
		break;
	case 0x1B:
		instructions->DECR(registers.DE);
		break;
	case 0x1C:
		instructions->INCR(registers.E.val);
		break;
	case 0x1D:
		instructions->DECR(registers.E.val);
		break;
	case 0x1E:
		instructions->loadToReg(registers.E, read8(s.PC + 1));
		break;
	case 0x1F:
		instructions->RRA();
		break;
	case 0x20:
		instructions->JR_CON(!registers.getFlag(FlagType::Zero), read8(s.PC + 1));
		break;
	case 0x21:
		instructions->loadToReg(registers.HL, read16(s.PC + 1));
		break;
	case 0x22:
		instructions->LD_HLI_A();
		break;
	case 0x23:
		instructions->INCR(registers.HL);
		break;
	case 0x24:
		instructions->INCR(registers.H.val);
		break;
	case 0x25:
		instructions->DECR(registers.H.val);
		break;
	case 0x26:
		instructions->loadToReg(registers.H, read8(s.PC + 1));
		break;
	case 0x27:
		instructions->DAA();
		break;
	case 0x28:
		instructions->JR_CON(registers.getFlag(FlagType::Zero), read8(s.PC + 1));
		break;
	case 0x29:
		instructions->addToHL(registers.HL);
		break;
	case 0x2A:
		instructions->LD_A_HLI();
		break;
	case 0x2B:
		instructions->DECR(registers.HL);
		break;
	case 0x2C:
		instructions->INCR(registers.L.val);
		break;
	case 0x2D:
		instructions->DECR(registers.L.val);
		break;
	case 0x2E:
		instructions->loadToReg(registers.L, read8(s.PC + 1));
		break;
	case 0x2F:
		instructions->CPL();
		break;
	case 0x30:
		instructions->JR_CON(!registers.getFlag(FlagType::Carry), read8(s.PC + 1));
		break;
	case 0x31:
		instructions->loadToReg(s.SP, read16(s.PC + 1));
		break;
	case 0x32:
		instructions->LD_HLD_A();
		break;
	case 0x33:
		instructions->INCR(s.SP);
		break;
	case 0x34:
		instructions->INCR_HL();
		break;
	case 0x35:
		instructions->DECR_HL();
		break;
	case 0x36:
		instructions->loadToAddr(registers.HL.val, read8(s.PC + 1));
		break;
	case 0x37:
		instructions->SCF();
		break;
	case 0x38:
		instructions->JR_CON(registers.getFlag(FlagType::Carry), read8(s.PC + 1));
		break;
	case 0x39:
		instructions->addToHL(s.SP);
		break;
	case 0x3A:
		instructions->LD_A_HLD();
		break;
	case 0x3B:
		instructions->DECR(s.SP);
		break;
	case 0x3C:
		instructions->INCR(registers.A.val);
		break;
	case 0x3D:
		instructions->DECR(registers.A.val);
		break;
	case 0x3E:
		instructions->loadToReg(registers.A, read8(s.PC + 1));
		break;
	case 0x3F:
		instructions->CCF();
		break;

	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x44:
	case 0x45:
	case 0x46:
	case 0x47:
	case 0x48:
	case 0x49:
	case 0x4A:
	case 0x4B:
	case 0x4C:
	case 0x4D:
	case 0x4E:
	case 0x4F:
	case 0x50:
	case 0x51:
	case 0x52:
	case 0x53:
	case 0x54:
	case 0x55:
	case 0x56:
	case 0x57:
	case 0x58:
	case 0x59:
	case 0x5A:
	case 0x5B:
	case 0x5C:
	case 0x5D:
	case 0x5E:
	case 0x5F:
	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x64:
	case 0x65:
	case 0x66:
	case 0x67:
	case 0x68:
	case 0x69:
	case 0x6A:
	case 0x6B:
	case 0x6C:
	case 0x6D:
	case 0x6E:
	case 0x6F:
	case 0x70:
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x77:
	case 0x78:
	case 0x79:
	case 0x7A:
	case 0x7B:
	case 0x7C:
	case 0x7D:
	case 0x7E:
	case 0x7F:
		instructions->loadToReg(inRegInd, outRegInd);
		break;

	case 0x76:
		instructions->HALT();
		break;

	case 0x80:
	case 0x81:
	case 0x82:
	case 0x83:
	case 0x84:
	case 0x85:
	case 0x86:
	case 0x87:
		instructions->ADD(outRegInd); 
		break;

	case 0x88:
	case 0x89:
	case 0x8A:
	case 0x8B:
	case 0x8C:
	case 0x8D:
	case 0x8E:
	case 0x8F:
		instructions->ADC(outRegInd);
		break;

	case 0x90:
	case 0x91:
	case 0x92:
	case 0x93:
	case 0x94:
	case 0x95:
	case 0x96:
	case 0x97:
		instructions->SUB(outRegInd); 
		break;

	case 0x98:
	case 0x99:
	case 0x9A:
	case 0x9B:
	case 0x9C:
	case 0x9D:
	case 0x9E:
	case 0x9F:
		instructions->SBC(outRegInd); 
		break;

	case 0xA0:
	case 0xA1:
	case 0xA2:
	case 0xA3:
	case 0xA4:
	case 0xA5:
	case 0xA6:
	case 0xA7:
		instructions->AND(outRegInd); 
		break;

	case 0xA8:
	case 0xA9:
	case 0xAA:
	case 0xAB:
	case 0xAC:
	case 0xAD:
	case 0xAE:
	case 0xAF:
		instructions->XOR(outRegInd); 
		break;

	case 0xB0:
	case 0xB1:
	case 0xB2:
	case 0xB3:
	case 0xB4:
	case 0xB5:
	case 0xB6:
	case 0xB7:
		instructions->OR(outRegInd); 
		break;

	case 0xB8:
	case 0xB9:
	case 0xBA:
	case 0xBB:
	case 0xBC:
	case 0xBD:
	case 0xBE:
	case 0xBF:
		instructions->CP(outRegInd); 
		break;

	case 0xC0:
		instructions->RET_CON(!registers.getFlag(FlagType::Zero));
		break;
	case 0xC1:
		instructions->POP(registers.BC.val);
		break;
	case 0xC2:
		instructions->JP_CON(!registers.getFlag(FlagType::Zero), read16(s.PC + 1));
		break;
	case 0xC3:
		instructions->JP(read16(s.PC + 1)); 
		break;
	case 0xC4:
		instructions->CALL_CON(!registers.getFlag(FlagType::Zero), read16(s.PC + 1));
		break;
	case 0xC5:
		instructions->PUSH(registers.BC.val);
		break;
	case 0xC6:
		instructions->ADD(registers.A, read8(s.PC + 1));
		break;
	case 0xC7:
		instructions->RST(0x00);
		break;
	case 0xC8:
		instructions->RET_CON(registers.getFlag(FlagType::Zero));
		break;
	case 0xC9:
		instructions->RET();
		break;
	case 0xCA:
		instructions->JP_CON(registers.getFlag(FlagType::Zero), read16(s.PC + 1));
		break;
	case 0xCB: // PREFIXED OPCODES
		opcode = read8(++s.PC);
		executePrefixed();
		break;
	case 0xCC:
		instructions->CALL_CON(registers.getFlag(FlagType::Zero), read16(s.PC + 1));
		break;
	case 0xCD:
		instructions->CALL(read16(s.PC + 1));
		break;
	case 0xCE:
		instructions->ADC(registers.A, read8(s.PC + 1));
		break;
	case 0xCF:
		instructions->RST(0x08);
		break;
	case 0xD0:
		instructions->RET_CON(!registers.getFlag(FlagType::Carry));
		break;
	case 0xD1:
		instructions->POP(registers.DE.val);
		break;
	case 0xD2:
		instructions->JP_CON(!registers.getFlag(FlagType::Carry), read16(s.PC + 1));
		break;
	case 0xD4:
		instructions->CALL_CON(!registers.getFlag(FlagType::Carry), read16(s.PC + 1));
		break;
	case 0xD5:
		instructions->PUSH(registers.DE.val);
		break;
	case 0xD6:
		instructions->SUB(registers.A, read8(s.PC + 1));
		break;
	case 0xD7:
		instructions->RST(0x10);
		break;
	case 0xD8:
		instructions->RET_CON(registers.getFlag(FlagType::Carry));
		break;
	case 0xD9:
		instructions->RET1();
		break;
	case 0xDA:
		instructions->JP_CON(registers.getFlag(FlagType::Carry), read16(s.PC + 1));
		break;
	case 0xDC:
		instructions->CALL_CON(registers.getFlag(FlagType::Carry), read16(s.PC + 1));
		break;
	case 0xDE:
		instructions->SBC(registers.A, read8(s.PC + 1));
		break;
	case 0xDF:
		instructions->RST(0x18);
		break;
	case 0xE0: 
		instructions->LD_OFFSET_A(read8(s.PC + 1));
		break;
	case 0xE1:
		instructions->POP(registers.HL.val);
		break;
	case 0xE2:
		instructions->LD_C_A();
		break;
	case 0xE5:
		instructions->PUSH(registers.HL.val);
		break;
	case 0xE6:
		instructions->AND(registers.A, read8(s.PC + 1));
		break;
	case 0xE7:
		instructions->RST(0x20);
		break;
	case 0xE8:
		instructions->addToSP(read8(s.PC + 1));
		break;
	case 0xE9:
		instructions->JP(registers.HL);
		break;
	case 0xEA:
		instructions->loadToAddr(read16(s.PC + 1), registers.A);
		break;
	case 0xEE:
		instructions->XOR(registers.A, read8(s.PC + 1));
		break;
	case 0xEF:
		instructions->RST(0x28);
		break;
	case 0xF0: 
		instructions->LD_A_OFFSET(read8(s.PC + 1));
		break;
	case 0xF1:
		instructions->POP_AF();
		break;
	case 0xF2: 
		instructions->LD_A_C();
		break;
	case 0xF3:
		instructions->DI();
		break;
	case 0xF5:
		instructions->PUSH(registers.AF.val);
		break;
	case 0xF6:
		instructions->OR(registers.A, read8(s.PC + 1));
		break;
	case 0xF7:
		instructions->RST(0x30);
		break;
	case 0xF8:
		instructions->LD_HL_SP(read8(s.PC + 1));
		break;
	case 0xF9:
		instructions->LD_SP_HL();
		break;
	case 0xFA:
		instructions->loadToReg(registers.A, read16(s.PC + 1));
		break;
	case 0xFB:
		instructions->EI();
		break;
	case 0xFE:
		instructions->CP(registers.A, read8(s.PC + 1));
		break;
	case 0xFF:
		instructions->RST(0x38);
		break;

	default:
		s.PC++;
	}
}

void CPU::executePrefixed()
{
	const uint8_t regInd = opcode & 0x07;
	const uint8_t bit = (opcode >> 3) & 0x07;

	switch (opcode)
	{
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		instructions->RLC(regInd);
		break;

	case 0x08:
	case 0x09:
	case 0x0A:
	case 0x0B:
	case 0x0C:
	case 0x0D:
	case 0x0E:
	case 0x0F:
		instructions->RRC(regInd);
		break;

	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17:
		instructions->RL(regInd);
		break;

	case 0x18:
	case 0x19:
	case 0x1A:
	case 0x1B:
	case 0x1C:
	case 0x1D:
	case 0x1E:
	case 0x1F:
		instructions->RR(regInd);
		break;

	case 0x20:
	case 0x21:
	case 0x22:
	case 0x23:
	case 0x24:
	case 0x25:
	case 0x26:
	case 0x27:
		instructions->SLA(regInd);
		break;

	case 0x28:
	case 0x29:
	case 0x2A:
	case 0x2B:
	case 0x2C:
	case 0x2D:
	case 0x2E:
	case 0x2F:
		instructions->SRA(regInd);
		break;

	case 0x30:
	case 0x31:
	case 0x32:
	case 0x33:
	case 0x34:
	case 0x35:
	case 0x36:
	case 0x37:
		instructions->SWAP(regInd);
		break;

	case 0x38:
	case 0x39:
	case 0x3A:
	case 0x3B:
	case 0x3C:
	case 0x3D:
	case 0x3E:
	case 0x3F:
		instructions->SRL(regInd);
		break;

	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x44:
	case 0x45:
	case 0x46:
	case 0x47:
	case 0x48:
	case 0x49:
	case 0x4A:
	case 0x4B:
	case 0x4C:
	case 0x4D:
	case 0x4E:
	case 0x4F:
	case 0x50:
	case 0x51:
	case 0x52:
	case 0x53:
	case 0x54:
	case 0x55:
	case 0x56:
	case 0x57:
	case 0x58:
	case 0x59:
	case 0x5A:
	case 0x5B:
	case 0x5C:
	case 0x5D:
	case 0x5E:
	case 0x5F:
	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x64:
	case 0x65:
	case 0x66:
	case 0x67:
	case 0x68:
	case 0x69:
	case 0x6A:
	case 0x6B:
	case 0x6C:
	case 0x6D:
	case 0x6E:
	case 0x6F:
	case 0x70:
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x77:
	case 0x78:
	case 0x79:
	case 0x7A:
	case 0x7B:
	case 0x7C:
	case 0x7D:
	case 0x7E:
	case 0x7F:
		instructions->BIT(bit, regInd);
		break;

	case 0x80:
	case 0x81:
	case 0x82:
	case 0x83:
	case 0x84:
	case 0x85:
	case 0x86:
	case 0x87:
	case 0x88:
	case 0x89:
	case 0x8A:
	case 0x8B:
	case 0x8C:
	case 0x8D:
	case 0x8E:
	case 0x8F:
	case 0x90:
	case 0x91:
	case 0x92:
	case 0x93:
	case 0x94:
	case 0x95:
	case 0x96:
	case 0x97:
	case 0x98:
	case 0x99:
	case 0x9A:
	case 0x9B:
	case 0x9C:
	case 0x9D:
	case 0x9E:
	case 0x9F:
	case 0xA0:
	case 0xA1:
	case 0xA2:
	case 0xA3:
	case 0xA4:
	case 0xA5:
	case 0xA6:
	case 0xA7:
	case 0xA8:
	case 0xA9:
	case 0xAA:
	case 0xAB:
	case 0xAC:
	case 0xAD:
	case 0xAE:
	case 0xAF:
	case 0xB0:
	case 0xB1:
	case 0xB2:
	case 0xB3:
	case 0xB4:
	case 0xB5:
	case 0xB6:
	case 0xB7:
	case 0xB8:
	case 0xB9:
	case 0xBA:
	case 0xBB:
	case 0xBC:
	case 0xBD:
	case 0xBE:
	case 0xBF:
		instructions->RES(bit, regInd);
		break;

	case 0xC0:
	case 0xC1:
	case 0xC2:
	case 0xC3:
	case 0xC4:
	case 0xC5:
	case 0xC6:
	case 0xC7:
	case 0xC8:
	case 0xC9:
	case 0xCA:
	case 0xCB:
	case 0xCC:
	case 0xCD:
	case 0xCE:
	case 0xCF:
	case 0xD0:
	case 0xD1:
	case 0xD2:
	case 0xD3:
	case 0xD4:
	case 0xD5:
	case 0xD6:
	case 0xD7:
	case 0xD8:
	case 0xD9:
	case 0xDA:
	case 0xDB:
	case 0xDC:
	case 0xDD:
	case 0xDE:
	case 0xDF:
	case 0xE0:
	case 0xE1:
	case 0xE2:
	case 0xE3:
	case 0xE4:
	case 0xE5:
	case 0xE6:
	case 0xE7:
	case 0xE8:
	case 0xE9:
	case 0xEA:
	case 0xEB:
	case 0xEC:
	case 0xED:
	case 0xEE:
	case 0xEF:
	case 0xF0:
	case 0xF1:
	case 0xF2:
	case 0xF3:
	case 0xF4:
	case 0xF5:
	case 0xF6:
	case 0xF7:
	case 0xF8:
	case 0xF9:
	case 0xFA:
	case 0xFB:
	case 0xFC:
	case 0xFD:
	case 0xFE:
	case 0xFF:
		instructions->SET(bit, regInd);
		break;

	default:
		s.PC++;
	}
}