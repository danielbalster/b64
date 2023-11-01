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

#ifndef MONITOR_H
#define MONITOR_H

#include <string>
#include "textmatrix.h"
#include "frame.h"
#include "button.h"
#include "dialog.h"
#include "slider.h"
#include "checkbox.h"
#include "text.h"
#include "progressbar.h"
#include "textedit.h"
#include "scanner.h"
#include "assembler.h"

class MnemoEdit : public TextEdit
{
public:
    bool hasSyntaxError;
    Scanner scanner;
    uint16_t addr;
    MnemoEdit(uint8_t _x, uint8_t _y, uint8_t _len)
    : TextEdit(_x,_y,_len)
    , hasSyntaxError(false)
    {

    }
    // disassemble addr and load string into buffer, cursor at and
    void read(uint16_t _addr);
    virtual int filter(int c);
    virtual bool input(int c);
    virtual void enter();
    virtual void output(TextMatrix* tm);          // draw to textmatrix
};

class NumberEdit : public TextEdit
{
public:
    NumberEdit(uint8_t _x,uint8_t _y,uint8_t _len)
    : TextEdit(_x,_y,_len)
    {
    }
    virtual bool input(int c);
};

class FilenameEdit : public TextEdit
{
public:
    FilenameEdit(uint8_t _x,uint8_t _y,uint8_t _len)
    : TextEdit(_x,_y,_len)
    {
    }
    virtual int filter(int c);
    virtual bool input(int c);
};

template <class CONTENT>
class Listing : public Widget
{
public:
    std::vector<CONTENT> items;
    int cursor;
    int offset;

    typedef std::function<void(Listing*,TextMatrix*,const CONTENT&)> render_t;
    render_t render;
    typedef std::function<void(Widget*,int)> select_t;
    select_t selected;

    Listing(uint8_t _x, uint8_t _y, uint8_t _width, uint8_t _height);
    void setCursor(int _cursor);
    virtual bool input(int c);
    virtual void output(TextMatrix* tm);

    void clear();
    void sort();
    void add(const CONTENT& _line);
    void remove(int _index);
};

class FileManagerDialog : public Dialog
{
public:
    struct ListEntry
    {
        std::string name;
        bool selected;
        bool directory;
        int size;
        bool operator< (const struct ListEntry&o) const
        {
            if (directory != o.directory) return directory > o.directory;
            return name < o.name;
        }
    };
    Listing<ListEntry> listing;
    TextEdit text1;
    Text label;


    std::string filepath;
    int drive;
    
    FileManagerDialog();
    void output(TextMatrix* tm) override;
    void scan();
    int input() override;
    void load() override;
    const char* selected() ;
};

void monitor();
void monitor_init();

#endif
