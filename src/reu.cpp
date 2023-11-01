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

#include "reu.h"

REU::REU()
{
        n_banks=0;
        for(int i=0; i<16; ++i)
        {
                banks[i]=nullptr;
        }
}
REU::~REU()
{
        init(0);
}
void REU::init(int _banks)
{
    n_banks = _banks & 15;
    for (int i=0; i<_banks; ++i)
    {
		if (banks[i]==nullptr)
		{
        	banks[i] = new uint8_t[32*1024];
		}
    }
    for (int i=_banks; i<16; ++i)
    {
		if (banks[i]!=nullptr)
		{
			delete [] banks[i];
			banks[i] = nullptr;
		}
    }

}
uint8_t REU::read(uint8_t _addr)
{
    return 0;
}

void REU::write(uint8_t _addr, uint8_t _value)
{

}

void REU::reset()
{

}

void REU::clock()
{

}

void REU::latch(uint8_t _value)
{

}
