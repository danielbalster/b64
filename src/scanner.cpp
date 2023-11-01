/*
 * Balster64, hacking a C64 emulator into an ESP32 microcontroller
 *
 * Copyright (C) Daniel Balster
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Daniel Balster nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL BALSTER ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL DANIEL BALSTER BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Arduino.h>

#include "scanner.h"
#include "cpu.h"

AddressmodeCompiler AddressModesPerOpcode;

const uint8_t BytesPerAdressMode[13] = {1,2,2,2,1,3,2,2,2,2,3,3,3};

const Adressmodes AddressmodeTable[256] = {
    IMP,INDX,IMP,INDX,ZP,ZP,ZP,ZP,IMP,IMM,ACC,IMM,ABSO,ABSO,ABSO,ABSO,          // 00
    REL,INDY,IMP,INDY,ZPX,ZPX,ZPX,ZPX,IMP,ABSY,IMP,ABSY,ABSX,ABSX,ABSX,ABSX,    // 10
    ABSO,INDX,IMP,INDX,ZP,ZP,ZP,ZP,IMP,IMM,ACC,IMM,ABSO,ABSO,ABSO,ABSO,         // 20
    REL,INDY,IMP,INDY,ZPX,ZPX,ZPX,ZPX,IMP,ABSY,IMP,ABSY,ABSX,ABSX,ABSX,ABSX,    // 30
    IMP,INDX,IMP,INDX,ZP,ZP,ZP,ZP,IMP,IMM,ACC,IMM,ABSO,ABSO,ABSO,ABSO,          // 40
    REL,INDY,IMP,INDY,ZPX,ZPX,ZPX,ZPX,IMP,ABSY,IMP,ABSY,ABSX,ABSX,ABSX,ABSX,    // 50
    IMP,INDX,IMP,INDX,ZP,ZP,ZP,ZP,IMP,IMM,ACC,IMM,IND,ABSO,ABSO,ABSO,           // 60
    REL,INDY,IMP,INDY,ZPX,ZPX,ZPX,ZPX,IMP,ABSY,IMP,ABSY,ABSX,ABSX,ABSX,ABSX,    // 70
    IMM,INDX,IMM,INDX,ZP,ZP,ZP,ZP,IMP,IMM,IMP,IMM,ABSO,ABSO,ABSO,ABSO,          // 80
    REL,INDY,IMP,INDY,ZPX,ZPX,ZPY,ZPY,IMP,ABSY,IMP,ABSY,ABSX,ABSX,ABSY,ABSY,    // 90
    IMM,INDX,IMM,INDX,ZP,ZP,ZP,ZP,IMP,IMM,IMP,IMM,ABSO,ABSO,ABSO,ABSO,          // a0
    REL,INDY,IMP,INDY,ZPX,ZPX,ZPY,ZPY,IMP,ABSY,IMP,ABSY,ABSX,ABSX,ABSY,ABSY,    // b0
    IMM,INDX,IMM,INDX,ZP,ZP,ZP,ZP,IMP,IMM,IMP,IMM,ABSO,ABSO,ABSO,ABSO,          // c0
    REL,INDY,IMP,INDY,ZPX,ZPX,ZPX,ZPX,IMP,ABSY,IMP,ABSY,ABSX,ABSX,ABSX,ABSX,    // d0
    IMM,INDX,IMM,INDX,ZP,ZP,ZP,ZP,IMP,IMM,IMP,IMM,ABSO,ABSO,ABSO,ABSO,          // e0
    REL,INDY,IMP,INDY,ZPX,ZPX,ZPX,ZPX,IMP,ABSY,IMP,ABSY,ABSX,ABSX,ABSX,ABSX     // f0
};

// lookup actual instruction byte to opcode class
const Opcodes OpcodeTable[256] = {
    OPCODE_BRK,OPCODE_ORA,OPCODE_NOP,OPCODE_SLO,OPCODE_NOP,OPCODE_ORA,OPCODE_ASL,OPCODE_SLO,    OPCODE_PHP,OPCODE_ORA,OPCODE_ASL,OPCODE_NOP,OPCODE_NOP,OPCODE_ORA,OPCODE_ASL,OPCODE_SLO,
    OPCODE_BPL,OPCODE_ORA,OPCODE_NOP,OPCODE_SLO,OPCODE_NOP,OPCODE_ORA,OPCODE_ASL,OPCODE_SLO,    OPCODE_CLC,OPCODE_ORA,OPCODE_NOP,OPCODE_SLO,OPCODE_NOP,OPCODE_ORA,OPCODE_ASL,OPCODE_SLO,
    OPCODE_JSR,OPCODE_AND,OPCODE_NOP,OPCODE_RLA,OPCODE_BIT,OPCODE_AND,OPCODE_ROL,OPCODE_RLA,    OPCODE_PLP,OPCODE_AND,OPCODE_ROL,OPCODE_NOP,OPCODE_BIT,OPCODE_AND,OPCODE_ROL,OPCODE_RLA,
    OPCODE_BMI,OPCODE_AND,OPCODE_NOP,OPCODE_RLA,OPCODE_NOP,OPCODE_AND,OPCODE_ROL,OPCODE_RLA,    OPCODE_SEC,OPCODE_AND,OPCODE_NOP,OPCODE_RLA,OPCODE_NOP,OPCODE_AND,OPCODE_ROL,OPCODE_RLA,
    OPCODE_RTI,OPCODE_EOR,OPCODE_NOP,OPCODE_SRE,OPCODE_NOP,OPCODE_EOR,OPCODE_LSR,OPCODE_SRE,    OPCODE_PHA,OPCODE_EOR,OPCODE_LSR,OPCODE_NOP,OPCODE_JMP,OPCODE_EOR,OPCODE_LSR,OPCODE_SRE,
    OPCODE_BVC,OPCODE_EOR,OPCODE_NOP,OPCODE_SRE,OPCODE_NOP,OPCODE_EOR,OPCODE_LSR,OPCODE_SRE,    OPCODE_CLI,OPCODE_EOR,OPCODE_NOP,OPCODE_SRE,OPCODE_NOP,OPCODE_EOR,OPCODE_LSR,OPCODE_SRE,
    OPCODE_RTS,OPCODE_ADC,OPCODE_NOP,OPCODE_RRA,OPCODE_NOP,OPCODE_ADC,OPCODE_ROR,OPCODE_RRA,    OPCODE_PLA,OPCODE_ADC,OPCODE_ROR,OPCODE_NOP,OPCODE_JMP,OPCODE_ADC,OPCODE_ROR,OPCODE_RRA,
    OPCODE_BVS,OPCODE_ADC,OPCODE_NOP,OPCODE_RRA,OPCODE_NOP,OPCODE_ADC,OPCODE_ROR,OPCODE_RRA,    OPCODE_SEI,OPCODE_ADC,OPCODE_NOP,OPCODE_RRA,OPCODE_NOP,OPCODE_ADC,OPCODE_ROR,OPCODE_RRA,
    OPCODE_NOP,OPCODE_STA,OPCODE_NOP,OPCODE_SAX,OPCODE_STY,OPCODE_STA,OPCODE_STX,OPCODE_SAX,    OPCODE_DEY,OPCODE_NOP,OPCODE_TXA,OPCODE_NOP,OPCODE_STY,OPCODE_STA,OPCODE_STX,OPCODE_SAX,
    OPCODE_BCC,OPCODE_STA,OPCODE_NOP,OPCODE_NOP,OPCODE_STY,OPCODE_STA,OPCODE_STX,OPCODE_SAX,    OPCODE_TYA,OPCODE_STA,OPCODE_TXS,OPCODE_NOP,OPCODE_NOP,OPCODE_STA,OPCODE_NOP,OPCODE_NOP,
    OPCODE_LDY,OPCODE_LDA,OPCODE_LDX,OPCODE_LAX,OPCODE_LDY,OPCODE_LDA,OPCODE_LDX,OPCODE_LAX,    OPCODE_TAY,OPCODE_LDA,OPCODE_TAX,OPCODE_NOP,OPCODE_LDY,OPCODE_LDA,OPCODE_LDX,OPCODE_LAX,
    OPCODE_BCS,OPCODE_LDA,OPCODE_NOP,OPCODE_LAX,OPCODE_LDY,OPCODE_LDA,OPCODE_LDX,OPCODE_LAX,    OPCODE_CLV,OPCODE_LDA,OPCODE_TSX,OPCODE_LAX,OPCODE_LDY,OPCODE_LDA,OPCODE_LDX,OPCODE_LAX,
    OPCODE_CPY,OPCODE_CMP,OPCODE_NOP,OPCODE_DCP,OPCODE_CPY,OPCODE_CMP,OPCODE_DEC,OPCODE_DCP,    OPCODE_INY,OPCODE_CMP,OPCODE_DEX,OPCODE_NOP,OPCODE_CPY,OPCODE_CMP,OPCODE_DEC,OPCODE_DCP,
    OPCODE_BNE,OPCODE_CMP,OPCODE_NOP,OPCODE_DCP,OPCODE_NOP,OPCODE_CMP,OPCODE_DEC,OPCODE_DCP,    OPCODE_CLD,OPCODE_CMP,OPCODE_NOP,OPCODE_DCP,OPCODE_NOP,OPCODE_CMP,OPCODE_DEC,OPCODE_DCP,
    OPCODE_CPX,OPCODE_SBC,OPCODE_NOP,OPCODE_ISB,OPCODE_CPX,OPCODE_SBC,OPCODE_INC,OPCODE_ISB,    OPCODE_INX,OPCODE_SBC,OPCODE_NOP,OPCODE_SBC,OPCODE_CPX,OPCODE_SBC,OPCODE_INC,OPCODE_ISB,
    OPCODE_BEQ,OPCODE_SBC,OPCODE_NOP,OPCODE_ISB,OPCODE_NOP,OPCODE_SBC,OPCODE_INC,OPCODE_ISB,    OPCODE_SED,OPCODE_SBC,OPCODE_NOP,OPCODE_ISB,OPCODE_NOP,OPCODE_SBC,OPCODE_INC,OPCODE_ISB
};


const char* OpcodeToString(Opcodes opcode)
{
    // instead of having 64 32-bit pointers
    static const char* const names = "ADC\0AND\0ASL\0BCC\0BCS\0BEQ\0BIT\0BMI\0BNE\0BPL\0BRK\0BVC\0BVS\0CLC\0CLD\0CLI\0CLV\0CMP\0CPX\0CPY\0DCP\0DEC\0DEX\0DEY\0EOR\0INC\0INX\0INY\0ISB\0JMP\0JSR\0LAX\0LDA\0LDX\0LDY\0LSR\0NOP\0ORA\0PHA\0PHP\0PLA\0PLP\0RLA\0ROL\0ROR\0RRA\0RTI\0RTS\0SAX\0SBC\0SEC\0SED\0SEI\0SLO\0SRE\0STA\0STX\0STY\0TAX\0TAY\0TSX\0TXA\0TXS\0TYA";
    return names + (opcode * 4);
}

uint8_t GetInstructionByte(Opcodes opcode, Adressmodes am)
{
    for (uint8_t i=0; i<256; ++i)
    {
        if (OpcodeTable[i]==opcode && AddressmodeTable[i]==am) return i;
    }
    return 0x00;
}

// TODO: use a Stream instead of "poke"

uint16_t Scanner::write(uint16_t addr, bool _output)
{
    int n = BytesPerAdressMode[addressmode];

    if (_output)
        poke(addr,instructionbyte);

    uint16_t value = operand;
    if (addressmode == REL)
    {
        value = (operand - addr) - 2;
    }

    if (_output)
    {
        if (n>1) poke(addr+1,value&255);
        if (n>2) poke(addr+2,(value>>8)&255);
    }

    return addr+n;
}

bool Scanner::scanOperand(const char* _line, int _limit)
{
    NumberBase base = Decimal;
    operand = 0;
    int digit = 0;

    for (int i=0; true; ++i)
    {
        lastScannedChar = _line[i];
        if (lastScannedChar==0) break;
        if (lastScannedChar==')') break; 
        if (lastScannedChar==',') break;
        if (lastScannedChar=='$')
        {
            if (i!=0) return false;
            base = Hexadecimal;
        }
        else if (lastScannedChar=='%')
        {
            if (i!=0) return false;
            base = Binary;
        }
        else
        {
            if (base==Hexadecimal && !isxdigit(lastScannedChar)) return false;
            if (base==Binary && !(lastScannedChar=='0' || lastScannedChar=='1')) return false;
            if (base==Decimal && !isdigit(lastScannedChar)) return false;
        }
    }
    unsigned long value = 0;
    switch(base)
    {
        case Decimal:
            value = strtoul(_line,nullptr,10);
            break;
        case Binary:
            for (int i=1; true; ++i)
            {
                lastScannedChar = _line[i];
                if (lastScannedChar==0) break;
                if (lastScannedChar==')') break; 
                if (lastScannedChar==',') break;
                value <<= 1;
                value |= (lastScannedChar=='1') ? 1 : 0;
            }
            break;
        case Hexadecimal:
            value = strtoul(_line+1,nullptr,16);
            break;
        default:
            return false;
    }

    if (value > _limit) return false;


    operand = value;
    return true;
}

bool Scanner::encode(Adressmodes am)
{
    addressmode = am;
    instructionbyte = GetInstructionByte(opcode,addressmode);
    if (instructionbyte==0 && opcode!=OPCODE_BRK) return false;
    return true;
}

/*
    Bugs:
    - Branches
    - first load 1-byte (zeropage, etc) then try 2-byte

    LDX $1000,X
*/

bool Scanner::scan(const char* _line)
{
    int len = strlen(_line);
    opcode=OPCODE_INVALID;

    for (int i=0; i<OPCODE_COUNT; ++i)
    {
        if (strncmp(_line,OpcodeToString((Opcodes)i),3)==0)
        {
            opcode = (Opcodes) i;
            break;
        }
    }
    if (opcode==OPCODE_INVALID) return false;

    uint16_t adressmodes = AddressModesPerOpcode[opcode];

    // check opcode has param
    if (len==3 && _line[3]==0 && (adressmodes & AM_IMP))
    {
        return encode(IMP);
    }

    if (len==3 && _line[3]==0 && (adressmodes & AM_ACC))
    {
        return encode(ACC);
    }

    if (len>3 && _line[3]!=' ') return false;
    if (len<5) return false;

    if (_line[4]=='#' && scanOperand(_line+5,0xff) && lastScannedChar==0)
    {
        return encode(IMM);
    }

    if ((adressmodes & AM_REL) && scanOperand(_line+4,0xffff) && lastScannedChar==0)
    {
        return encode(REL);
    }

    // OPC ($XX),Y
    if (_line[4]=='(' && (adressmodes & AM_INDY) && scanOperand(_line+5,0xff) && lastScannedChar==')' && _line[len-2]==',' && _line[len-1]=='Y')
    {
        return encode(INDY);
    }

    // OPC ($XX,X)
    if (_line[4]=='(' && (adressmodes & AM_INDX) && scanOperand(_line+5,0xff) && lastScannedChar==',' && _line[len-2]=='X' && _line[len-1]==')')
    {
        return encode(INDX);
    }

    if (_line[4]=='(' && (adressmodes & AM_IND) && scanOperand(_line+5,0xffff) && lastScannedChar==')')
    {
        return encode(IND);
    }

    if (((adressmodes & AM_ZPX) || (adressmodes & AM_ZPY)) && scanOperand(_line+4,0xff) && lastScannedChar==',')
    {
        if ((_line[len-1]=='X') && (adressmodes & AM_ZPX))
        {
            return encode(ZPX);
        }
        if ((_line[len-1]=='Y') && (adressmodes & AM_ZPY))
        {
            return encode(ZPY);
        }
    }

    if ((adressmodes & AM_ZP) && scanOperand(_line+4,0xff) && lastScannedChar==0)
    {
        return encode(ZP);
    }

    if (((adressmodes & AM_ABSX) || (adressmodes & AM_ABSY)) && scanOperand(_line+4,0xffff) && lastScannedChar==',')
    {
        if ((_line[len-1]=='X') && (adressmodes & AM_ABSX))
        {
            return encode(ABSX);
        }
        if ((_line[len-1]=='Y') && (adressmodes & AM_ABSY))
        {
            return encode(ABSY);
        }
    }

    if ((adressmodes & AM_ABSO) && scanOperand(_line+4,0xffff) && lastScannedChar==0)
    {
        return encode(ABSO);
    }

    return false;
}
