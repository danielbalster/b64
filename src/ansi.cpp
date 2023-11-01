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

#include "ansi.h"
#include <Arduino.h>

static const char* CSI = "\e[";

const char* Ansi::Foreground[16] = {
      "\e[38;5;0m",// BLK
      "\e[38;5;15m",// WHT
      "\e[38;5;88m",// RED  \e[38;2;137;63;48;m
      "\e[38;5;109m",// CYN \e[38;2;128;186;200;m
      "\e[38;5;92m",// PUR \e[38;2;142;70;177;m
      "\e[38;5;70m",// GRN \e[38;2;105;168;65;m
      "\e[38;5;18m",// BLU \e[38;2;61;49;166;m
      "\e[38;5;185m",// YEL \e[38;2;208;219;114;m
      "\e[38;5;94m",// ORN \e[38;2;145;93;38;m
      "\e[38;5;52m",// BRN \e[38;2;89;66;0;m
      "\e[38;5;138m",// LRED \e[38;2;187;118;106;m
      "\e[38;5;240m",// GRY1 \e[38;2;86;83;82;m
      "\e[38;5;244m",// GRY2 \e[38;2;127;131;113;m
      "\e[38;5;150m",// LGRN \e[38;2;175;232;135;m
      "\e[38;5;62m",// LBLU \e[38;2;122;114;212;m
      "\e[38;5;248m",// GRY3 \e[38;2;171;171;170;m
};

const char* Ansi::Background[16] = {
      "\e[48;5;0m",// BLK
      "\e[48;5;15m",// WHT
      "\e[48;5;88m",// RED  \e[38;2;137;63;48;m
      "\e[48;5;109m",// CYN \e[38;2;128;186;200;m
      "\e[48;5;92m",// PUR \e[38;2;142;70;177;m
      "\e[48;5;70m",// GRN \e[38;2;105;168;65;m
      "\e[48;5;18m",// BLU \e[38;2;61;49;166;m
      "\e[48;5;185m",// YEL \e[38;2;208;219;114;m
      "\e[48;5;94m",// ORN \e[38;2;145;93;38;m
      "\e[48;5;52m",// BRN \e[38;2;89;66;0;m
      "\e[48;5;138m",// LRED \e[38;2;187;118;106;m
      "\e[48;5;240m",// GRY1 \e[38;2;86;83;82;m
      "\e[48;5;244m",// GRY2 \e[38;2;127;131;113;m
      "\e[48;5;150m",// LGRN \e[38;2;175;232;135;m
      "\e[48;5;62m",// LBLU \e[38;2;122;114;212;m
      "\e[48;5;248m",// GRY3 \e[38;2;171;171;170;m
};

int Ansi::read(Stream& _s)
{
  int c = _s.read();
  if (c!=27) return c;
  int modifier = 0;
  int c2 = _s.read();
  if (c2=='[')
  {
      String ansi;
      while (true)
      {
          c2 = _s.read(); 
          if (c2==-1) break;
          ansi += (char)c2;
      }
      if (ansi=="A") c=VK_UP;
      else if (ansi=="B") c=VK_DOWN;
      else if (ansi=="C") c=VK_RIGHT;
      else if (ansi=="D") c=VK_LEFT;
      else if (ansi=="11~") c=VK_F1;
      else if (ansi=="12~") c=VK_F2;
      else if (ansi=="13~") c=VK_F3;
      else if (ansi=="14~") c=VK_F4;
      else if (ansi=="15~") c=VK_F5;
      else if (ansi=="17~") c=VK_F6;
      else if (ansi=="18~") c=VK_F7;
      else if (ansi=="19~") c=VK_F8;
      else if (ansi=="20~") c=VK_F9;
      else if (ansi=="21~") c=VK_F10;
      else if (ansi=="23~") c=VK_F11;
      else if (ansi=="24~") c=VK_F12;
      else if (ansi=="2~") c=VK_INSERT;
      else if (ansi=="3~") c=VK_DELETE;
      else if (ansi=="1~") c=VK_HOME;
      else if (ansi=="4~") c=VK_END;
      else if (ansi=="5~") c=VK_PGDOWN;
      else if (ansi=="6~") c=VK_PGUP;
  }
  return c | modifier;
}

AnsiRenderer::AnsiRenderer()
{
  uppercase = true;
}
void Ansi::setCursor(int x, int y)
{
    Serial.printf("\e[%d;%dH",y,x);
}
void Ansi::showCursor()
{
    Serial.print("\e[?25h");
}
void Ansi::hideCursor()
{
    Serial.print("\e[?25l");
}
void Ansi::setTextColor(uint8_t color)
{
    Serial.print(Ansi::Foreground[color&15]);
}
void Ansi::setBackColor(uint8_t color)
{
    Serial.print(Ansi::Background[color&15]);
}
void Ansi::clearScreen()
{
    Serial.print("\e[2J");
}
void AnsiRenderer::invalidate(uint8_t* text, uint8_t* cram)
{
  bg = -1;
  if (text==0) return;
  if (cram==0) return;
  for (int i=0; i<1000; ++i)
  {
    text_shadow[i] = ~text[i];
    cram_shadow[i] = ~cram[i];
  }
}
int utf8_encode(char *out, uint32_t utf)
{
  if (utf <= 0x7F) {
    out[0] = (char) utf;
    out[1] = 0;
    return 1;
  }
  else if (utf <= 0x07FF) {
    out[0] = (char) (((utf >> 6) & 0x1F) | 0xC0);
    out[1] = (char) (((utf >> 0) & 0x3F) | 0x80);
    out[2] = 0;
    return 2;
  }
  else if (utf <= 0xFFFF) {
    out[0] = (char) (((utf >> 12) & 0x0F) | 0xE0);
    out[1] = (char) (((utf >>  6) & 0x3F) | 0x80);
    out[2] = (char) (((utf >>  0) & 0x3F) | 0x80);
    out[3] = 0;
    return 3;
  }
  else if (utf <= 0x10FFFF) {
    out[0] = (char) (((utf >> 18) & 0x07) | 0xF0);
    out[1] = (char) (((utf >> 12) & 0x3F) | 0x80);
    out[2] = (char) (((utf >>  6) & 0x3F) | 0x80);
    out[3] = (char) (((utf >>  0) & 0x3F) | 0x80);
    out[4] = 0;
    return 4;
  }
  else { 
    out[0] = (char) 0xEF;  
    out[1] = (char) 0xBF;
    out[2] = (char) 0xBD;
    out[3] = 0;
    return 0;
  }
}

void AnsiRenderer::sync(uint8_t *text, uint8_t *cram, uint8_t bgcolor)
{
    int i = 0;
    if (text == 0)
        return;
    if (cram == 0)
        return;
    uint8_t *p = text;
    uint8_t *q = text_shadow;
    uint8_t *pc = cram;
    uint8_t *qc = cram_shadow;

    uint32_t offset = 0xe000;
    if (!uppercase)
        offset += 0x100; // memorymap

    if (bg != bgcolor)
    {
        invalidate(text, cram);
        bg = bgcolor;
       setBackColor(bg & 15);
    }
    int color = -1;
    for (int y = 1; y <= 25; ++y)
    {
        int cx = -1;
        for (int x = 1; x <= 40; ++x)
        {
            if ((*p != *q) || (*pc != *qc))
            {
                *q = *p;
                *qc = *pc;
                if (cx != x)
                {
                    setCursor(x,y);
                    cx = x;
                }
                if (color != *pc)
                {
                  color = *pc;
                  setTextColor(*pc);
                }
                uint8_t ch = *p;
                char buf[6];
                utf8_encode(buf, offset + ch);
                Serial.print(buf);
                cx++;
            }
            p++;
            q++;
            pc++;
            qc++;
        }
    }
}
