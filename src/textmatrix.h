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

#ifndef TEXTMATRIX_H
#define TEXTMATRIX_H

#include <stdint.h>
#include <vector>
#include <functional>
#include "ansi.h"

void refreshscreen(AnsiRenderer *_ansi, bool _use_ansi);

enum {
    OLINE=0x63, // overline
    ULINE=0x64, // underline
    LINEH=0x40, 
    LINEV=0x5d,
    EDGETL=0x70,
    EDGETR=0x6e,
    EDGEBL=0x6d,
    EDGEBR=0x7d,
    TRIDOWN=0x72,
    TRIUP=0x71,
    TRILEFT=0x73,
    TRIRIGHT=0x6b,
    CROSSWAY=0x5b,
};

class TextMatrix
{
public:
    uint8_t* text;
    uint8_t* cram;
    uint8_t x;
    uint8_t y;
    uint8_t color;
    uint8_t reverse;
    bool showcursor;
    bool sync;

    TextMatrix()
    {
        static uint8_t s_text[40*25];
        static uint8_t s_cram[40*25];
        text = &s_text[0];
        cram = &s_cram[0];
        clear(0);
        sync = true;
    }

    TextMatrix(uint8_t* _text, uint8_t* _cram)
    {
        text = _text;
        cram = _cram;
        clear(0);
        sync = true;
    }

    inline void setColor(uint8_t _color)
    {
        color = _color;
    }
    inline void setCursor(uint8_t _x, uint8_t _y)
    {
        x = _x;
        y = _y;
    }
    inline void setReverse(bool _on)
    {
        reverse = _on ? 0x80 : 0x00;
    }

    void showCursor()
    {
    }
    void hideCursor()
    {
    }

    void set(int x, int y, uint8_t code);
    uint8_t get(int x, int y) const;

    void hline(int x1, int x2, int y, uint8_t _code=LINEH );
    void vline(int y1, int y2, int x, uint8_t _code=LINEV );
    void frame(int x1, int y1, int w, int h, bool round = false );
    void fill(int x1, int y1, int w, int h, uint8_t code );
    void clear(uint8_t ch);
    void tab(int x1, int y1, int w, int h, const char* _header, bool _active);
    void progress(int x, int y, int w, int v);

    void printf(const char* _fmt, ...);
    void print(const char* _s);
    enum ScrollDirection { UP, DOWN, LEFT, RIGHT };
    void scroll(int x, int y, int w, int h, const ScrollDirection& direction);

};

#endif
