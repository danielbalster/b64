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

#include "vic.h"
#include "memory.h"
#include "cpu.h"
#include "cia.h"
#include <stdio.h>
#include <string.h>

VIC vic;

uint8_t VIC::read(uint8_t adr)
{
    switch(adr)
    {
      case 0: return spr[0].x & 255;
      case 1: return spr[0].y;
      case 2: return spr[1].x & 255;
      case 3: return spr[1].y;
      case 4: return spr[2].x & 255;
      case 5: return spr[2].y;
      case 6: return spr[3].x & 255;
      case 7: return spr[3].y;
      case 8: return spr[4].x & 255;
      case 9: return spr[4].y;
      case 10: return spr[5].x & 255;
      case 11: return spr[5].y;
      case 12: return spr[6].x & 255;
      case 13: return spr[6].y;
      case 14: return spr[7].x & 255;
      case 15: return spr[7].y;
      case 16:
      {
        uint8_t bits = 0;
        bits |= ((spr[0].x>>8)&1) << 0;
        bits |= ((spr[1].x>>8)&1) << 1;
        bits |= ((spr[2].x>>8)&1) << 2;
        bits |= ((spr[3].x>>8)&1) << 3;
        bits |= ((spr[4].x>>8)&1) << 4;
        bits |= ((spr[5].x>>8)&1) << 5;
        bits |= ((spr[6].x>>8)&1) << 6;
        bits |= ((spr[7].x>>8)&1) << 7;
        return bits;
      }
      case 0x17:
        return spryexp;
      case 0x1d:
        return sprxexp;
      case 0x1c:
        return d01c;
      case 0x11:
        return (current.yctrl&0x7f) | ((rasterline>0xff)?0x80:0x00);
      case 0x12:
        return rasterline & 0xFF;
      case 0x15:
        return spren;
      case 0x16:
        return current.xctrl | 0xC0;
      case 0x18:
        return mem | 1;
      case 0x19:
        return (irr & imr) ? (irr|0xF0) : (irr|0X70);
      case 0x1a:
        return imr | 0xF0;
      case 0x1f: // spr bkg collision
      case 0x1e: // spr spr collision
        {
          uint8_t v = regs[adr];
          regs[0x1e] = 0;
          return v;
        }
      case 0x20:
      case 0x21:
      case 0x22:
      case 0x23:
      case 0x24:
      case 0x25:
      case 0x26:
      case 0x27:
      case 0x28:
      case 0x29:
      case 0x2a:
      case 0x2b:
      case 0x2c:
      case 0x2d:
      case 0x2e:
        return colors[adr-0x20] | 0xF0;
    }
    return regs[adr];
  }

  void VIC::write(uint8_t adr, uint8_t value)
  {
    regs[adr] = value;
    switch (adr)
    {
      case 0:
        spr[0].x &= 0x100;
        spr[0].x |= value;
        return;
      case 1:
        spr[0].y = value;
        return;
      case 2:
        spr[1].x &= 0x100;
        spr[1].x |= value;
        return;
      case 3:
        spr[1].y = value;
        return;
      case 4:
        spr[2].x &= 0x100;
        spr[2].x |= value;
        return;
      case 5:
        spr[2].y = value;
        return;
      case 6:
        spr[3].x &= 0x100;
        spr[3].x |= value;
        return;
      case 7:
        spr[3].y = value;
        return;
      case 8:
        spr[4].x &= 0x100;
        spr[4].x |= value;
        return;
      case 9:
        spr[4].y = value;
        return;
      case 10:
        spr[5].x &= 0x100;
        spr[5].x |= value;
        return;
      case 11:
        spr[5].y = value;
        return;
      case 12:
        spr[6].x &= 0x100;
        spr[6].x = value;
        return;
      case 13:
        spr[6].y = value;
        return;
      case 14:
        spr[7].x &= 0x100;
        spr[7].x |= value;
        return;
      case 15:
        spr[7].y = value;
        return;
      case 16:
        for (int i=0; i<8; ++i)
        {
          spr[i].x &= 0xFF;
          spr[i].x |= ((value>>i) & 1)<<8;
        }
      return;
      case 0x11:
        current.yctrl = delayed.yctrl = value;
        if (unlikely(rasterline == rasterline_irq && rasterline != 0))
        {
          irr |= 1;
          if (irr & imr)
          {
            cpu.irq();
          }
        }
        memorymap();
        return;
      case 0x15:
        spren = value;
        return;
      case 0x17:
        spryexp = value;
        return;
      case 0x1d:
        sprxexp = value;
        return;
      case 0x16:
        delayed.xctrl = value;
        memorymap();
        return;
      case 0x18: // mem control
        mem = value;
        memorymap();
      return;
      case 0x19:
        irr &= (~value) & 0x0F;
        return;
      case 0x1a:
        imr = value & 0x0F;
        return;
      case 0x1c:
        d01c = value;
        return;
      case 0x12:
        if (value != rasterline_irq)
        {
          rasterline_irq = value;

          if (unlikely(rasterline == rasterline_irq))
          {
            irr |= 1;
            if (irr & imr)
            {
              cpu.irq();
            }
          }

          //if (rasterline == rasterline_irq)
          //{
          //  irr |= 1;
          //}
        }
        return;

      case 0x20: // border color
      case 0x21:  // background color
      case 0x22:  // multicolor 1
      case 0x23:  // multicolor 2
      case 0x24:
      case 0x25:
      case 0x26:
      case 0x27:
      case 0x28:
      case 0x29:
      case 0x2a:
      case 0x2b:
      case 0x2c:
      case 0x2d:
      case 0x2e:
          colors[adr - 0x20] = value & 0xF;
          return;
    }
  }

void VIC::draw_text_multicolor()
{

}

void VIC::draw_bitmap_multicolor()
{

}

void VIC::draw_border_color()
{

}

void VIC::draw_void()
{

}

void VIC::draw_bitmap_mono()
{

}

void VIC::draw_text_ecm()
{

}

void VIC::draw_text_mono()
{

}

void VIC::draw_background()
{
}

void VIC::draw_sprites()
{
}

void VIC::evalDrawMode()
{
  if (((current.yctrl & 0x10)==0) // den
  || (((current.yctrl & 8)==0) && ((rastery<8) || (rastery>=(200-8))))
  )
  {
    draw = nullptr; //&VIC::draw_border_color;
    return;
  }
  if (current.yctrl & 0x20) // bitmaps
  {
    if (current.xctrl & 0x10) // multicolor
    {
      draw = &VIC::draw_bitmap_multicolor;
    }
    else
    {
      draw = &VIC::draw_bitmap_mono;
    }
  }
  else
  {
    if (current.xctrl & 0x10) // multicolor
    {
      draw = &VIC::draw_text_multicolor;
    }
    else if (current.yctrl & 0x40) // extended color mode
    {
      draw = &VIC::draw_text_ecm;
    }
    else
    {
      draw = &VIC::draw_text_mono;
    }
  }
}

void VIC::begin(uint16_t y)
{
  rasterline = y;
  current.xctrl = delayed.xctrl;
  current.yctrl = delayed.yctrl;
   
  isCanvas = y>=PAL_TOP && y<PAL_BOTTOM;
  isBorder = false;

  if (y == 0x30) DENwasSetInRasterline30 = (current.yctrl & 0x10);
}

void VIC::end()
{
}

void VIC::clock()
{
  switch(cycle)
  {
    case 2:
    if (unlikely(rasterline == rasterline_irq))
    {
      irr |= 1;
      if (irr & imr)
      {
        cpu.irq();
      }
    }
    break;
    case 17:
      if (likely(isCanvas))
      {
        cpu_is_rdy = ((rasterline & 0x07) != (current.yctrl & 0x07)) && DENwasSetInRasterline30;
        cpu_is_rdy |= (current.yctrl & 0x10) == 0x00;
      }
    break;
    case 57:
      cpu_is_rdy = true;
    break;
  }
}

void VIC::setup()
{
}

void VIC::setup_palette()
{

}

void VIC::reset()
{
  memset(regs,0,sizeof(regs));
  rasterline_irq = 0;
  rasterline = 0;
  current.yctrl = delayed.yctrl = 0x9B;
  current.xctrl = delayed.xctrl = 0x08;
  mem = 0x14;
  irr = 0x0f;
  sprxexp = 0;
  spryexp = 0;
  spren = 0;
  cpu_is_rdy = true;
  DENwasSetInRasterline30 = false;
  memorymap();
}
