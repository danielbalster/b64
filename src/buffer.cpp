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

#include "buffer.h"

Buffer::Buffer(size_t _size) : buffer(nullptr)
{
    inplace = false;
    resize(_size);
}

Buffer::Buffer(uint8_t* _inplace, size_t _size)
{
    inplace = true;
    buffer = _inplace;
    buffer_size = _size;
    rpos = 0;
    wpos = 0;
}

Buffer::~Buffer()
{
    resize(0);
}

bool Buffer::operator! () const
{
    return !!buffer;
}

void Buffer::resize(size_t _size)
{
    if (!inplace && buffer) delete [] buffer;
    buffer = nullptr;
    buffer_size = 0;
    if (_size==0) return;
    buffer = new uint8_t[_size];
    buffer_size = _size;
    rpos = 0;
    wpos = 0;
    inplace = false;
}

size_t Buffer::write(uint8_t ch)
{
    if (buffer==nullptr) return 0;
    if (wpos>=buffer_size) return 0;
    buffer[wpos] = ch;
    wpos++;
    return 1;
}

size_t Buffer::write(const char* b, size_t _n)
{
    size_t l=0;
    for (int i=0; i<_n; ++i) write(b[i]);
    return l;
}

int Buffer::available()
{
    if (buffer==nullptr) return 0;
    return wpos-rpos;
}

int Buffer::read()
{
    if (buffer==nullptr) return -1;
    if (rpos>=wpos) return -1;
    int ch = buffer[rpos];
    rpos++;
    return ch;
}

int Buffer::peek()
{
    if (buffer==nullptr) return -1;
    return buffer[rpos];
}

void Buffer::flush()
{
    wpos = rpos = 0;
}
