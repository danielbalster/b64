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

#include "io.h"
#include <Arduino.h>
#include <Wire.h>

static bool twosids = false;

const static inline void set_rw_high()
{
    GPIO.out1_w1ts.val = ((uint32_t)1 << (SID_RW - 32));
}
const static inline void set_rw_low()
{
    GPIO.out1_w1tc.val = ((uint32_t)1 << (SID_RW - 32));
}

const static inline void set_cs_high(bool d400=false)
{
    if (twosids)
    {
        GPIO.out_w1ts =  ((uint32_t)1 << (SID_CS2));
        GPIO.out1_w1ts.val = ((uint32_t)1 << (SID_CS - 32));
    }
    else
    {
        if (d400)
            GPIO.out_w1ts =  ((uint32_t)1 << (SID_CS2));
        else
            GPIO.out1_w1ts.val = ((uint32_t)1 << (SID_CS - 32));
    }
}

const static inline void set_cs_low(bool d400=false)
{
    if (twosids)
    {
        GPIO.out_w1tc =  ((uint32_t)1 << (SID_CS2));
        GPIO.out1_w1tc.val = ((uint32_t)1 << (SID_CS - 32));
    }
    else
    {
        if (d400)
            GPIO.out_w1tc =  ((uint32_t)1 << (SID_CS2));
        else
            GPIO.out1_w1tc.val = ((uint32_t)1 << (SID_CS - 32));
    }
}

void readwrite(bool dir)
{
    static bool lastdir = true;
    if (lastdir == dir) return;
    lastdir = dir;
    if (dir)
    {
        pinMode(SID_D0,OUTPUT);
        pinMode(SID_D1,OUTPUT);
        pinMode(SID_D2,OUTPUT);
        pinMode(SID_D3,OUTPUT);
        pinMode(SID_D4,OUTPUT);
        pinMode(SID_D5,OUTPUT);
        pinMode(SID_D6,OUTPUT);
        pinMode(SID_D7,OUTPUT);
    }
    else
    {
        pinMode(SID_D0,INPUT_PULLDOWN);
        pinMode(SID_D1,INPUT_PULLDOWN);
        pinMode(SID_D2,INPUT_PULLDOWN);
        pinMode(SID_D3,INPUT_PULLDOWN);
        pinMode(SID_D4,INPUT_PULLDOWN);
        pinMode(SID_D5,INPUT_PULLDOWN);
        pinMode(SID_D6,INPUT_PULLDOWN);
        pinMode(SID_D7,INPUT_PULLDOWN);
    }
}

uint8_t IO::read(uint8_t adr)
{
    reads++;
    uint8_t value = 0;

    readwrite(false);

    digitalWrite(SID_A0,adr & 1);
    digitalWrite(SID_A1,adr & 2);
    digitalWrite(SID_A2,adr & 4);
    digitalWrite(SID_A3,adr & 8);
    digitalWrite(SID_A4,adr & 16);

    set_rw_high();
    set_cs_low(adr & 128);
    delayMicroseconds(1);
    uint32_t bits = GPIO.in;
    set_cs_high(adr & 128);

    value |= ((bits >> SID_D7)&1) << 7;
    value |= ((bits >> SID_D6)&1) << 6;
    value |= ((bits >> SID_D5)&1) << 5;
    value |= ((bits >> SID_D4)&1) << 4;
    value |= ((bits >> SID_D3)&1) << 3;
    value |= ((bits >> SID_D2)&1) << 2;
    value |= ((bits >> SID_D1)&1) << 1;
    value |= ((bits >> SID_D0)&1) << 0;
    return value;
}

void IO::write(uint8_t reg, uint8_t value)
{
    readwrite(true);
    set_rw_low();

    uint32_t bits[2];
    bits[0] = 0;
    bits[1] = 0;

    bits[value & 1] |= 1 << SID_D0;
    bits[(value>>1) & 1] |= 1 << SID_D1;
    bits[(value>>2) & 1] |= 1 << SID_D2;
    bits[(value>>3) & 1] |= 1 << SID_D3;
    bits[(value>>4) & 1] |= 1 << SID_D4;
    bits[(value>>5) & 1] |= 1 << SID_D5;
    bits[(value>>6) & 1] |= 1 << SID_D6;
    bits[(value>>7) & 1] |= 1 << SID_D7;

    bits[(reg>>0) & 1] |= 1 << SID_A0;
    bits[(reg>>1) & 1] |= 1 << SID_A1;
    bits[(reg>>2) & 1] |= 1 << SID_A2;
    bits[(reg>>3) & 1] |= 1 << SID_A3;
    bits[(reg>>4) & 1] |= 1 << SID_A4;

    GPIO.out_w1tc = bits[0];
    GPIO.out_w1ts = bits[1];

    set_cs_low(reg & 128);
    delayMicroseconds(1);
    set_cs_high(reg & 128);
    writes++;
}

void IO::init()
{
    pinMode(SID_D0,OUTPUT);
    pinMode(SID_D1,OUTPUT);
    pinMode(SID_D2,OUTPUT);
    pinMode(SID_D3,OUTPUT);
    pinMode(SID_D4,OUTPUT);
    pinMode(SID_D5,OUTPUT);
    pinMode(SID_D6,OUTPUT);
    pinMode(SID_D7,OUTPUT);
    pinMode(SID_A0,OUTPUT);
    pinMode(SID_A1,OUTPUT);
    pinMode(SID_A2,OUTPUT);
    pinMode(SID_A3,OUTPUT);
    pinMode(SID_A4,OUTPUT);
    pinMode(SID_CS,OUTPUT);
    pinMode(SID_CS2,OUTPUT);
    pinMode(SID_RW,OUTPUT);
}

IO io;