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

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

void memorymap();

extern "C" const uint8_t basic[8192] asm("_binary_roms_basic_start");
extern "C" const uint8_t kernal[8192] asm("_binary_roms_kernal_start");
extern "C" const unsigned char chargen[4096] asm("_binary_roms_chargen_start");

#if defined( ARDUINO_ARCH_ESP32 )
#define RAMSIZE 64*1024
#else
#define RAMSIZE 28*1024
#endif

void memory_init();

extern uint8_t* RAM;
extern uint8_t* CRAM;
extern uint8_t processorport;
extern uint8_t EXROM;
extern uint8_t GAME;
extern uint8_t* mapped_kernal;
extern uint8_t* mapped_basic;
extern uint8_t* mapped_io;
extern uint8_t* mapped_8000;

extern uint8_t* vic_bitmap;
extern uint8_t* vic_matrix;
extern uint8_t* vic_chargen;

extern uint8_t* delayed_bitmap;
extern uint8_t* delayed_matrix;
extern uint8_t* delayed_chargen;

#endif
