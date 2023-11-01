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

#include "monitor.h"
#include <Arduino.h>
#include "cpu.h"
#include <LITTLEFS.h>
#include "memory.h"
#include "ansi.h"
#include "textmatrix.h"
#include "storage.h"

static TextMatrix* g_matrix = nullptr;
static AnsiRenderer* g_ansi = nullptr;

class Disassembler : public Widget
{
public:
    uint16_t base;
    Disassembler(uint8_t _x, uint8_t _y, uint8_t _w, uint8_t _h) : Widget(_x,_y,_w,_h)
    {
        canFocus = true;
    }

    uint16_t findPrevious(uint16_t addr)
    {
        uint8_t b = peek(addr-2);
        uint8_t c = peek(addr-3);
        if (BytesPerAdressMode[AddressmodeTable[c]]==3) return addr-3;
        if (BytesPerAdressMode[AddressmodeTable[b]]==2) return addr-2;
        return addr-1;
    }

    uint16_t rollPrevious(uint16_t base, int lines)
    {
        uint16_t pc = base - (3*lines);

        while (pc < base)
        {
            uint16_t v = pc;
            for (int i=0; i<lines; ++i) v = findNext(v);
            if (v>base) return pc-1;
            if (v>=base) return pc;
            pc++;
        }
        return base;
    }

    uint16_t findNext(uint16_t addr)
    {
        return addr + BytesPerAdressMode[AddressmodeTable[peek(addr)]];
    }

    bool showBranch;
    uint16_t target;
    void checkIsBranch()
    {
        uint8_t opcode = peek(cpu.pc);
        showBranch = (AddressmodeTable[opcode] == REL);
        uint8_t nbytes = BytesPerAdressMode[AddressmodeTable[opcode]];

        uint16_t ra = peek(cpu.pc+1);
        if (ra&0x80) ra |= 0xFF00;
        target = ((cpu.pc + nbytes) + ra)&0xFFFF;
    }

    uint16_t effectiveAddress;
    void calcEffectiveAddress()
    {
        uint8_t opcode = peek(cpu.pc);
        effectiveAddress = 0;
        switch(AddressmodeTable[opcode])
        {
            case IMP:
            break;
            case INDX:
            break;
            case ZP:
            break;
            case IMM:
            break;
            case ACC:
            break;
            case ABSO:
            break;
            case REL:
            break;
            case INDY:
            break;
            case ZPX:
            break;
            case ZPY:
            break;
            case ABSX:
            break;
            case ABSY:
            break;
            case IND:
            break;
        }
    }

    uint16_t disassemble(TextMatrix* tm, int x, int y, int color, uint16_t addr)
    {
        uint8_t b[3]={0};
        uint8_t opcode = peek(addr);
        uint8_t nbytes = BytesPerAdressMode[AddressmodeTable[opcode]];
        b[0] = opcode;
        if (nbytes>1) b[1] = peek(addr+1);
        if (nbytes>2) b[2] = peek(addr+2);

        tm->setCursor(x,y);
        tm->setColor(color);
        
        if (addr == cpu.pc)
            tm->setColor(YELLOW);

        if (cpu.findBreakpoint(addr))
        {
            tm->setColor(RED);
        }


        tm->printf("%04X:",addr);

        switch(nbytes)
        {
            case 1: tm->printf("%02X      ",b[0]); break;
            case 2: tm->printf("%02X %02X   ",b[0],b[1]); break;
            case 3: tm->printf("%02X %02X %02X",b[0],b[1],b[2]); break;
        }

        tm->print(" ");
        
        const char* opn = OpcodeToString(OpcodeTable[opcode]);
        switch (AddressmodeTable[opcode])
        {
            case IMP: // 1 byte
                             //LDA ($1000),X
                tm->printf("%s        ", opn);
                break;
            case ACC:
                tm->printf("%s        ", opn);
                break;
            case IMM: // 2 byte
                tm->printf("%s #$%02X   ", opn, b[1]);
                break;
            case ZP:
                tm->printf("%s $%02X    ", opn, b[1] );
                break;
            case INDX:
                tm->printf("%s ($%02X,X)", opn, b[1]);
                break;
            case INDY:
                tm->printf("%s ($%02X),Y", opn, b[1]);
                break;
            case REL:
            {
                uint16_t ra = b[1];
                if (ra&0x80) ra |= 0xFF00;
                tm->printf("%s $%04X  ", opn, ((addr + nbytes) + ra)&0xFFFF);
            }
                break;
            case ZPX:
                tm->printf("%s $%02X,X  ", opn, b[1]);
                break;
            case ZPY:
                tm->printf("%s $%02X,Y  ", opn, b[1]);
                break;
            case ABSO:
                tm->printf("%s $%04X  ", opn, b[1] + (b[2]<<8));
                break;
            case ABSX:
                tm->printf("%s $%04X,X", opn, b[1] + (b[2] << 8));
                break;
            case ABSY:
                tm->printf("%s $%04X,Y", opn, b[1] + (b[2] << 8));
                break;
            case IND:
                tm->printf("%s ($%04X)", opn, b[1] + (b[2] << 8));
                break;
        }
        addr += nbytes;
        return addr;
    }

    virtual bool input(int c)
    {
        return false;
    }
    virtual void output(TextMatrix* tm)
    {
        uint16_t pc;
        pc = rollPrevious(base,(height/2));
        for (int _y=y; _y<y+height-1; ++_y)
        {
            if (hasFocus())
            {
                tm->setReverse (_y==y+(height/2)) ;
            }
            pc = disassemble(tm,x,_y,LIGHTGREEN,pc);
        }
        tm->setReverse (false) ;
        if (hasFocus()) tm->hideCursor();
    }
};

class Hexview : public Widget
{
public:
    uint16_t base;
    uint8_t columns;
    Hexview(uint8_t _x, uint8_t _y, uint8_t _w, uint8_t _h, uint8_t _cols)
    : Widget(_x,_y,_w,_h)
    , columns(_cols)
    {
    }
    virtual void output(TextMatrix* tm);
    virtual bool input(int c);
};

void Hexview::output(TextMatrix* tm)
{
    tm->setColor(WHITE);
    uint16_t addr = base;
    for (int row=0; row<height; ++row)
    {
        tm->setCursor(x+0,y+row);
        tm->printf("%04X:",addr);
        for (int col=0; col<columns; ++col)
        {
            tm->printf("%02X ",peek(addr+col));
        }
        for (int col=0; col<columns; ++col)
        {
            uint8_t cc = peek(addr+col);
            if (cc<0x10) cc='.';
            tm->printf("%c",cc);
        }
        addr+=columns;
    }
}

bool Hexview::input(int c)
{
    switch(c)
    {
        case VK_DOWN:
        base+=columns;
        return true;
        case VK_UP:
        base-=columns;
        return true;
        case VK_PGDOWN:
        base+=columns*height;
        return true;
        case VK_PGUP:
        base-=columns*height;
        return true;
        case VK_LEFT:
        base++;
        return true;
        case VK_RIGHT:
        base--;
        return true;
    }
    return false;
}

bool iscursor(int c)
{
    switch(c)
    {
        case VK_UP:
        case VK_DOWN:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_DELETE:
        case VK_BACKSPACE:
        case VK_INSERT:
        case VK_RETURN:
            return true;
    }
    return false;
}

bool NumberEdit::input(int c)
{
    if (isxdigit(c) || iscursor(c))
    {
        if (isxdigit(c)) c = toupper(c);
        return TextEdit::input(c);
    }
    return false;
}

bool FilenameEdit::input(int c)
{
    if (isprint(c) || iscursor(c))
    {
        if (c=='/' || c==':' || c=='*' || c=='?' )
        {
            return false;
        }
        return TextEdit::input(c);
    }
    return false;
}

int FilenameEdit::filter(int c)
{
    if (isprint(c))
    {
        return c;
    }
    return -1;
}

class CPUStatus : public Widget
{
public:
    CPUStatus(uint8_t _x, uint8_t _y)
    : Widget(_x,_y,0,0)
    {}
    virtual void output(TextMatrix* tm)
    {
        tm->setColor(GRAY2);
        tm->setCursor(x,y);

        tm->print("PC=");
        tm->printf("%04X",cpu.pc);

        tm->print(" A=");
        tm->setColor(cpu.a!=cpu.last.a?RED:GRAY2);
        tm->printf("%02X",cpu.a);
        tm->setColor(GRAY2);

        tm->print(" X=");
        tm->setColor(cpu.x!=cpu.last.x?RED:GRAY2);
        tm->printf("%02X",cpu.x);
        tm->setColor(GRAY2);

        tm->print(" Y=");
        tm->setColor(cpu.y!=cpu.last.y?RED:GRAY2);
        tm->printf("%02X",cpu.y);
        tm->setColor(GRAY2);

        tm->print(" SP=");
        tm->setColor(cpu.sp!=cpu.last.sp?RED:GRAY2);
        tm->printf("%02X ",cpu.sp);

        tm->setColor(((cpu.status&FLAG_CARRY)!=(cpu.last.status&FLAG_CARRY))?RED:GRAY2);
        tm->print((cpu.status&FLAG_CARRY)?"C":"-");
        tm->setColor(((cpu.status&FLAG_ZERO)!=(cpu.last.status&FLAG_ZERO))?RED:GRAY2);
        tm->print((cpu.status&FLAG_ZERO)?"Z":"-");
        tm->setColor(((cpu.status&FLAG_INTERRUPT)!=(cpu.last.status&FLAG_INTERRUPT))?RED:GRAY2);
        tm->print((cpu.status&FLAG_INTERRUPT)?"I":"-");
        tm->setColor(((cpu.status&FLAG_DECIMAL)!=(cpu.last.status&FLAG_DECIMAL))?RED:GRAY2);
        tm->print((cpu.status&FLAG_DECIMAL)?"D":"-");
        tm->setColor(((cpu.status&FLAG_BREAK)!=(cpu.last.status&FLAG_BREAK))?RED:GRAY2);
        tm->print((cpu.status&FLAG_BREAK)?"B":"-");
        tm->setColor(GRAY2);
        tm->print("1");
        tm->setColor(((cpu.status&FLAG_OVERFLOW)!=(cpu.last.status&FLAG_OVERFLOW))?RED:GRAY2);
        tm->print((cpu.status&FLAG_OVERFLOW)?"O":"-");
        tm->setColor(((cpu.status&FLAG_SIGN)!=(cpu.last.status&FLAG_SIGN))?RED:GRAY2);
        tm->print((cpu.status&FLAG_SIGN)?"S":"-");

    }
};

class GotoDialog : public Dialog
{
public:
    Frame frame,f2;
    Text text,text2;
    Button cancel;
    NumberEdit edit;
    uint32_t value;
    GotoDialog()
    : frame(5,7,30,10)
    , f2(21,9,6,3)
    , text(5+3,7,"GOTO")
    , text2(7,10,"ENTER ADDRESS")
    , cancel(25,13,8,3, "CANCEL")
    , edit(22,10,4)
    {
        add(&frame);
        add(&f2);
        add(&text);
        add(&edit);
        add(&cancel);
        add(&text2);
        edit.setFocus();

        edit.accepted = [&](const std::string&v){
            result = Result::OK;
            value = strtoul(v.c_str(),nullptr,16);
        };

        cancel.clicked = [&](Widget*) {
            result = Result::CANCEL;
        };
    }

    virtual int input()
    {
        int c = Dialog::input();
        if (c==27)
        {
            result = Result::CANCEL;
            return c;
        }
        return -1;
    }

};

class JumpToDialog : public Dialog
{
public:
};

class FillMemoryDialog : public Dialog
{
public:
};


void MnemoEdit::enter()
{
    //read(addr);
    buffer = "";
    cursor=0;
    //cursor  = buffer.size();
}

void MnemoEdit::read(uint16_t _addr)
{
    addr = _addr;
    uint8_t b[3]={0};
    uint8_t opcode = peek(addr);
    uint8_t nbytes = BytesPerAdressMode[AddressmodeTable[opcode]];
    b[0] = opcode;
    if (nbytes>1) b[1] = peek(addr+1);
    if (nbytes>2) b[2] = peek(addr+2);

    char line[16];

    buffer.clear();
    
    const char* opn = OpcodeToString(OpcodeTable[opcode]);
    switch (AddressmodeTable[opcode])
    {
        case IMP: // 1 byte
                            //LDA ($1000),X
            sprintf(line,"%s", opn);
            break;
        case ACC:
            sprintf(line,"%s", opn);
            break;
        case IMM: // 2 byte
            sprintf(line,"%s #$%02X", opn, b[1]);
            break;
        case ZP:
            sprintf(line,"%s $%02X", opn, b[1] );
            break;
        case INDX:
            sprintf(line,"%s ($%02X,X)", opn, b[1]);
            break;
        case INDY:
            sprintf(line,"%s ($%02X),Y", opn, b[1]);
            break;
        case REL:
        {
            uint16_t ra = b[1];
            if (ra&0x80) ra |= 0xFF00;
            sprintf(line,"%s $%04X", opn, ((addr + nbytes) + ra)&0xFFFF);
        }
            break;
        case ZPX:
            sprintf(line,"%s $%02X,X", opn, b[1]);
            break;
        case ZPY:
            sprintf(line,"%s $%02X,Y", opn, b[1]);
            break;
        case ABSO:
            sprintf(line,"%s $%04X", opn, b[1] + (b[2]<<8));
            break;
        case ABSX:
            sprintf(line,"%s $%04X,X", opn, b[1] + (b[2] << 8));
            break;
        case ABSY:
            sprintf(line,"%s $%04X,Y", opn, b[1] + (b[2] << 8));
            break;
        case IND:
            sprintf(line,"%s ($%04X)", opn, b[1] + (b[2] << 8));
            break;
    }
    buffer += line;
    cursor = buffer.size();
}

int MnemoEdit::filter(int c)
{
    switch(c)
    {
    default:
        if (isprint(c))
        {
            return toupper(c);
        }
        break;

    }
    return -1;
}

bool MnemoEdit::input(int c)
{
    bool rc = TextEdit::input(c);

    if (c == 27)
    {
        parent->focusNext();
        return -1;
    }
    
    hasSyntaxError = !scanner.scan(buffer.c_str());

    return rc;
}

void MnemoEdit::output(TextMatrix* tm)
{
    if (!hasFocus()) return;
    color = (hasSyntaxError?RED:YELLOW);
    tm->fill(x,y,width,1,' ');
    TextEdit::output(tm);
}

template <class T>
Listing<T>::Listing(uint8_t _x, uint8_t _y, uint8_t _width, uint8_t _height)
: Widget(_x,_y,_width,_height)
{
    canFocus = true;
    setCursor(-1);
}

template <class T>
void Listing<T>::sort()
{
    std::sort(items.begin(),items.end(),[](const T& a, const T& b){
        return a < b;
    });
}

template <class T>
void Listing<T>::setCursor(int _cursor)
{
    cursor = _cursor;
    if (cursor==-1)
    {
        offset = 0;
    }
    else
    {
        while (cursor<offset) offset--;
        while (cursor>(offset+height-1)) offset++;
    }
}

template <class T>
bool Listing<T>::input(int c)
{
    switch(c)
    {
        case VK_RETURN:
            if (selected!=nullptr)
            {
                selected(this,cursor);
                return true;
            }
            break;
        case VK_PGDOWN:
            if (cursor>=height) setCursor(cursor-height);
            return true;
        break;
        case VK_PGUP:
            if ((cursor<(items.size()-height)))
                setCursor(cursor+height);
                else
                {
                    setCursor(items.size());
                }
            return true;
        break;
        case VK_UP:
            if (cursor>0) setCursor(cursor-1);
            return true;
        case VK_DOWN:
            if (cursor<(items.size()-1)) setCursor(cursor+1);
            return true;
        case VK_HOME:
            setCursor(0);
            return true;
        case VK_END:
            setCursor(items.size()-1);
            return true;
    }
    return false;
}

template <class T>
void Listing<T>::output(TextMatrix* tm)
{
    if (render == nullptr) return;
    tm->setColor(hasFocus() ? YELLOW: GRAY1 );
    typename std::vector<T>::iterator i=items.begin();
    std::advance(i,offset);
    for (int _y=0; _y<height;++_y)
    {
        if (i==items.end()) break;
        tm->setReverse((_y+offset)==cursor);
        tm->setCursor(x,_y+y);
        render(this,tm,*i);
        //tm->print((*i).c_str());
        ++i;
    }
    tm->setReverse(false);
}

template <class T>
void Listing<T>::clear()
{
    items.clear();
    setCursor(-1);
}

template <class T>
void Listing<T>::add(const T& _line)
{
    items.push_back(_line);
    setCursor( items.size()-1 );
}

template <class T>
void Listing<T>::remove(int _index)
{

}
class ProgressDialog : public Dialog
{
public:
    Frame frame;
    Text text;
    ProgressDialog()
    : frame(16,12,10,3)
    , text(17,13,"",LIGHTBLUE)
    {
        add(&frame);
        add(&text);
    }
    std::function<bool(Text&)> action;
    virtual int input()
    {
        if (action(text))
        {
            result = OK;
            return -1;
        }
        int c = Dialog::input();
        if (c==27)
        {
            result = CANCEL;
            return -1;
        }
        return -1;
    }
};

class FileDialog : public Dialog
{
protected:
    Frame frame;
    Text text;
    Listing<std::string> listing;
public:
    FileDialog()
    : frame(3,3,36,21)
    , text(5,3,"FILE DIALOG")
    , listing(5,5,30,17)
    {
        add(&frame);
        add(&text);
        add(&listing);

        listing.render = [&](Listing<std::string>*self,TextMatrix*tm,const std::string& content){
            tm->print(content.c_str());
        };

        listing.setFocus();
        //listing.selected = [this](Widget*w,int index){
        //    this->result = OK;
        //};
        scan();
    }

    void scan()
    {
        File path = LITTLEFS.open("/");

        ProgressDialog dlg;
        dlg.action = [&](Text& txt){

            File entry = path.openNextFile();
            if (!entry) return true;
            char buf[32];
            sprintf(buf,"%-22s %7d",entry.name(),entry.size());
            listing.add(buf);
            entry.close();

            return false;
        };
        dlg.run(this,this->matrix,this->ansi);
       
        path.close();
    }

    virtual int input()
    {
        int c = Dialog::input();

        if (c==VK_F5)
        {
            scan();
            return -1;
        }
        if (c==VK_DELETE)
        {
            listing.clear();
        }
        if (c==VK_INSERT)
        {
            char buf[100];
            sprintf(buf,"entry %d",rand());
            listing.add(buf);
        }
        if (c==27)
        {
            result = CANCEL;
            return -1;
        }

        if (c == -1) return c;

        return -1;
    }
};


class CopyFilesDialog : public Dialog
{
protected:
    Frame frame;
    Text text;
    Progressbar progress1;
    Progressbar progress2;
    Text text1;
    Text text2;
public:
    CopyFilesDialog()
    : frame(3,3,36,17)
    , text(5,3,"Copy files")
    , progress1(5,8,30,0,100,0)
    , progress2(5,12,30,0,100,0)
    , text1(5,9,"")
    , text2(5,13,"")
    {
        add(&frame);
        add(&text);
        add(&progress1);
        add(&progress2);
        add(&text1);
        add(&text2);
    }

    std::vector<std::string>::iterator begin;
    std::vector<std::string>::iterator end;
    std::vector<std::string>::iterator current;
    File source;
    File target;
    size_t totalBytes;
    size_t writtenBytes;
    int drive;
    char label[32];
    void copy(std::vector<std::string>& _filenames,int _drive)
    {
        text2.text = "";
        drive = _drive;
        begin = _filenames.begin();
        end = _filenames.end();
        open( begin );
        progress2.max = _filenames.size();
        progress2.min = 0;
    }

    void open(std::vector<std::string>::iterator iter)
    {
        current = iter;
        if (current == end)
        {
            result = Dialog::Result::OK;
            return;
        }
        //text2.text = "ja";
        std::string sourcename;
        std::string targetname;

        sourcename = Storage::getcwd(drive==8);
        if (*sourcename.rbegin()!='/')
            sourcename += "/";
        sourcename.append(*current);

        targetname = Storage::getcwd(drive!=8);
        if (*targetname.rbegin()!='/')
            targetname += "/";
        targetname.append(*current);

        source = Storage::open(sourcename.c_str(),"r",drive==8);
        target = Storage::open(targetname.c_str(),"w",drive!=8);

        text1.text = (*current).c_str();

        progress2.value++;
        sprintf(label,"%d / %d",progress2.value,progress2.max);
        text2.text = label;
        
        writtenBytes = 0;
        totalBytes = source.size();
        progress1.min = 0;
        progress1.max = totalBytes;
        progress1.value = 0;
    }

    void copy()
    {
        if (source && target)
        {
            char buffer[4096];
            int written = 0;
            int read = source.readBytes(buffer,sizeof(buffer));
            if (read>0)
                written = target.write((const uint8_t*)buffer,read);
            progress1.value += read;
            if (read<=0 || read!=written)
            {
                progress1.min = 0;
                progress1.max = 100;
                progress1.value = 100;
                source.close();
                target.close();
                ++current;
                open(current);
            }
        }
    }

    virtual int input()
    {
        if (current!=end) copy();
        int c = Dialog::input();
        if (c==27)
        {
            result = CANCEL;
            return -1;
        }
        return -1;
    }
};

class CreateDirectoryDialog : public Dialog
{
public:
    Frame frame,f2;
    Text text;
    Button cancel;
    FilenameEdit edit;
    CreateDirectoryDialog()
    : frame(2,7,38,11)
    , f2(5-1,10-1,32+2,3)
    , text(3,7,"Create Directory")
    , cancel(30,13,8,3, "CANCEL")
    , edit(5,10,32)
    {
        add(&frame);
        add(&f2);
        add(&text);
        add(&edit);
        add(&cancel);
        edit.setFocus();

        edit.accepted = [&](const std::string&v){
            result = Result::OK;
        };

        cancel.clicked = [&](Widget*) {
            result = Result::CANCEL;
        };
    }

    virtual int input()
    {
        int c = Dialog::input();
        if (c==27)
        {
            result = Result::CANCEL;
            return c;
        }
        return -1;
    }

};

FileManagerDialog::FileManagerDialog()

    : listing(1,3,40,23)
    , text1(1,1,40)
    , label(1,2,"LittleFS, 0 bytes free",LIGHTGREEN)
{
    add(&listing);
    add(&text1);
    add(&label);

    filepath = "";

    listing.render = [&](Listing<ListEntry>*self,TextMatrix*tm,const ListEntry& le){
        std::string copy(le.name);
        std::replace( copy.begin(), copy.end(), '_', ' '); // replace all 'x' to 'y'
        if (le.directory)
        {
            tm->setColor(le.selected ? ORANGE : GRAY1);
            tm->printf("%-40s",copy.c_str());
        }
        else
        {
            tm->setColor(le.selected ? YELLOW : GRAY3);
            tm->printf("%-33s%7d",copy.c_str(),le.size);
        }
    };
}

void FileManagerDialog::load()
{
    listing.setFocus();
    scan();
}


void FileManagerDialog::output(TextMatrix* tm)
{
    tm->clear(' ');
    tm->setColor(WHITE);

    //tm->setCursor(0,22);
    //tm->print(filepath.c_str());

    Dialog::output(tm);
}

void FileManagerDialog::scan()
{
    listing.clear();
    File path = Storage::open(Storage::getcwd(drive==8),"r",drive==8);
    text1.buffer = Storage::getcwd(drive==8);

    ProgressDialog dlg;
    dlg.action = [&](Text& txt)
    {
        char buf[128];
        //static const uint8_t anim[4] = { 0x7b - 0x40, 0x7e - 0x40,0x7c-0x40,0x6c-0x40 };
        for (int i=0; i<16; ++i)
        {
            File entry = path.openNextFile();
            if (!entry) return true;

            const char* name = rindex(entry.name(),'/');
            if (name==nullptr) name=entry.name(); else name++;
            strcpy(buf,name);
            //if (entry.isDirectory()) strcat(buf,"/");
            ListEntry le;
            le.name = buf;
            le.directory = entry.isDirectory();
            le.selected = false;
            le.size = entry.size();
            listing.add(le);
            entry.close();
            txt.text = String(listing.items.size());
        }

        return false;
    };
    dlg.run(this,this->matrix,this->ansi);
       
    listing.sort();
    listing.setCursor(0);
    path.close();
}

const char* FileManagerDialog::selected() 
{
    if (listing.cursor==-1) return nullptr;

    filepath = Storage::getcwd(drive==8);
    if (*filepath.rbegin()!='/')
        filepath += "/";
    filepath.append(listing.items[listing.cursor].name);

    return filepath.c_str();
}

int FileManagerDialog::input()
{
    int c = Dialog::input();

    switch(c)
    {
        case VK_SPACE:
            listing.items[listing.cursor].selected = !listing.items[listing.cursor].selected;
            break;
        case VK_INSERT:
            listing.items[listing.cursor].selected = !listing.items[listing.cursor].selected;
            if (listing.cursor<(listing.items.size()-1)) listing.setCursor(listing.cursor+1);
            break;

        case 'f': // find
        break;

        case '*': // select by typing
        break;

        case 'a':
        {
            int n_selected = 0;
            for (auto&& item : listing.items) if (item.selected) n_selected++;
            for (auto&& item : listing.items) item.selected = n_selected == 0;
        }
            break;

        case VK_RETURN:
            filepath = listing.items[listing.cursor].name;
            //Serial.printf(">%s<\n",filepath.c_str()); 007
            if (listing.items[listing.cursor].directory)
            {
                std::string buffer = Storage::getcwd(drive==8);
                if (*buffer.rbegin()!='/')
                    buffer += "/";
                buffer.append(filepath);
                Storage::chdir(buffer.c_str(),drive==8);
                //edit.buffer.resize (edit.buffer.size () - 1);
                filepath.clear();
                scan();
            }
            else
            {
                result = OK;
            }
            return -1;
        case VK_BACKSPACE:
            {
                std::string buffer = Storage::getcwd(drive==8);
                auto iter = buffer.rfind('/');
                if (iter != std::string::npos)
                {
                    buffer.erase(iter);
                }
                if (buffer.empty()) buffer="/";
                Storage::chdir(buffer.c_str(),drive==8);
                scan();
            }
            break;
        case VK_DELETE:         // delete selection
        {
            int n_selected = 0;
            for (auto&& item : listing.items) if (item.selected) n_selected++;

            if (n_selected==0)
            {
                if (listing.cursor>=0 && listing.cursor<listing.items.size())
                {
                    listing.items[listing.cursor].selected = true;
                }
            }

            for (auto&& item : listing.items)
            {
                if (!item.selected) continue;
                std::string buffer = Storage::getcwd(drive==8);
                if (*buffer.rbegin()!='/')
                    buffer += "/";
                buffer.append(item.name);

                if (item.directory)
                    Storage::rmdir(buffer.c_str(),drive==8);
                else
                    Storage::remove(buffer.c_str(),drive==8);
            }
            scan();
        
            return true;
        }

        
        case VK_F5:             // create directory
            {
                std::vector<std::string> names;
                for (auto&& item : listing.items)
                {
                    if (!item.selected) continue;
                    names.push_back(item.name);
                }

                CopyFilesDialog dlg;
                dlg.copy(names,drive);
                dlg.run(this,matrix,ansi);

                scan();

            }
            return true;

        case VK_F7:             // copy files
            {
                CreateDirectoryDialog dlg;
                dlg.run(this,this->matrix,this->ansi);
                if (dlg.result == Dialog::Result::OK)
                {
                    std::string buffer = Storage::getcwd(drive==8);
                    if (*buffer.rbegin()!='/')
                        buffer += "/";
                    buffer.append(dlg.edit.buffer);

                    Storage::mkdir(buffer.c_str(),drive==8);
                    scan();
                }
            }
            return true;

    }

    if (c==VK_F1)
    {
        drive=1;
        label.text = "LittleFS ";
        label.text += Storage::freeblocks(false);
        label.text += " blocks free";
        scan();
        return -1;
    }
    if (c==VK_F2)
    {
        drive=8;
        label.text = "SD-Card ";
        label.text += Storage::freeblocks(true);
        label.text += " blocks free";
        scan();
        return -1;
    }


    if (c==27)
    {
        result = CANCEL;
        return -1;
    }

    if (c == -1) return c;

    return -1;
}


class OpenFileDialog : public FileDialog
{
public:
    OpenFileDialog() : FileDialog()
    {
        text.text = "OPEN FILE DIALOG";
    }
};

class SaveFileDialog : public FileDialog
{
public:
    SaveFileDialog()
    : FileDialog()
    {
        text.text = "SAVE FILE DIALOG";
    }
};


class DisassemblerDialog : public Dialog
{
    Frame frame1, frame2;
    Text text;
    Disassembler disass;
    CPUStatus status;
    Hexview hexview;
    MnemoEdit mnemo;
    //Checkbox cb1;
    //Slider sl;
public:
    DisassemblerDialog()
    : frame1(1,3,40,23)
    , frame2(1,1,40,3)
    , text(2,2,"Balstermon",PURPLE)
    , disass(2,4,38,20)
    , status(2,24)
    , hexview(2,4,38,20,8)
    , mnemo(16,14,24)
    //, cb1(13,2,"ANSI")
    //, sl(13,2,10,-5,+5,"Cycle Accuracy")
    {
      add(&frame1);
      add(&frame2);
      add(&disass);
      add(&hexview);
      add(&text);
      add(&status);
      add(&mnemo);
      //add(&cb1);
      //add(&sl);
/*
      cb1.checked = [&](Checkbox*w){
        digitalWrite(33,w->value);
      };
*/
/*
      sl.change(cpu.cyclehack);
      sl.changed = [&](Slider*s){
        cpu.cyclehack = s->value;  
      };
*/
        text.color = ORANGE;

      hexview.enabled = false;
      disass.enabled = true;
      hexview.base = cpu.pc;
      disass.setFocus();
      disass.base = cpu.pc;
      disass.setFocus();
      mnemo.read(disass.base);

      mnemo.accepted = [&](const std::string&){
          disass.base = mnemo.scanner.write(disass.base);
          mnemo.clear();
          //disass.base = mnemo.encode();
      };
    }

    virtual int input()
    {
        int c = Dialog::input();
        if (c == -1) return c;

        switch(c)
        {
            case VK_UP:
                disass.base = disass.rollPrevious(disass.base,1);
                mnemo.read(disass.base);
                break;
            case VK_DOWN:
                disass.base = disass.findNext(disass.base);
                mnemo.read(disass.base);
                break;

            case VK_F9:
            {
                //BreakpointsDialog dlg;
                //dlg.run(this,matrix,ansi);
            }
            break;

            case VK_F2:
            {
                SaveFileDialog dlg;
                dlg.run(this,matrix,ansi);
            }
            break;
            case VK_F7:
                disass.enabled = true;
                mnemo.enabled = true;
                hexview.enabled = false;
                disass.setFocus();

            break;
            case VK_F8:
                disass.enabled = false;
                mnemo.enabled = false;
                hexview.enabled = true;
                hexview.setFocus();

            break;
            case VK_F1:
            {
                FileManagerDialog dlg;
                dlg.run(this,matrix,ansi);
            }
            break;

            case VK_F11:
            {
                cpu.stepIn();
                disass.base = cpu.pc;
                hexview.base = cpu.pc;
            }
            break;
            case 'o':
            {
                cpu.stepOut();
                disass.base = cpu.pc;
                hexview.base = cpu.pc;
            }
            break;
            case VK_F10:
            {
                cpu.stepOver();
                disass.base = cpu.pc;
                hexview.base = cpu.pc;
            }
            break;
            case 'i':
            {
                cpu.irq();
                disass.base = cpu.pc;
                hexview.base = cpu.pc;
            }
            break;
            case '+':
                cpu.addBreakpoint(disass.base);
                break;
            case '-':
                cpu.removeBreakpoint(disass.base);
                break;

            case 'g':
                {
                    GotoDialog dlg;
                    dlg.text.text = "SET CURSOR TO";
                    dlg.run(this,matrix,ansi);
                    if (dlg.result == OK)
                    {
                        disass.base = dlg.value;
                        hexview.base = dlg.value;
                        mnemo.addr = dlg.value;
                    }
                    return true;
                }
                break;
            case 'j':
                {
                    GotoDialog dlg;
                    dlg.text.text = "JUMP TO";
                    dlg.run(this,matrix,ansi);
                    if (dlg.result == OK)
                    {
                        disass.base = dlg.value;
                        hexview.base = dlg.value;
                        mnemo.addr = dlg.value;
                        cpu.pc = dlg.value;
                    }
                    return true;
                }
                break;
            case 27:
                result = Result::OK;
                break;
        }


        return c;
    }
};

void monitor_init()
{
    if (g_matrix == nullptr)
    {
      g_matrix = new TextMatrix();
      g_ansi = new AnsiRenderer();
      g_ansi->uppercase=false;
    }
}

void monitor()
{
    
    DisassemblerDialog().run(nullptr,g_matrix,g_ansi);
}
