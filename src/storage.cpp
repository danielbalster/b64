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

#include "storage.h"

#include <stdint.h>
#include <LITTLEFS.h>
#include <SD_MMC.h>
#define SD SD_MMC

static std::string directorypath[2];

bool Storage::mkdir(const char* _path, bool _sd)
{
  if (_sd)
  {
    return SD.mkdir(_path);
  }
  else
  {
    return LITTLEFS.mkdir(_path);
  }
}

void Storage::chdir(const char* _path, bool _sd)
{
  directorypath[_sd?0:1] = _path;
}

const char* const Storage::getcwd(bool _sd)
{
  return directorypath[_sd?0:1].c_str();
}

fs::File Storage::open(const char* path, const char* mode, bool _sd)
{
  if (_sd) return SD.open(path,mode);
  return LITTLEFS.open(path,mode);
}

uint32_t Storage::freeblocks(bool _sd)
{
  if (_sd) return (SD.totalBytes() - SD.usedBytes())/256;
  return (LITTLEFS.totalBytes() - LITTLEFS.usedBytes())/256;
}

bool Storage::exists(const char* name, bool _sd)
{
  if (_sd) return SD.exists(name);
  return LITTLEFS.exists(name);
}

void Storage::remove(const char* name, bool _sd)
{
  if (_sd) SD.remove(name);
  else             LITTLEFS.remove(name);
}

void Storage::rmdir(const char* name, bool _sd)
{
  if (_sd) SD.rmdir(name);
  else             LITTLEFS.rmdir(name);
}
