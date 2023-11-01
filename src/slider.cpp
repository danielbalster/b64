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

#include "slider.h"
#include "textmatrix.h"

void Slider::change(int d)
{
    value = d;
    if (changed != nullptr) changed(this);
}
bool Slider::input(int c)
{
    switch(c)
    {
        case VK_HOME:
            change(min);
            return true;
        case VK_END:
            change(max);
            return true;
        case '0':
            change(0);
            return true;
        case '+':
        case VK_RIGHT:
            if (value<max) change(value+1);
            return true;
        case '-':
        case VK_LEFT:
            if (value>min) change(value-1);
            return true;
    }
    return false;
}
void Slider::output(TextMatrix* tm)
{
    tm->setColor(hasFocus() ? YELLOW: WHITE );
    tm->setCursor(x,y);
    for (int i=0; i<(max-min)+1; ++i)
        tm->print("-");
    tm->printf(" %s",label);
    tm->setCursor(x+value-min,y);
    tm->print("*");
}

