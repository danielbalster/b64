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

#include "keyboard.h"
#include <string.h>

#define GET_BIT(bits,num) (((bits)>>num)&1)
#define CLR_BIT(bits,num) bits&=~(1<<num)
#define SET_BIT(bits,num) bits|=(1<<num)

  Keyboard::Keyboard()
  {
    reset();
  }

  void Keyboard::press(CbmKeyType key)
  {
      queue.push_back(KeyEntry(key,1));
  }
  
  void Keyboard::release(CbmKeyType key, uint8_t _delay)
  {
      queue.push_back(KeyEntry(key,0,_delay));
  }

  void Keyboard::processKey()
  {
    if (queue.empty()) return;
    if (queue.front().pressed)
      pressKey(queue.front().key);
    else
    {
      releaseKey(queue.front().key);
    }
    queue.pop_front();
  }

  void Keyboard::reset()
  {
    memset(kbMatrixCol,0xff,sizeof(kbMatrixCol));
    memset(kbMatrixRow,0xff,sizeof(kbMatrixRow));

    queue.clear();
    counter=0;
  }
  bool Keyboard::keypressed(CbmKeyType key)
  {
    return ((kbMatrixRow[row(key)] & (1 << col(key))) == 0)
    && ((kbMatrixCol[col(key)] & (1 << row(key))) == 0);
  }
  void Keyboard::pressKey(CbmKeyType key)
  {
    kbMatrixRow[row(key)] &= ~(1 << col(key));
    kbMatrixCol[col(key)] &= ~(1 << row(key));
  }
  void Keyboard::releaseKey(CbmKeyType key)
  {
    kbMatrixRow[row(key)] |= (1 << col(key));
    kbMatrixCol[col(key)] |= (1 << row(key));
  }
  uint8_t Keyboard::getColumnValues(uint8_t rowMask)
  {
    uint8_t result = 0xff;
    
    for (unsigned i = 0; i < 8; i++) {
        if (GET_BIT(rowMask, i)) {
            result &= kbMatrixCol[i];
        }
    }
    return result;
  }
  uint8_t Keyboard::getRowValues(uint8_t columnMask)
  {
	  uint8_t result = 0xff;
		
	  for (unsigned i = 0; i < 8; i++) {
		  if (GET_BIT(columnMask, i)) {
  			result &= kbMatrixRow[i];
	  	}
	  }
    
  	return result;
  }


Keyboard kbd;
