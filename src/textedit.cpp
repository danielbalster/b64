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

#include "textedit.h"


int TextEdit::filter(int c)
{
    if (isprint(c))
    {
        return toupper(c);
    }
    return -1;
}

bool TextEdit::input(int c)
{
    if (c == VK_RETURN)
    {
        if (accepted != nullptr) accepted(buffer);
        return true;
    }
    switch (c)
    {
        case VK_UP:
        case VK_DOWN:
        case VK_PGDOWN:
        case VK_PGUP:
        return false;
        case VK_HOME:
        cursor = 0;
        break;
        case VK_END:
        cursor = buffer.length();
        break;
        case VK_LEFT:
            if (cursor>0) cursor--;
            break;
        case VK_RIGHT:
            if (cursor < buffer.length() && cursor < width) cursor++;
            break;
        case VK_BACKSPACE:
            if (cursor>0)
            {
                cursor--;
                buffer.erase(cursor,1);
            }
            break;
        case VK_DELETE:
            if (buffer.length()>0)
            {
                buffer.erase(cursor,1);
                if (cursor>buffer.length()) cursor=buffer.length();
            }
            break;

    }
    c = filter(c);
    if (c != -1)
    {
        if (buffer.length()<width)
        {
            buffer.insert(cursor,1,(char)c);
            cursor++;
        }
        return true;
    }
    return false;
}

void TextEdit::output(TextMatrix* tm)
{
    tm->setColor(hasFocus() ? color: GRAY1 );
    tm->fill(x,y,width,height,' ');
    tm->setCursor(x,y);
    tm->print(buffer.c_str());
    if (hasFocus())
    {
        tm->setCursor(x+cursor,y);
        if (hasFocus()) AnsiRenderer::setCursor(x+cursor,y);
    }
}
