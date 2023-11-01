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

#ifndef SID_H
#define SID_H

#include <Arduino.h>
#include "chip.h"

class HardSID : public Chip
{
    //constexpr static uint8_t cs = 26;
    bool connected;

    void send_cmd(char x31, char x30);
    void send_cmd_wait(char x31, char x30);
    char get_pcmd(char x31, char x30);
    void sid_off(void);
    void sidoffon(void);
    char get_p(void);
    char get_q(void);
    int detect();

    uint8_t mask;
public:
    HardSID(uint8_t id);

    uint8_t regs[64];
    int frequency;
  // follin galway average strong extreme
    uint8_t filterStrength6581;
    uint8_t lowestFilterFrequency6581;
    uint8_t centralFilterFrequency8580;
    uint8_t lowestFilterFrequency8580;
    char model;
    bool mute[3];

    void applyFilterSettings();
    void readFilterSettings();
    void saveToRAM();
    void saveToFlash();
    void setChipType(char type);
    void setPAL();
    void setNTSC();
    virtual void setup();
    virtual void clock();
    virtual void reset();
    virtual uint8_t read(uint8_t adr);
    virtual void write(uint8_t adr, uint8_t val);
};

extern HardSID sid, sid2;

#endif
