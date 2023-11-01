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

#ifndef ANSI_H
#define ANSI_H

#include <Arduino.h>
#include <stdint.h>
#include <functional>

enum {
    VK_MODIFIER_MASK = 0x1fff,
    VK_SHIFT   = 0x2000,
    VK_CONTROL = 0x4000,
    VK_RETURN = 13,
    VK_SPACE = 32,
    VK_TAB = 9,
    VK_BACKSPACE = 127,
    VK_F1 = 1000,
    VK_F2,
    VK_F3,
    VK_F4,
    VK_F5,
    VK_F6,
    VK_F7,
    VK_F8,
    VK_F9,
    VK_F10,
    VK_F11,
    VK_F12,
    VK_UP,
    VK_DOWN,
    VK_LEFT,
    VK_RIGHT,
    VK_PGUP,
    VK_PGDOWN,
    VK_HOME,
    VK_END,
    VK_INSERT,
    VK_DELETE,
};

enum FixedColor {
  BLACK,WHITE,RED,CYAN,
  PURPLE,GREEN,BLUE,YELLOW,
  ORANGE,BROWN,LIGHTRED,GRAY1,
  GRAY2,LIGHTGREEN,LIGHTBLUE,GRAY3
};

class Ansi
{
public:
  static const char* Foreground[16];
  static const char* Background[16];

  static int read(Stream& _s);
  static void setCursor(int x, int y);
  static void showCursor();
  static void hideCursor();
  static void setTextColor(uint8_t color);
  static void setBackColor(uint8_t color);
  static void clearScreen();
};

class AnsiRenderer : public Ansi
{
  uint8_t text_shadow[1000]; // dirty map
  uint8_t cram_shadow[1000]; // dirty map
  int bg;
public:
  bool uppercase;

  AnsiRenderer();
  void invalidate(uint8_t* text, uint8_t* cram);
  void sync(uint8_t* text, uint8_t* cram, uint8_t bgcolor);
};

int utf8_encode(char *out, uint32_t utf);

#endif
