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

#include "dialog.h"
#include "widget.h"
#include "textmatrix.h"

void Dialog::focusNext()
{
    auto iter = std::find(focusList.begin(),focusList.end(),focussedWidget);
    focussedWidget = nullptr;
    while (focussedWidget == nullptr)
    {
        std::advance(iter,1);
        if (iter==focusList.end()) iter=focusList.begin();
        if ((*iter)->enabled)
        {
            (*iter)->setFocus();
        }
    }
}

int Dialog::input()
{
    int c = Ansi::read(Serial);
    if (c == -1) return -1;

    if (c==VK_TAB)
    {
        focusNext();
        return -1;
    }

    if (focussedWidget && focussedWidget->enabled)
    {
        if (focussedWidget->input(c)) return -1;
    }

    return c;
}

void Dialog::output(TextMatrix* tm)
{
    for (auto& w : widgets)
    {
        if (w->enabled) w->output(tm);
    }
}

void Dialog::run(Dialog *parent, TextMatrix* _matrix, AnsiRenderer* _ansi, bool _uppercase)
{
    this->matrix = _matrix;
    this->ansi = _ansi;
    Widget* oldFocus = (parent) ? parent->focussedWidget : nullptr;
    result=RUNNING;
    bool old = ansi->uppercase;
    ansi->uppercase = _uppercase;
    ansi->invalidate(matrix->text,matrix->cram);
    load();
    while (result==RUNNING)
    {
        output(matrix);
        if (matrix->sync) ansi->sync(matrix->text,matrix->cram,BLACK);
        input();
    }
    unload();
    ansi->uppercase = old;
    ansi->invalidate(matrix->text,matrix->cram);
    if (oldFocus) oldFocus->setFocus();
}

void Dialog::add(Widget* _w)
{
    _w->parent = this;
    widgets.push_back(_w);
    if (_w->canFocus)
    {
        focusList.push_back(_w);
        _w->setFocus();
    }
}

