#include "CPU.h"
#include "instructions.h"
#include <iostream>

uint8_t CPU::getRegister(uint8_t ind)
{
	switch (ind)
	{
		case 0: return registers.B;
		case 1: return registers.C;
		case 2: return registers.D;
		case 3: return registers.E;
		case 4: return registers.H;
		case 5: return registers.L;
		case 6: return read8(registers.get16Bit({ registers.H, registers.L }));
		case 7: return registers.A;
		default: throw "Index out of range";
	}
}

void CPU::setRegister(uint8_t ind, uint8_t val)
{
	switch (ind)
	{
		case 0: registers.B = val; break;
		case 1: registers.C = val; break;
		case 2:	registers.D = val; break;
		case 3: registers.E = val; break;
		case 4: registers.H = val; break;
		case 5: registers.L = val; break;
		case 6: write8(registers.get16Bit({ registers.H, registers.L }), val);
		case 7: registers.A = val; break;
		default: throw "Index out of ranger-";
	}
}

uint8_t CPU::execute()
{
	opcode = RAM[PC];
	if (opcode == 0xCB)
	{
		opcode = RAM[++PC];
		executePrefixed();
	}
	else
		executeUnprefixed();

	return cycles;
}

void CPU::executeUnprefixed()
{
	uint8_t operand = RAM[PC + 1];
	uint16_t operand16 = (RAM[PC + 2] << 8) | operand;

	switch (opcode)
	{
	case 0x00: // NOP
		// no operation
		cycles = 1;
		PC++;
		break;
	case 0x01: 
		Instructions::loadToReg({ registers.B, registers.C }, operand16);
		break;
	case 0x02: 
		Instructions::loadAtoAddr({ registers.B, registers.C });
		break;
	case 0x03: 
		Instructions::increment({ registers.B, registers.C });
		break;
	case 0x04: 
		Instructions::increment(registers.B);
		break;
	case 0x05:
		Instructions::decrement(registers.B);
		break;
	case 0x06: 
		Instructions::loadToReg(registers.B, operand);
		break;
	case 0x07:
		Instructions::RLCA();
		break;
	case 0x08:
		Instructions::loadToAddr(operand16, SP);
		break;
	case 0x09:
		Instructions::addToHL({ registers.B, registers.C });
		break;
	case 0x0A:
		Instructions::loadAddrToA({ registers.B, registers.C });
		break;
	case 0x0B:
		Instructions::decrement({ registers.B, registers.C });
		break;
	case 0x0C:
		Instructions::increment(registers.C);
		break;
	case 0x0D:
		Instructions::decrement(registers.C);
		break;
	case 0x0E:
		Instructions::loadToReg(registers.C, operand);
		break;
	case 0x0F:
		Instructions::RLCA();
		break;
	default:
		std::cout << "Unknown unprefixed opcode: " << std::hex << opcode << "\n";
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
	}
}