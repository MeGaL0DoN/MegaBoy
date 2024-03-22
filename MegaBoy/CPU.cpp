#include <iostream>
#include <fstream>

#include "CPU.h"
#include "instructionsEngine.h"

InstructionsEngine instructions;

CPU::CPU()
{
	instructions = { this };
}

void CPU::loadROM(std::string_view path)
{
	std::ifstream ifs(path.data(), std::ios::binary | std::ios::ate);
	std::ifstream::pos_type pos = ifs.tellg();

	ifs.seekg(0, std::ios::beg);
	ifs.read(reinterpret_cast<char*>(&MEM[0]), pos);
	ifs.close();
}

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
		case 6: return read8(registers.HL.val);
		case 7: return registers.A.val;
		default: throw "Unknown index!";
	}
}

uint8_t CPU::execute()
{
	if (halted) return 1;

	opcode = read8(PC);
	if (opcode == 0xCB)
	{
		opcode = read8(++PC);
		executePrefixed();
	}
	else
		executeUnprefixed();

	if (setIME)
	{
		IME = true;
		setIME = false;
	}

	return cycles;
}

void CPU::executeUnprefixed()
{
	uint8_t operand = read8(PC + 1);
	uint16_t operand16 = read16(PC + 1);

	uint8_t outRegInd = opcode & 0x07;
	uint8_t inRegInd = (opcode >> 3) & 0x07;

	switch (opcode)
	{
	// 0x00: NOP
	// no operation
	case 0x01: 
		instructions.loadToReg(registers.BC, operand16);
		break;
	case 0x02: 
		instructions.loadToAddr(registers.BC, registers.A);
		break;
	case 0x03: 
		instructions.INCR(registers.BC);
		break;
	case 0x04: 
		instructions.INCR(registers.B);
		break;
	case 0x05:
		instructions.DECR(registers.B);
		break;
	case 0x06: 
		instructions.loadToReg(registers.B, operand);
		break;
	case 0x07:
		instructions.RLCA();
		break;
	case 0x08:
		instructions.loadToAddr(operand16, SP);
		break;
	case 0x09:
		instructions.addToHL(registers.BC);
		break;
	case 0x0A:
		instructions.loadToReg(registers.A, registers.BC); 
		break;
	case 0x0B:
		instructions.DECR(registers.BC);
		break;
	case 0x0C:
		instructions.INCR(registers.C);
		break;
	case 0x0D:
		instructions.DECR(registers.C);
		break;
	case 0x0E:
		instructions.loadToReg(registers.C, operand);
		break;
	case 0x0F:
		instructions.RRCA();
		break;
	case 0x10: 
		instructions.STOP();
		break;
	case 0x11:
		instructions.loadToReg(registers.DE, operand16);
		break;
	case 0x12:  
		instructions.loadToAddr(registers.DE, registers.A); 
		break;
	case 0x13:
		instructions.INCR(registers.DE);
		break;
	case 0x14:
		instructions.INCR(registers.D);
		break;
	case 0x15:
		instructions.DECR(registers.D);
		break;
	case 0x16:
		instructions.loadToReg(registers.D, operand);
		break;
	case 0x17:
		instructions.RLA();
		break;
	case 0x18:
		instructions.JR(operand);
		break;
	case 0x19:
		instructions.addToHL(registers.DE);
		break;
	case 0x1A:
		instructions.loadToReg(registers.A, registers.DE);
		break;
	case 0x1B:
		instructions.DECR(registers.DE);
		break;
	case 0x1C:
		instructions.INCR(registers.E);
		break;
	case 0x1D:
		instructions.DECR(registers.E);
		break;
	case 0x1E:
		instructions.loadToReg(registers.E, operand);
		break;
	case 0x1F:
		instructions.RRA();
		break;
	case 0x20:
		instructions.JR_CON(!registers.getFlag(FlagType::Zero), operand);
		break;
	case 0x21:
		instructions.loadToReg(registers.HL, operand16);
		break;
	case 0x22:
		instructions.LD_HLI_A();
		break;
	case 0x23:
		instructions.INCR(registers.HL);
		break;
	case 0x24:
		instructions.INCR(registers.H);
		break;
	case 0x25:
		instructions.DECR(registers.H);
		break;
	case 0x26:
		instructions.loadToReg(registers.H, operand);
		break;
	case 0x27:
		// DAA // TODO
		break;
	case 0x28:
		instructions.JR_CON(registers.getFlag(FlagType::Zero), operand);
		break;
	case 0x29:
		instructions.addToHL(registers.HL);
		break;
	case 0x2A:
		instructions.LD_A_HLI();
		break;
	case 0x2B:
		instructions.DECR(registers.HL);
		break;
	case 0x2C:
		instructions.INCR(registers.L);
		break;
	case 0x2D:
		instructions.DECR(registers.L);
		break;
	case 0x2E:
		instructions.loadToReg(registers.L, operand);
		break;
	case 0x2F:
		instructions.CPL();
		break;
	case 0x30:
		instructions.JR_CON(!registers.getFlag(FlagType::Carry), operand);
		break;
	case 0x31:
		instructions.loadToReg(SP, operand16);
		break;
	case 0x32:
		instructions.LD_HLD_A();
		break;
	case 0x33:
		instructions.INCR(SP);
		break;
	case 0x34:
		instructions.INCR(registers.HL);
		break;
	case 0x35:
		instructions.DECR(registers.HL);
		break;
	case 0x36:
		instructions.loadToAddr(registers.HL.val, operand);
		break;
	case 0x37:
		instructions.SCF();
		break;
	case 0x38:
		instructions.JR_CON(registers.getFlag(FlagType::Carry), operand);
		break;
	case 0x39:
		instructions.addToHL(SP);
		break;
	case 0x3A:
		instructions.LD_A_HLD();
		break;
	case 0x3B:
		instructions.DECR(SP);
		break;
	case 0x3C:
		instructions.INCR(registers.A);
		break;
	case 0x3D:
		instructions.DECR(registers.A);
		break;
	case 0x3E:
		instructions.loadToReg(registers.A, operand);
		break;
	case 0x3F:
		instructions.CCF();
		break;

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
		instructions.loadToReg(inRegInd, outRegInd);
		break;

	case 0x76:
		instructions.HALT();
		break;

	case 0x80:
	case 0x81:
	case 0x82:
	case 0x83:
	case 0x84:
	case 0x85:
	case 0x86:
	case 0x87:
		instructions.ADD(outRegInd); // TO CHECK IND
		break;

	case 0x88:
	case 0x89:
	case 0x8A:
	case 0x8B:
	case 0x8C:
	case 0x8D:
	case 0x8E:
	case 0x8F:
		instructions.ADC(outRegInd); // TO CHECK IND
		break;

	case 0x90:
	case 0x91:
	case 0x92:
	case 0x93:
	case 0x94:
	case 0x95:
	case 0x96:
	case 0x97:
		instructions.SUB(outRegInd); // TO CHECK IND
		break;

	case 0x98:
	case 0x99:
	case 0x9A:
	case 0x9B:
	case 0x9C:
	case 0x9D:
	case 0x9E:
	case 0x9F:
		instructions.SBC(outRegInd); // TO CHECK IND
		break;

	case 0xA0:
	case 0xA1:
	case 0xA2:
	case 0xA3:
	case 0xA4:
	case 0xA5:
	case 0xA6:
	case 0xA7:
		instructions.AND(outRegInd); // TO CHECK IND
		break;

	case 0xA8:
	case 0xA9:
	case 0xAA:
	case 0xAB:
	case 0xAC:
	case 0xAD:
	case 0xAE:
	case 0xAF:
		instructions.XOR(outRegInd); // TO CHECK IND
		break;

	case 0xB0:
	case 0xB1:
	case 0xB2:
	case 0xB3:
	case 0xB4:
	case 0xB5:
	case 0xB6:
	case 0xB7:
		instructions.OR(outRegInd); // TO CHECK IND
		break;

	case 0xB8:
	case 0xB9:
	case 0xBA:
	case 0xBB:
	case 0xBC:
	case 0xBD:
	case 0xBE:
	case 0xBF:
		instructions.CP(outRegInd); // TO CHECK IND
		break;

	case 0xC0:
		instructions.RET_CON(!registers.getFlag(FlagType::Zero));
		break;
	case 0xC1:
		instructions.POP(registers.BC.val);
		break;
	case 0xC2:
		instructions.JP_CON(!registers.getFlag(FlagType::Zero), operand16);
		break;
	case 0xC3:
		instructions.JP(operand16);
		break;
	case 0xC4:
		instructions.CALL_CON(!registers.getFlag(FlagType::Zero), operand16);
		break;
	case 0xC5:
		instructions.PUSH(registers.BC.val);
		break;
	case 0xC6:
		instructions.ADD(registers.A, operand);
		break;
	case 0xC7:
		instructions.RST(0x00); // TO CHECK
		break;
	case 0xC8:
		instructions.RET_CON(registers.getFlag(FlagType::Zero));
		break;
	case 0xC9:
		instructions.RET();
		break;
	case 0xCA:
		instructions.JP_CON(registers.getFlag(FlagType::Zero), operand16);
		break;
	case 0xCC:
		instructions.CALL_CON(registers.getFlag(FlagType::Zero), operand16);
		break;
	case 0xCD:
		instructions.CALL(operand16);
		break;
	case 0xCE:
		instructions.ADC(registers.A, operand);
		break;
	case 0xCF:
		instructions.RST(0x08);
		break;
	case 0xD0:
		instructions.RET_CON(!registers.getFlag(FlagType::Carry));
		break;
	case 0xD1:
		instructions.POP(registers.DE.val);
		break;
	case 0xD2:
		instructions.JP_CON(!registers.getFlag(FlagType::Carry), operand16);
		break;
	case 0xD4:
		instructions.CALL_CON(!registers.getFlag(FlagType::Carry), operand16);
		break;
	case 0xD5:
		instructions.PUSH(registers.DE.val);
		break;
	case 0xD6:
		instructions.SUB(registers.A, operand);
		break;
	case 0xD7:
		instructions.RST(0x10);
		break;
	case 0xD8:
		instructions.RET_CON(registers.getFlag(FlagType::Carry));
		break;
	case 0xD9:
		// RET1 TODO
		break;
	case 0xDA:
		instructions.JP_CON(registers.getFlag(FlagType::Carry), operand16);
		break;
	case 0xDC:
		instructions.CALL_CON(registers.getFlag(FlagType::Carry), operand16);
		break;
	case 0xDE:
		instructions.SBC(registers.A, operand);
		break;
	case 0xDF:
		instructions.RST(0x18);
		break;
	case 0xE0: // TO CHECK
		instructions.loadToAddr(0xFF00 + operand, registers.A.val);
		break;
	case 0xE1:
		instructions.POP(registers.HL.val);
		break;
	case 0xE2:
		instructions.LD_C_A();
		break;
	case 0xE5:
		instructions.PUSH(registers.HL.val);
		break;
	case 0xE6:
		instructions.AND(registers.A, operand);
		break;
	case 0xE7:
		instructions.RST(0x20);
		break;
	case 0xE8:
		instructions.addToSP(operand);
		break;
	case 0xE9:
		instructions.JP(registers.HL);
		break;
	case 0xEA:
		instructions.loadToAddr(operand16, registers.A);
		break;
	case 0xEE:
		instructions.XOR(registers.A, operand);
		break;
	case 0xEF:
		instructions.RST(0x28);
		break;
	case 0xF0: // TO CHECK
		instructions.loadToReg(registers.A, static_cast<uint16_t>(0xFF00 + operand));
		break;
	case 0xF1:
		instructions.POP(registers.AF.val);
		break;
	case 0xF2: 
		instructions.LD_A_C();
		break;
	case 0xF3:
		instructions.DI();
		break;
	case 0xF5:
		instructions.PUSH(registers.AF.val);
		break;
	case 0xF6:
		instructions.OR(registers.A, operand);
		break;
	case 0xF7:
		instructions.RST(0x30);
		break;
	case 0xF8:

		break;

	default:
		std::cout << "Unknown unprefixed opcode: " << std::hex << opcode << "\n";
		cycles = 1;
		PC++;
	}
}

void CPU::executePrefixed()
{
	switch (opcode)
	{
	case 0x00:
		break;

	default:
		std::cout << "Unknown prefixed opcode: " << std::hex << opcode << "\n";
		cycles = 1;
		PC++;
	}
}