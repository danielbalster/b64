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

#ifndef SCANNER_H
#define SCANNER_H

#include <Arduino.h>

enum Adressmodes
{
    IMP, INDX, ZP, IMM, ACC, ABSO, REL, INDY, ZPX, ZPY, ABSX, ABSY, IND
};

enum AdressmodeBits
{
    AM_IMP=1<<IMP,       // implied, NOP
    AM_ZP=1<<ZP,        // zeropage, LDA $xx
    AM_ZPX=1<<ZPX,     // LDX $10,X
    AM_ZPY=1<<ZPY,     // LDY $10,Y
    AM_IMM=1<<IMM,       // immediate, LDA #$xx
    AM_ACC=1<<ACC,      // TXA 
    AM_ABSO=1<<ABSO,     // LDA $xxxx
    AM_REL=1<<REL,      // BNE $xx
    AM_INDX=1<<INDX,      // LDA ($LL,X)
    AM_INDY=1<<INDY,    // LDA ($XX),Y
    AM_ABSX=1<<ABSX,   // LDA $xxxx,X
    AM_ABSY=1<<ABSY,   // LDA $xxxx,Y
    AM_IND=1<<IND     // LDA ($1000)
};

enum Opcodes {
    OPCODE_ADC,OPCODE_AND,OPCODE_ASL,OPCODE_BCC,OPCODE_BCS,OPCODE_BEQ,OPCODE_BIT,OPCODE_BMI,
    OPCODE_BNE,OPCODE_BPL,OPCODE_BRK,OPCODE_BVC,OPCODE_BVS,OPCODE_CLC,OPCODE_CLD,OPCODE_CLI,
    OPCODE_CLV,OPCODE_CMP,OPCODE_CPX,OPCODE_CPY,OPCODE_DCP,OPCODE_DEC,OPCODE_DEX,OPCODE_DEY,
    OPCODE_EOR,OPCODE_INC,OPCODE_INX,OPCODE_INY,OPCODE_ISB,OPCODE_JMP,OPCODE_JSR,OPCODE_LAX,
    OPCODE_LDA,OPCODE_LDX,OPCODE_LDY,OPCODE_LSR,OPCODE_NOP,OPCODE_ORA,OPCODE_PHA,OPCODE_PHP,
    OPCODE_PLA,OPCODE_PLP,OPCODE_RLA,OPCODE_ROL,OPCODE_ROR,OPCODE_RRA,OPCODE_RTI,OPCODE_RTS,
    OPCODE_SAX,OPCODE_SBC,OPCODE_SEC,OPCODE_SED,OPCODE_SEI,OPCODE_SLO,OPCODE_SRE,OPCODE_STA,
    OPCODE_STX,OPCODE_STY,OPCODE_TAX,OPCODE_TAY,OPCODE_TSX,OPCODE_TXA,OPCODE_TXS,OPCODE_TYA,
    OPCODE_COUNT,
    OPCODE_INVALID=-1,
};


extern const uint8_t BytesPerAdressMode[13];
extern const Adressmodes AddressmodeTable[256];
extern const Opcodes OpcodeTable[256];

const char* OpcodeToString(Opcodes opcode);
uint8_t GetInstructionByte(Opcodes opcode, Adressmodes am);

class AddressmodeCompiler
{
    uint16_t table[OPCODE_COUNT];
public:
    AddressmodeCompiler()
    {
        for (int i=0; i<OPCODE_COUNT; ++i)
        {
            table[i] = 0;
        }

        for (int i=0; i<256; ++i)
        {
            table[ OpcodeTable[i] ] |=  1 << AddressmodeTable[i];
        }
    }
    uint16_t operator[](int index) const
    {
        return table[index];
    }
};

extern AddressmodeCompiler AddressModesPerOpcode;

class Scanner
{
    enum NumberBase { Binary, Octal, Decimal, Hexadecimal };
    bool scanOperand(const char* _line, int _limit);
    bool encode(Adressmodes am);
public:
    bool scan(const char* _line);
    uint16_t write(uint16_t addr,bool _output = true);

    Opcodes opcode;
    Adressmodes addressmode;
    uint16_t operand;
    uint8_t instructionbyte;
    char lastScannedChar;
};

#endif
