#include <sstream>
#include <iomanip>
#include <array>

#include "CPU.h"
#include "../GBCore.h"
#include "../defines.h"

std::string CPU::disassemble(uint16_t addr, uint8_t(*readFunc)(uint16_t), uint8_t* instrLen)
{
    *instrLen = 0;
    const auto read8 = [this, &instrLen, &readFunc](uint16_t addr) { (*instrLen)++; return readFunc(addr); };

    uint8_t opcode { read8(addr) };
    std::stringstream ss;

    if (opcode == 0xCB) [[unlikely]]
    {
        opcode = read8(addr + 1);

        const uint8_t x = (opcode >> 6) & 0x3;
        const uint8_t y = (opcode >> 3) & 0x7;
        const uint8_t z = opcode & 0x7;

        constexpr std::array regNames { "B", "C", "D", "E", "H", "L", "[HL]", "A" };

        switch (x) 
        {
        case 0:
            switch (y) 
            {
                case 0: ss << "RLC " << regNames[z]; break;
                case 1: ss << "RRC " << regNames[z]; break;
                case 2: ss << "RL " << regNames[z]; break;
                case 3: ss << "RR " << regNames[z]; break;
                case 4: ss << "SLA " << regNames[z]; break;
                case 5: ss << "SRA " << regNames[z]; break;
                case 6: ss << "SWAP " << regNames[z]; break;
                case 7: ss << "SRL " << regNames[z]; break;
            }
            break;
        case 1: 
            ss << "BIT " << (int)y << "," << regNames[z];
            break;
        case 2: 
            ss << "RES " << (int)y << "," << regNames[z];
            break;
        case 3:
            ss << "SET " << (int)y << "," << regNames[z];
            break;
        }
        return ss.str();
    }

    switch (opcode) 
    {
    case 0x00: return "NOP";
    case 0x01: ss << "LD BC, $" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0x02: return "LD [BC],A";
    case 0x03: return "INC BC";
    case 0x04: return "INC B";
    case 0x05: return "DEC B";
    case 0x06: ss << "LD B,$" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0x07: return "RLCA";
    case 0x08: ss << "LD [$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)) << "],SP"; return ss.str();
    case 0x09: return "ADD HL,BC";
    case 0x0A: return "LD A,[BC]";
    case 0x0B: return "DEC BC";
    case 0x0C: return "INC C";
    case 0x0D: return "DEC C";
    case 0x0E: ss << "LD C,$" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0x0F: return "RRCA";

    case 0x10: return "STOP";
    case 0x11: ss << "LD DE,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0x12: return "LD [DE],A";
    case 0x13: return "INC DE";
    case 0x14: return "INC D";
    case 0x15: return "DEC D";
    case 0x16: ss << "LD D,$" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0x17: return "RLA";
    case 0x18: ss << "JR $" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(addr + 2 + (int8_t)read8(addr + 1)); return ss.str();
    case 0x19: return "ADD HL,DE";
    case 0x1A: return "LD A,[DE]";
    case 0x1B: return "DEC DE";
    case 0x1C: return "INC E";
    case 0x1D: return "DEC E";
    case 0x1E: ss << "LD E,$" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0x1F: return "RRA";

    case 0x20: ss << "JR NZ,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(addr + 2 + (int8_t)read8(addr + 1)); return ss.str();
    case 0x21: ss << "LD HL,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0x22: return "LD [HL+],A";
    case 0x23: return "INC HL";
    case 0x24: return "INC H";
    case 0x25: return "DEC H";
    case 0x26: ss << "LD H,$" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0x27: return "DAA";
    case 0x28: ss << "JR Z,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(addr + 2 + (int8_t)read8(addr + 1)); return ss.str();
    case 0x29: return "ADD HL,HL";
    case 0x2A: return "LD A,[HL+]";
    case 0x2B: return "DEC HL";
    case 0x2C: return "INC L";
    case 0x2D: return "DEC L";
    case 0x2E: ss << "LD L,$" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0x2F: return "CPL";

    case 0x30: ss << "JR NC,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(addr + 2 + (int8_t)read8(addr + 1)); return ss.str();
    case 0x31: ss << "LD SP,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0x32: return "LD [HL-],A";
    case 0x33: return "INC SP";
    case 0x34: return "INC [HL]";
    case 0x35: return "DEC [HL]";
    case 0x36: ss << "LD [HL],$" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0x37: return "SCF";
    case 0x38: ss << "JR C,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(addr + 2 + (int8_t)read8(addr + 1)); return ss.str();
    case 0x39: return "ADD HL,SP";
    case 0x3A: return "LD A,[HL-]";
    case 0x3B: return "DEC SP";
    case 0x3C: return "INC A";
    case 0x3D: return "DEC A";
    case 0x3E: ss << "LD A,$" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0x3F: return "CCF";

    case 0x40: return "LD B,B";
    case 0x41: return "LD B,C";
    case 0x42: return "LD B,D";
    case 0x43: return "LD B,E";
    case 0x44: return "LD B,H";
    case 0x45: return "LD B,L";
    case 0x46: return "LD B,[HL]";
    case 0x47: return "LD B,A";
    case 0x48: return "LD C,B";
    case 0x49: return "LD C,C";
    case 0x4A: return "LD C,D";
    case 0x4B: return "LD C,E";
    case 0x4C: return "LD C,H";
    case 0x4D: return "LD C,L";
    case 0x4E: return "LD C,[HL]";
    case 0x4F: return "LD C,A";

    case 0x50: return "LD D,B";
    case 0x51: return "LD D,C";
    case 0x52: return "LD D,D";
    case 0x53: return "LD D,E";
    case 0x54: return "LD D,H";
    case 0x55: return "LD D,L";
    case 0x56: return "LD D,[HL]";
    case 0x57: return "LD D,A";
    case 0x58: return "LD E,B";
    case 0x59: return "LD E,C";
    case 0x5A: return "LD E,D";
    case 0x5B: return "LD E,E";
    case 0x5C: return "LD E,H";
    case 0x5D: return "LD E,L";
    case 0x5E: return "LD E,[HL]";
    case 0x5F: return "LD E,A";

    case 0x60: return "LD H,B";
    case 0x61: return "LD H,C";
    case 0x62: return "LD H,D";
    case 0x63: return "LD H,E";
    case 0x64: return "LD H,H";
    case 0x65: return "LD H,L";
    case 0x66: return "LD H,[HL]";
    case 0x67: return "LD H,A";
    case 0x68: return "LD L,B";
    case 0x69: return "LD L,C";
    case 0x6A: return "LD L,D";
    case 0x6B: return "LD L,E";
    case 0x6C: return "LD L,H";
    case 0x6D: return "LD L,L";
    case 0x6E: return "LD L,[HL]";
    case 0x6F: return "LD L,A";

    case 0x70: return "LD [HL],B";
    case 0x71: return "LD [HL],C";
    case 0x72: return "LD [HL],D";
    case 0x73: return "LD [HL],E";
    case 0x74: return "LD [HL],H";
    case 0x75: return "LD [HL],L";
    case 0x76: return "HALT";
    case 0x77: return "LD [HL],A";
    case 0x78: return "LD A,B";
    case 0x79: return "LD A,C";
    case 0x7A: return "LD A,D";
    case 0x7B: return "LD A,E";
    case 0x7C: return "LD A,H";
    case 0x7D: return "LD A,L";
    case 0x7E: return "LD A,[HL]";
    case 0x7F: return "LD A,A";

    case 0x80: return "ADD B";
    case 0x81: return "ADD C";
    case 0x82: return "ADD D";
    case 0x83: return "ADD E";
    case 0x84: return "ADD H";
    case 0x85: return "ADD L";
    case 0x86: return "ADD [HL]";
    case 0x87: return "ADD A";
    case 0x88: return "ADC B";
    case 0x89: return "ADC C";
    case 0x8A: return "ADC D";
    case 0x8B: return "ADC E";
    case 0x8C: return "ADC H";
    case 0x8D: return "ADC L";
    case 0x8E: return "ADC [HL]";
    case 0x8F: return "ADC A";

    case 0x90: return "SUB B";
    case 0x91: return "SUB C";
    case 0x92: return "SUB D";
    case 0x93: return "SUB E";
    case 0x94: return "SUB H";
    case 0x95: return "SUB L";
    case 0x96: return "SUB [HL]";
    case 0x97: return "SUB A";
    case 0x98: return "SBC B";
    case 0x99: return "SBC C";
    case 0x9A: return "SBC D";
    case 0x9B: return "SBC E";
    case 0x9C: return "SBC H";
    case 0x9D: return "SBC L";
    case 0x9E: return "SBC [HL]";
    case 0x9F: return "SBC A";

    case 0xA0: return "AND B";
    case 0xA1: return "AND C";
    case 0xA2: return "AND D";
    case 0xA3: return "AND E";
    case 0xA4: return "AND H";
    case 0xA5: return "AND L";
    case 0xA6: return "AND [HL]";
    case 0xA7: return "AND A";
    case 0xA8: return "XOR B";
    case 0xA9: return "XOR C";
    case 0xAA: return "XOR D";
    case 0xAB: return "XOR E";
    case 0xAC: return "XOR H";
    case 0xAD: return "XOR L";
    case 0xAE: return "XOR [HL]";
    case 0xAF: return "XOR A";

    case 0xB0: return "OR B";
    case 0xB1: return "OR C";
    case 0xB2: return "OR D";
    case 0xB3: return "OR E";
    case 0xB4: return "OR H";
    case 0xB5: return "OR L";
    case 0xB6: return "OR [HL]";
    case 0xB7: return "OR A";
    case 0xB8: return "CP B";
    case 0xB9: return "CP C";
    case 0xBA: return "CP D";
    case 0xBB: return "CP E";
    case 0xBC: return "CP H";
    case 0xBD: return "CP L";
    case 0xBE: return "CP [HL]";
    case 0xBF: return "CP A";

    case 0xC0: return "RET NZ";
    case 0xC1: return "POP BC";
    case 0xC2: ss << "JP NZ,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0xC3: ss << "JP $" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0xC4: ss << "CALL NZ,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0xC5: return "PUSH BC";
    case 0xC6: ss << "ADD $" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0xC7: return "RST $00";
    case 0xC8: return "RET Z";
    case 0xC9: return "RET";
    case 0xCA: ss << "JP Z,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0xCC: ss << "CALL Z,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0xCD: ss << "CALL $" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0xCE: ss << "ADC $" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0xCF: return "RST $08";

    case 0xD0: return "RET NC";
    case 0xD1: return "POP DE";
    case 0xD2: ss << "JP NC,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0xD3: return "INVALID";
    case 0xD4: ss << "CALL NC,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0xD5: return "PUSH DE";
    case 0xD6: ss << "SUB $" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0xD7: return "RST $10";
    case 0xD8: return "RET C";
    case 0xD9: return "RETI";
    case 0xDA: ss << "JP C,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0xDB: return "INVALID";
    case 0xDC: ss << "CALL C,$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)); return ss.str();
    case 0xDD: return "INVALID";
    case 0xDE: ss << "SBC $" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0xDF: return "RST $18";

    case 0xE0: ss << "LDH [$ff" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1) << "],A"; return ss.str();
    case 0xE1: return "POP HL";
    case 0xE2: return "LDH [C],A";
    case 0xE3: return "INVALID";
    case 0xE4: return "INVALID";
    case 0xE5: return "PUSH HL";
    case 0xE6: ss << "AND $" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0xE7: return "RST $20";
    case 0xE8: ss << "ADD SP,$" << std::hex << std::setw(2) << std::setfill('0') << (int)(int8_t)read8(addr + 1); return ss.str();
    case 0xE9: return "JP HL";
    case 0xEA: ss << "LD [$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)) << "],A"; return ss.str();
    case 0xEB: return "INVALID";
    case 0xEC: return "INVALID";
    case 0xED: return "INVALID";
    case 0xEE: ss << "XOR $" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0xEF: return "RST $28";

    case 0xF0: ss << "LDH A,[$ff" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1) << "]"; return ss.str();
    case 0xF1: return "POP AF";
    case 0xF2: return "LDH A,[C]";
    case 0xF3: return "DI";
    case 0xF4: return "INVALID";
    case 0xF5: return "PUSH AF";
    case 0xF6: ss << "OR $" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0xF7: return "RST $30";
    case 0xF8: ss << "LD HL,SP+$" << std::hex << std::setw(2) << std::setfill('0') << (int)(int8_t)read8(addr + 1); return ss.str();
    case 0xF9: return "LD SP,HL";
    case 0xFA: ss << "LD A,[$" << std::hex << std::setw(4) << std::setfill('0') << (uint16_t)(read8(addr + 2) << 8 | read8(addr + 1)) << "]"; return ss.str();
    case 0xFB: return "EI";
    case 0xFC: return "INVALID";
    case 0xFD: return "INVALID";
    case 0xFE: ss << "CP $" << std::hex << std::setw(2) << std::setfill('0') << (int)read8(addr + 1); return ss.str();
    case 0xFF: return "RST $38";
    }

    UNREACHABLE();
}