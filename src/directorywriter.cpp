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

#include "directorywriter.h"

DirectoryWriter::DirectoryWriter(Buffer* _buffer) : buffer(_buffer), width(28)
{

}

void DirectoryWriter::begin(const char* _label, uint16_t _addr)
{
    addr = _addr;
    start = addr;
    buffer->write((start + 30) & 0xff);
    buffer->write((start + 30) >> 8);
    buffer->write(0); // line number 0
    buffer->write(0);
    buffer->write(" \x22",2);
    char label[32];
    strncpy(label,_label,32);
    for (int i=strlen(label); i<32; ++i) label[i]=' ';
    buffer->write(label,16);
    buffer->write("\x22 IK 2A\x0",8);
    addr = start + 30;
}

void DirectoryWriter::add(int blocks, const char* name, int filetype)
{
    int offset = 0;
    start = addr;
    // +4 = these four bytes
    buffer->write((start + width + 4) & 0xff);
    buffer->write((start + width + 4) >> 8);
    //# of blocks
    buffer->write(blocks & 0xff);
    buffer->write(blocks >> 8);

    int m1 = buffer->available();

    int offs = 1;
        if (blocks < 100) offs++;
        if (blocks < 10)  offs++;
    buffer->write("   ",offs);

    char buf[64];
    strncpy(buf,name,64);
    buf[16]=0;
    buffer->printf("\x22%s\x22",buf);
    int m2 = buffer->available();

    int n = m2-m1;
    // 4 = first four bytes
    // 4-offs = 1-3 space depending on blocks
    for (int i=n; i<width-4-(4-offs); ++i) buffer->write(' ');

    const char* type = 0;
    switch (filetype & 7)
    {
        case 0: type = "DEL"; break;
        case 1: type = "SEQ"; break;
        case 2: type = "PRG"; break;
        case 3: type = "USR"; break;
        case 4: type = "REL"; break;
        case 5: type = "???"; break;
        case 6: type = "SID"; break;
        case 7: type = "DIR"; break;
    }

    buffer->write(type,3);
    buffer->write(' ');
    buffer->write(0);

            addr = start + width + 4; // 
}

void DirectoryWriter::end(int blocks)
{
/*add last line to BASIC listing*/
    start = addr;

    buffer->write((start + width + 4) & 0xff);
    buffer->write((start + width + 4) >> 8);
            //# of blocks
    buffer->write(blocks & 0xff);
    buffer->write(blocks >> 8);

    int m1=buffer->available();
    int offs = 1;
        if (blocks < 100) offs++;
        if (blocks < 10)  offs++;
    buffer->write("   ",offs);

    buffer->write("BLOCKS FREE.",12);
    int m2=buffer->available();
    int n=m2-m1;

    for (int i=n; i<width-4-(4-offs); ++i) buffer->write(0);
    buffer->write(0);
    buffer->write(0);
    buffer->write(0);
}
