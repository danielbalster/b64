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

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "textmatrix.h"
#include "memory.h"
#include "vic.h"

void refreshscreen(AnsiRenderer *_ansi, bool _use_ansi)
{
    bool uppercase = (vic.regs[0x18]&2) != 2;
    if (_ansi->uppercase != uppercase)
    {
        _ansi->uppercase = uppercase;
    }
  if (_use_ansi)
  {
    AnsiRenderer::hideCursor();
    AnsiRenderer::clearScreen();
    AnsiRenderer::setCursor(1,1);
    uint8_t color = 255;
    AnsiRenderer::setBackColor(vic.read(0x21)&15);
    for (int i=0; i<40*25; ++i)
    {
      if (color != CRAM[i])
      {
        color = CRAM[i];
        AnsiRenderer::setTextColor(color);
      }
      
      char buf[6];
      utf8_encode(buf,0xe000+delayed_matrix[i]);
      Serial.print(buf);

    }
    AnsiRenderer::setCursor(RAM[211],RAM[214]+1);
    AnsiRenderer::showCursor();
  }
  else
  {
    AnsiRenderer::hideCursor();
    _ansi->invalidate(delayed_matrix,CRAM);
    _ansi->sync(delayed_matrix,CRAM,vic.read(0x21));
  }
}


class Screen2PetsciiConverter
{
  uint8_t table[256];
public:
  Screen2PetsciiConverter()
  {
    for (int i=0x00; i<0x20; ++i) table[i] = i+64;
    for (int i=0x20; i<0x40; ++i) table[i] = i;
    for (int i=0x40; i<0x60; ++i) table[i] = i+128;
    for (int i=0x60; i<0x80; ++i) table[i] = i+64;
    for (int i=0x80; i<0xA0; ++i) table[i] = i-128;
    for (int i=0xA0; i<0xC0; ++i) table[i] = i-64; // ??
    for (int i=0xC0; i<0xE0; ++i) table[i] = i-64;
    for (int i=0xE0; i<0xFF; ++i) table[i] = i;
  }
  uint8_t operator[](uint8_t c)
  {
    return table[c];
  }
  uint16_t unicode(uint8_t c)
  {
    return 0xe000+table[c];
  }
};

Screen2PetsciiConverter Screen2Petscii;

void TextMatrix::set(int _x, int _y, uint8_t code)
{
    int i = ((_y-1)*40)+(_x-1);
    if (i<0 || i>999) return;
    text[i]=code | reverse;
    cram[i]=color;
}

uint8_t TextMatrix::get(int _x, int _y) const
{
    int i = ((_y-1)*40)+(_x-1);
    if (i<0 || i>999) return 0;
    return text[i];
}

void TextMatrix::hline(int x1, int x2, int _y, uint8_t _code)
{
    for (int _x=x1; _x<=x2; ++_x) set(_x,_y,_code);
}

void TextMatrix::vline(int y1, int y2, int _x, uint8_t _code)
{
    for (int _y=y1; _y<=y2; ++_y) set(_x,_y,_code);
}

void TextMatrix::frame(int x1, int y1, int w, int h, bool round)
{
    hline(x1+1,x1+w-2,y1);
    hline(x1+1,x1+w-2,y1+h-1);
    
    vline(y1+1,y1+h-2,x1);
    vline(y1+1,y1+h-2,x1+w-1);

    set(x1,y1,round?0x55:0x70);
    set(x1+w-1,y1,round?0x49:0x6e);
    set(x1,y1+h-1,round?0x4a:0x6d);
    set(x1+w-1,y1+h-1,round?0x4b:0x7d);
}

void TextMatrix::tab(int x1, int y1, int w, int h, const char* _header, bool _active)
{
    setColor(_active?GRAY3:GRAY1);

    hline(x1+1,x1+w-2,y1);
    
    vline(y1+1,y1+h-2,x1);
    vline(y1+1,y1+h-2,x1+w-1);

    set(x1,y1,0x55);
    set(x1+w-1,y1,0x49);

    if (_active)
    {
        hline(x1+1,x1+w-2,y1+h-1,0x20);
        set(x1,y1+h-1,x1==1?LINEV:0x4b);
        set(x1+w-1,y1+h-1,0x4a);
    }

    setColor(_active?WHITE:GRAY1);
    setCursor(x1+1,y1+1);
    print(_header);
}

void TextMatrix::progress(int _x, int _y, int _w, int _v)
{
    int p = (_y*40)+_x;
    for (int i=0; i<_w; ++i)
    {
        float t = ((i+0.5f)/float(_w))*100.0f;
        text[p]=0x66;
        cram[p]=(i<_v) ? LIGHTBLUE : GRAY3;
        p++;
    }
}

void TextMatrix::fill(int x1, int y1, int w, int h, uint8_t code )
{
    for (int _x=x1; _x<=x1+w-1; ++_x)
    {
        for (int _y=y1; _y<=y1+h-1; ++_y)
        {
            set(_x,_y,code);
        }
    }
}

void TextMatrix::clear(uint8_t ch)
{
    memset(text,ch,40*25);
    memset(cram,color,40*25);
}

void TextMatrix::print(const char* _s)
{
    if (x<1)x=1;
    if (y<1)y=1;
    if (x>40) return;
    if (y>25) return;

    while (*_s && x<=40 && y<=25)
    {
        int ch = *_s++;
        switch(ch)
        {
            case 13:
                x=0;
                y++;
                continue;
            default:
                break;
        }
        if (ch<0x20) ch+=0x60;
        else if (ch>0x60) ch-=0x60;
        set(x,y,ch| reverse);
        x++;
    }
}

void TextMatrix::printf(const char* _fmt, ...)
{
    char buffer[64];
    va_list va;
    va_start(va,_fmt);
    int len = vsnprintf(buffer,64,_fmt,va);
    va_end(va);
    print(buffer);
}

void TextMatrix::scroll(int x, int y, int w, int h, const ScrollDirection& direction)
{
  // TODO
}

