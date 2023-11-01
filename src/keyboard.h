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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <deque>

enum CbmKeyType {
  CBM_KEY_INSDEL=0, //	$00	&00	Insert/Delete
  CBM_KEY_RETURN=1, //	$01	&01	Return
  CBM_KEY_LEFTRIGHT=2, //	$02	&02	cursor left/right
  CBM_KEY_F7F8=3, //	$03	&03	F7/F8
  CBM_KEY_F1F2=4, //	$04	&04	F1/F2
  CBM_KEY_F3F4=5, //	$05	&05	F3/F4
  CBM_KEY_F5F6=6, //	$06	&06	F5/F6
  CBM_KEY_UPDOWN=7, //	$07	&07	cursor up/down
  CBM_KEY_3=8, //	$08	&10	3
  CBM_KEY_W=9, //	$09	&11	W
  CBM_KEY_A=10, //	$0A	&12	A
  CBM_KEY_4=11, //	$0B	&13	4
  CBM_KEY_Z=12, //	$0C	&14	Z
  CBM_KEY_S=13, //	$0D	&15	S
  CBM_KEY_E=14, //	$0E	&16	E
  CBM_KEY_LSHIFT=15, //	$0F	&17	left Shift
  CBM_KEY_5=16, //	$10	&20	5
  CBM_KEY_R=17, //	$11	&21	R
  CBM_KEY_D=18, //	$12	&22	D
  CBM_KEY_6=19, //	$13	&23	6
  CBM_KEY_C=20, //	$14	&24	C
  CBM_KEY_F=21, //	$15	&25	F
  CBM_KEY_T=22, //	$16	&26	T
  CBM_KEY_X=23, //	$17	&27	X
  CBM_KEY_7=24, //	$18	&30	7
  CBM_KEY_Y=25, //	$19	&31	Y
  CBM_KEY_G=26, //	$1A	&32	G
  CBM_KEY_8=27, //	$1B	&33	8
  CBM_KEY_B=28, //	$1C	&34	B
  CBM_KEY_H=29, //	$1D	&35	H
  CBM_KEY_U=30, //	$1E	&36	U
  CBM_KEY_V=31, //	$1F	&37	V
  CBM_KEY_9=32, //	$20	&40	9
  CBM_KEY_I=33, //	$21	&41	I
  CBM_KEY_J=34, //	$22	&42	J
  CBM_KEY_0=35, //	$23	&43	0
  CBM_KEY_M=36, //	$24	&44	M
  CBM_KEY_K=37, //	$25	&45	K
  CBM_KEY_O=38, //	$26	&46	O
  CBM_KEY_N=39, //	$27	&47	N
  CBM_KEY_PLUS=40, //	$28	&50	+ (plus)
  CBM_KEY_P=41, //	$29	&51	P
  CBM_KEY_L=42, //	$2A	&52	L
  CBM_KEY_MINUS=43, //	$2B	&53	– (minus)
  CBM_KEY_PERIOD=44, //	$2C	&54	. (period)
  CBM_KEY_COLON=45, //	$2D	&55	: (colon)
  CBM_KEY_AT=46, //	$2E	&56	@ (at)
  CBM_KEY_COMMA=47, //	$2F	&57	, (comma)
  CBM_KEY_POUND=48, //	$30	&60	£ (pound)
  CBM_KEY_ASTERISK=49, //	$31	&61	* (asterisk)
  CBM_KEY_SEMICOLON=50, //	$32	&62	; (semicolon)
  CBM_KEY_CLEARHOME=51, //	$33	&63	Clear/Home
  CBM_KEY_RSHIFT=52, //	$34	&64	right Shift
  CBM_KEY_EQUAL=53, //	$35	&65	= (equal)
  CBM_KEY_UPARROW=54, //	$36	&66	↑ (up arrow)
  CBM_KEY_SLASH=55, //	$37	&67	/ (slash)
  CBM_KEY_1=56, //	$38	&70	1
  CBM_KEY_LEFTARROW=57, //	$39	&71	← (left arrow)
  CBM_KEY_CONTROL=58, //	$3A	&72	Control
  CBM_KEY_2=59, //	$3B	&73	2
  CBM_KEY_SPACE=60, //	$3C	&74	Space
  CBM_KEY_COMMODORE=61, //	$3D	&75	Commodore
  CBM_KEY_Q=62, //	$3E	&76	Q
  CBM_KEY_RUNSTOP=63, //	$3F	&77	Run/Stop 
  CBM_KEY_NULL=0,
};

class Keyboard
{
  public:
  uint8_t kbMatrixRow[8];
  uint8_t kbMatrixCol[8];

  struct KeyEntry
  {
    CbmKeyType key;
    uint8_t pressed : 1;
    uint8_t delay : 7;
    KeyEntry(CbmKeyType _key, uint8_t _pressed, uint8_t _delay=1) : key(_key), pressed(_pressed), delay(_delay) {}
  };
  
  std::deque<KeyEntry> queue;
  int counter;

  static const inline uint8_t row(CbmKeyType key)
  {
    return (key>>3)&7;
  }
  static const inline uint8_t col(CbmKeyType key)
  {
    return key&7;
  }

public:
  Keyboard();
  void press(CbmKeyType key);
  void release(CbmKeyType key, uint8_t _delay=1);
  void processKey();
  inline void type(CbmKeyType key, CbmKeyType mod=CBM_KEY_INSDEL)
  {
    if (mod) press(mod);
    press(key);
    release(key);
    if (mod) release(mod);
  }

  void reset();
  bool keypressed(CbmKeyType key);
  void pressKey(CbmKeyType key);
  void releaseKey(CbmKeyType key);
  uint8_t getColumnValues(uint8_t rowMask);
  uint8_t getRowValues(uint8_t columnMask);
};
extern Keyboard kbd;

#endif

