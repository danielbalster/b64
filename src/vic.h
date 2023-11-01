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

#ifndef VIC_H
#define VIC_H

#include <stdint.h>

static const uint16_t PAL_TOP = (16+35);
static const uint16_t PAL_BOTTOM = 312-((12+49));
static const int16_t CYCLES_PER_RASTERLINE = 63;
static const int16_t RASTERLINES_PER_FRAME = 312;
static const uint32_t CLOCK_FREQUENCY = CYCLES_PER_RASTERLINE*RASTERLINES_PER_FRAME*(50.125);

/*
static const uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static const uint16_t RGB2REF(uint32_t c)
{
  uint8_t r = (c & 0xFF0000) >> 16;
  uint8_t g = (c & 0xFF00) >> 8;
  uint8_t b = (c & 0xFF);
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
*/

#define BLOCKHEIGHT 8

class VIC
{
public:

  enum Colors
  {
    BORDER, // 0x20
    BG0,
    BG1,
    BG2,
    BG3,
    SPR_EX1,
    SPR_EX2,
    SPR0,
    SPR1,
    SPR2,
    SPR3,
    SPR4,
    SPR5,
    SPR6,
    SPR7,
  };

  struct Registers
  {
    //uint16_t sprX[8];
    //uint8_t sprY[8];
    uint8_t yctrl;
    //uint8_t spren;
    uint8_t xctrl;
    //uint8_t spryexp;
    //uint8_t sprprio;
    //uint8_t sprmc;
    //uint8_t sprxexp;
  };

  Registers current;
  Registers delayed;

  uint8_t regs[0x40];
  uint8_t colors[16];


  uint8_t* line;
  uint8_t* cram;

  uint16_t rasterline;
  uint16_t rasterline_irq;
  uint8_t imr;  // d01a
  uint8_t irr;  // d019
  uint8_t mem;  // d018
  uint8_t spren; // d015
  uint8_t d01c; // sprite multicolor
  uint8_t sprxexp;
  uint8_t spryexp;
  uint8_t cycle;
  bool isCanvas;
  bool isBorder;

  uint16_t vc; // video matrix counter, 10 bit (0-1023)
  uint16_t vcbase;
  uint8_t rc; // row counter

  uint16_t rasterx; // current x position
  uint16_t rastery;
  uint8_t* pout;
  
  bool cpustall; // true if vic stalls the CPU, ie badline conditions etc
  bool cpu_is_rdy;
  bool enabled;
  bool DENwasSetInRasterline30;

  // cached: 24 bit sprite bitmap for current line
  struct {
    uint32_t bitmap;
    uint16_t x;
    uint8_t y;
    uint8_t active;
    uint8_t x2,y2;
  } spr[8];

  void setup_palette();

  void begin(uint16_t y);
  inline void update()
  {
    if (draw==nullptr) return;
    (this->*draw)();
    if (spren) draw_sprites();
    rasterx+=8;
  }


  void end();
  void(VIC::*draw)();
  void evalDrawMode();
  void clock();

  void setup_cyclefuncs();
  void setup();

  uint16_t base;
  void reset();

  void draw_background();
  void draw_sprites();
  void draw_text_mono();
  void draw_bitmap_mono();
  void draw_text_multicolor();
  void draw_bitmap_multicolor();
  void draw_border_color();
  void draw_text_ecm();
  void draw_void();

  uint8_t read(uint8_t adr);
  void write(uint8_t adr, uint8_t value);

};

extern VIC vic;

#endif
