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

#ifndef KERNALFILE_H
#define KERNALFILE_H

#include <Arduino.h>
#include <stdint.h>
#include <FS.h>
#include "buffer.h"

struct KernalFile
{
  File file;
  Buffer* buffer;
  uint8_t fileno;
  uint8_t secondaryAddress;
  uint8_t device;

  int read();
  void write(uint8_t _char);

  KernalFile() : fileno(0), secondaryAddress(0), device(0)
  { }

  KernalFile(File _file, Buffer* _buffer, uint8_t _fileno, uint8_t _secondaryAddress, uint8_t _device)
  : file(_file), buffer(_buffer), fileno(_fileno), secondaryAddress(_secondaryAddress), device(_device)
  { }
};

class KernalFileManager
{
  KernalFile files[10];
  int nFilesOpen = 0;
public:
  KernalFileManager();
  KernalFile* get(uint8_t _fileno, uint8_t _secondaryAddress, uint8_t _device);
  void reset();
  KernalFile* open(uint8_t _fileno, uint8_t _secondaryAddress, uint8_t _device);
  void close(uint8_t _fileno, uint8_t _secondaryAddress, uint8_t _device);

};

extern KernalFileManager kfm;

#endif
