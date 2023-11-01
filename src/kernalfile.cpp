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

#include "kernalfile.h"

KernalFileManager kfm;



int KernalFile::read()
{
  if (buffer) return buffer->read();
  return file.read();
}

void KernalFile::write(uint8_t _char)
{
  if (buffer) buffer->write(_char);
  else file.write(_char);
}

KernalFile* KernalFileManager::get(uint8_t _fileno, uint8_t _secondaryAddress, uint8_t _device)
{
  for (int i=0; i<nFilesOpen; ++i)
  {
    if (files[i].fileno==_fileno && files[i].device==_device)
    {
      return files+i;
    }
  }
  return nullptr;
}

void KernalFileManager::reset()
{
  for (int i=0; i<nFilesOpen; ++i)
  {
    if (files[i].buffer)
    {
      delete files[i].buffer;
    }
    else
    {
      files[i].file.close();
    }
    files[i].buffer = nullptr;
    files[i].file = File();
  }
  nFilesOpen=0;
}

KernalFileManager::KernalFileManager()
{

}

KernalFile* KernalFileManager::open(uint8_t _fileno, uint8_t _secondaryAddress, uint8_t _device)
{
  if (nFilesOpen==10) return nullptr;
  KernalFile* fe = files+nFilesOpen;
  nFilesOpen++;
  fe->fileno = _fileno;
  fe->secondaryAddress = _secondaryAddress;
  fe->device = _device;
  return fe;
}

void KernalFileManager::close(uint8_t _fileno, uint8_t _secondaryAddress, uint8_t _device)
{
  KernalFile* fe = get(_fileno,_secondaryAddress,_device);
  if (fe == nullptr) return;
  fe->file.close();
  fe->file = File();
  if (fe->buffer)
  {
    delete fe->buffer;
    fe->buffer = nullptr;
  }
  fe->device = -1;
  fe->fileno = -1;
  fe->secondaryAddress = -1;
  int i = fe - files; // index 
  nFilesOpen--;
  if (i != nFilesOpen) files[i] = files[nFilesOpen];
}
