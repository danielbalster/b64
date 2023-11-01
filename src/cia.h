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

#ifndef CIA_H
#define CIA_H

#include <stdint.h>
#include "chip.h"

class CIA : public Chip
{
protected:
public:

  enum {
    PRA,
    PRB,
    DDRA,
    DDRB,
    TA_LO,
    TA_HI,
    TB_LO,
    TB_HI,
    TOD_10THS,
    TOD_SEC,
    TOD_MIN,
    TOD_HR,
    SDR,
    ICR,
    CRA,
    CRB,
  };
  uint8_t imr;
  uint8_t icr;

  uint8_t pra,prb,ddra,ddrb;

  uint8_t cra;
  uint8_t crb;

  uint16_t counterA;
  uint16_t counterB;
  uint16_t latchA;
  uint16_t latchB;

  uint8_t prefetch_pra, prefetch_prb;

  virtual  void prefetch();

  void forceClock()
  {
    counterA = 1;
    counterB = 1;
    clock();
  }

  uint32_t elapsed;

  virtual void reset();
  virtual void clock();
  virtual void setup();
  virtual void nmi();

};

class CIA1 : public CIA
{
public:
  virtual uint8_t read(uint8_t adr);
  virtual void write(uint8_t adr, uint8_t value);
  virtual  void prefetch();
};

class CIA2 : public CIA
{
public:
  virtual uint8_t read(uint8_t adr);
  virtual void write(uint8_t adr, uint8_t value);
  virtual  void prefetch();
};

extern CIA1 cia1;
extern CIA2 cia2;

#endif
