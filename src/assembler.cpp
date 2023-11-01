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

#include "assembler.h"

void Assembler::define(const char* _symbol, uint16_t _value)
{
    Symbol s;
    s.key = _symbol;
    s.value = _value;
    s.key.toUpperCase();
    s.key.trim();
    symbols.push_back(s);
    std::sort(symbols.begin(),symbols.end(),[](const Symbol&a, const Symbol& b){return a.key.compareTo(b.key);});
}

bool Assembler::compile(const char* _source)
{
    Scanner scanner;
    uint16_t addr;
    // pass 0: collect symbols, each is string=value. symbol = line starts with .
    // pass 1: get lines, replace symbol with value, scan. if line is symbol, set address to current address
    // pass 2: do the same, now all addresses are resolved.
    for (int pass=0; pass<3; ++pass)
    {
        addr = start;

        const char* line = _source;
        while (*line)
        {
            String l = line;
            l.trim();
            l.toUpperCase();

            if (l.startsWith("*"))              // set origin
            {
                const char* p = l.c_str()+3;
                addr = strtoul(p,0,16);
            }
            else if (l.startsWith("."))         // manage symbols
            {
                l.remove(0,1);
                std::vector<Symbol>::iterator iter = std::find_if(symbols.begin(),symbols.end(),[l](const Symbol&o){return o.key==l;});
                if (iter != symbols.end())
                {
                    iter->value = addr;
                }
                else
                {
                    define(l.c_str(),addr);
                }
            }
            else
            {
                if (pass>0)
                {
                    for (auto& s : symbols)
                    {
                        int idx;

                        String lobyte = "<" + s.key;
                        String hibyte = ">" + s.key;
                        idx = l.indexOf(lobyte);
                        if (idx != -1)
                        {
                            l.replace(lobyte,String(s.value&255));
                        }
                        idx = l.indexOf(hibyte);
                        if (idx != -1)
                        {
                            l.replace(hibyte,String((s.value>>8)&255));
                        }
                        idx = l.indexOf(s.key);
                        if (idx != -1)
                        {
                            l.replace(s.key,String(s.value));
                        }
                    }
                    if (!scanner.scan(l.c_str()))
                    {
                        for (auto& s : symbols)
                        {
                            Serial.printf("%s=%04x\n",s.key.c_str(),s.value);    
                        }
                        Serial.printf("SYNTAX ERROR: %s\n",l.c_str());
                        return false;
                    }
                    //Serial.printf("%04X: %s\n",addr,l.c_str());    
                    addr = scanner.write(addr,pass == 2);
                }
            }
            while (*line++);
        }
    }

    start = addr;

    return true;
}
