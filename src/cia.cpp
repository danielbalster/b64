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

#include "cia.h"
#include "cpu.h"
#include "keyboard.h"
#include "memory.h"
#include <Arduino.h>

#include "io.h"

CIA1 cia1;
CIA2 cia2;

void CIA::reset()
{
  pra = 0xaa;
  prb = 0xaa;
  ddra = 0xaa;
  ddrb = 0xaa;
  latchA = 0xFFFF;
  latchB = 0xFFFF;
}

void CIA::nmi()
{
  icr |= 0x80;  
}

void CIA::setup()
{

}

void CIA::clock()
{
  bool irq = false;
  if (cra&1)
  {
    counterA--;
    if (counterA==0)
    {
      if (cra&8)  // timer stops after underflow
      {
        cra&=~1;  // stop timer
      }
      else
      {
        counterA = latchA;
      }
      if (imr&1)
      {
        icr|=1; // flag interrupt 
        irq = true;
      }
    }
  }

  if (crb&1)
  {
    counterB--;
    if (counterB==0)
    {
      if (crb&8)  // timer stops after underflow
      {
        crb&=~1;  // stop timer
      }
      else
      {
        counterB = latchB;
      }
      if (imr&2)
      {
        icr|=2; // flag interrupt 
        irq = true;
      }
    }
  }

  if (irq) cpu.irq();
  // TODO: nmi
}

void CIA1::prefetch()
{
  //uint16_t bits = io_read(BUS_CIA1);
  //prefetch_pra = (bits>>8) & 0xFF;
  //prefetch_prb = bits & 0xFF;
}

void CIA2::prefetch()
{
  //uint16_t bits = io.read(BUS_CIA2);
  //prefetch_pra = (bits>>8) & 0xFF;
  //prefetch_prb = bits & 0xFF;
}

uint8_t CIA1::read(uint8_t adr)
{
  switch (adr)
  {
    case PRA:
      {
        uint8_t pa = (pra & ddra) | (0xff & ~ddra);
        uint8_t rowMask = ~prb & ddrb;
        pa &= kbd.getColumnValues(rowMask);
        return pa;
      }
    case PRB:
    {
      uint8_t pb = (prb & ddrb) | (0xff & ~ddrb);
      uint8_t columnMask = ~pra & ddra;
      pb &= kbd.getRowValues(columnMask);
      return pb;
    }
    case DDRA:
      return ddra;
    case DDRB:
      return ddrb;
    case TA_LO:
      return counterA & 0xFF;
    case TA_HI:
      return (counterA>>8) & 0xFF;
    case TB_LO:
      return counterB & 0xFF;
    case TB_HI:
      return (counterB>>8) & 0xFF;
    case ICR:
      {
        uint8_t val = (icr & imr) ? (icr|0x80) : (icr);
        icr = 0;
        return val;
      }
    break;
    case CRA:
      return cra;
    case CRB:
      return crb;
    default:
    break;
  }
  return 0;
}
void CIA1::write(uint8_t adr, uint8_t value)
{
  switch (adr)
  {
    case PRA:
      pra = value;
      //io.write(BUS_CIA1,prb,pra);
      return;
    case PRB:
      prb = value;
      //io.write(BUS_CIA1,prb,pra);
      return;
    case DDRA:
      ddra = value;
      return;
    case DDRB:
      ddrb = value;
      return;
    case TA_LO:
      latchA &= 0xFF00;
      latchA |= value;
      counterA &= 0xFF00;
      counterA |= value;
      return;
    case TA_HI:
      latchA &= 0x00FF;
      latchA |= value<<8;
      counterA &= 0x00FF;
      counterA |= value<<8;
      return;
    case ICR:
    {
      if ((value & 0x80) == 0x80)
      {
        imr |= (value & 0x1F);
      }
      else
      {
        imr &= ~(value & 0x1F);
      }
    }
    return;

    case CRA:
      cra = value;
      return;
    case CRB:
      crb = value;
      return;
  }
}
uint8_t CIA2::read(uint8_t adr)
{
  switch (adr)
  {
    case PRA:
    {
      prefetch();
      return (prefetch_pra&0xFC) | (pra&3);
    }
    case PRB:
    {
      prefetch();
      return prefetch_prb;
    }
    case DDRA:
      return ddra;
    case DDRB:
      return ddrb;
    case TA_LO:
      return counterA & 0xFF;
    case TA_HI:
      return (counterA>>8) & 0xFF;
    case TB_LO:
      return counterB & 0xFF;
    case TB_HI:
      return (counterB>>8) & 0xFF;

    case ICR:
      {
        uint8_t val = (icr & imr) ? (icr|0x80) : (icr);
        icr = 0;
        return val;
      }
    break;
    case CRA:
      return cra;
    case CRB:
      return crb;
    default:
    break;
  }
  return 0;
}

void CIA2::write(uint8_t adr, uint8_t value)
{
  switch (adr)
  {
    case PRA:
      pra = value;
      memorymap();
      //io.write(BUS_CIA2,prb,pra);
      return;
    case PRB:
      prb = value;
      //io.write(BUS_CIA2,prb,pra);
      return;
    case DDRA:
      ddra = value;
      return;
    case DDRB:
      ddrb = value;
      return;
    case TA_LO:
      latchA &= 0xFF00;
      latchA |= value;
      counterA &= 0xFF00;
      counterA |= value;
      return;
    case TA_HI:
      latchA &= 0x00FF;
      latchA |= value<<8;
      counterA &= 0x00FF;
      counterA |= value<<8;
      return;
    case ICR:
    {
      if ((value & 0x80) == 0x80)
      {
        imr |= (value & 0x1F);
      }
      else
      {
        imr &= ~(value & 0x1F);
      }
    }
    return;

    case CRA:
      cra = value;
      return;
    case CRB:
      crb = value;
      return;
  }
}
