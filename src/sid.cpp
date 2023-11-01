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

#include "sid.h"
#include "io.h"

#include "driver/ledc.h"
#include "driver/periph_ctrl.h"

HardSID::HardSID(uint8_t id)
{
    mask = id ? 0x80 : 0x00;
}

void HardSID::send_cmd(char x31, char x30)
{
    write(31,x31);
    write(30,x30);
}

void HardSID::send_cmd_wait(char x31, char x30)
{
    send_cmd(x31,x30);
    delay(10);
}

char HardSID::get_pcmd(char x31, char x30)
{
    send_cmd(x31,x30);
    delay(1);
    return get_p();
}

void HardSID::sid_off(void)
{
    write(29,0);
    write(29,0);
    write(29,0);
}

void HardSID::sidoffon(void)
{
    sid_off();
    delay(10);
    send_cmd('D','I');
    write(29,'S');
    delay(1);
}

char HardSID::get_p(void)
{
    return read(27);
}

char HardSID::get_q(void)
{
    return read(28);
}

int HardSID::detect()
{
    int count = 0;
    // silent
    write(24,0);
    write(24,0);
    write(24,0);
    sidoffon();
    uint8_t p,q;
    p = get_p();
    q = get_q();
    if (p=='N' && q=='O')
    {
        p = get_pcmd('E','I');
        q = get_q();
        if (p=='S' && q=='W')
        {
        count++;
        }
    }
    return count;
}

void HardSID::applyFilterSettings()
{
    sidoffon();
    send_cmd_wait(0x80|(1<<4)|(filterStrength6581 & 15),'E');
    send_cmd_wait(0x80|(0<<4)|(lowestFilterFrequency6581 & 15),'E');
    send_cmd_wait(0x80|(3<<4)|(centralFilterFrequency8580 & 15),'E');
    send_cmd_wait(0x80|(2<<4)|(lowestFilterFrequency8580 & 15),'E');
    send_cmd(0xC0,'E');
    delay(1000);
    sid_off();
}

void HardSID::readFilterSettings()
{
    uint8_t p,q;
    p = get_pcmd('H','I');
    q = get_q();
    filterStrength6581 = (p>>4) & 15;
    lowestFilterFrequency6581 = (p   ) & 15;
    centralFilterFrequency8580 = (q>>4) & 15;
    lowestFilterFrequency8580 = (q   ) & 15;
}

void HardSID::saveToRAM()
{
    send_cmd(0xC0,'E');
    delay(1000);
}

void HardSID::saveToFlash()
{
    send_cmd(0xCF,'E');
    delay(2000);
}

void HardSID::setChipType(char type)
{
    model = type;
    sidoffon();
    send_cmd_wait(type,'E');    
    sid_off();
}

void HardSID::setPAL()
{
    // frequency is 50.125 * 504 * 312 / 8 = 985257
    frequency = 985257;
    ledcSetup(0, 985257,1);
    ledcAttachPin(SID_CLK, 0);
    ledcWrite(0, 1);
}

void HardSID::setNTSC()
{
    frequency = 1022727;
    ledcSetup(0, 1022727 ,1);
    ledcAttachPin(SID_CLK, 0);
    ledcWrite(0, 1);
}

void HardSID::setup()
{
    setNTSC();
    setPAL();
    connected = true;
}

void HardSID::clock()
{
}

void HardSID::reset()
{
    setup();
    setChipType('8');
    mute[0] = false;
    mute[1] = false;
    mute[2] = false;
    for (int i=0; i<0x19; ++i) write(i,0);
}

uint8_t HardSID::read(uint8_t adr)
{
    return io.read(adr);
}

void HardSID::write(uint8_t adr, uint8_t val)
{
    // RES,RW,CS,A4...A0,D7...D0
    // RES=1 = normal, 1=reset
    // RW: 0=write 1=read
    // CS: 1=tristate 0=ok
    adr &= 31;
    regs[adr]=val;
    if (mute[0] && adr<7) val=0;
    if (mute[1] && adr>=7 && adr<14) val=0;
    if (mute[2] && adr>=14 && adr<21) val=0;
    // D400
    // D500
    // 
    io.write((adr)|mask,val);
}

HardSID sid(0), sid2(1);