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

#ifndef IO_H
#define IO_H

#include <stdint.h>

#define SID_D0 23
#define SID_D1 22
#define SID_D2 21
#define SID_D3 19
#define SID_D4 18
#define SID_D5 5
#define SID_D6 17
#define SID_D7 16
#define SID_A0 4
#define SID_A1 12
#define SID_A2 27
#define SID_A3 26
#define SID_A4 25

#define SID_RW 32
#define SID_CS 33
#define SID_CS2 13
#define SID_CLK 0

// 1 bit SD MMC protocol
#define SD_MOSI 15
#define SD_SCK 14
#define SD_MISO 2

// the first version was running with several hw busses

class IO
{
public:
    uint16_t reads;
    uint16_t writes;

    void init();
    void write(uint8_t reg, uint8_t value);
    uint8_t read(uint8_t adr);
    void enable(int bits);
    void disable(int bits);
};

extern IO io;

#endif
