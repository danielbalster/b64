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

#include <Arduino.h>
//#include <ArduinoOTA.h>
#include <SPI.h>

#include <LITTLEFS.h>
#include "buffer.h"
#include "driver/sdmmc_types.h"
#include <SD_MMC.h>

#if defined(USEWIFI)
#include <SPIFFSEditor.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(80);
#endif

#include <stdio.h>
#include <errno.h>
#include "config.h"
#include "cia.h"
#include "keyboard.h"
#include "cpu.h"
#include "vic.h"
#include "memory.h"
#include "monitor.h"
#include "ansi.h"
#include "io.h"
#include "textmatrix.h"
#include "reu.h"
#include "storage.h"
#include "directorywriter.h"
#include "sid.h"
#include "note.h"
#include "frame.h"
#include "button.h"
#include "dialog.h"
#include "slider.h"
#include "checkbox.h"
#include "text.h"
#include "help.h"
#include "kernalfile.h"


bool use_ansi = false;
int debug_io = 0;
AnsiRenderer* ansi;
unsigned long last_timestamp;
long delta_accumulator;
unsigned long accurracy = 0;
int frame=0;

// first index is bit 0..2 of processor port
// second index are banks 0xA000, 0xD000 and 0xE000 + 0x8000
// nullptr means I/O mapped
uint8_t* mappings[8][4];
int curbank = -1;
uint32_t clocks_in_irq;
bool count_irq=false;
uint8_t last_curvis = 0;

SPIClass spi = SPIClass(HSPI);


int serialread(uint8_t *buf, int len);
void reset();

uint8_t peek(uint16_t address)
{
  switch(address&0xF000)
  {
    case 0xA000:
    case 0xB000:
      return mapped_basic[address];
    case 0xD000:
    {
      if (unlikely(mapped_io!=nullptr))
      {
        return mapped_io[address];
      }
      else
      {
        switch( address & 0xF00 )
        {
          case 0x000:
          case 0x100:
          case 0x200:
          case 0x300:
            return vic.read(address&0x3f);

          case 0x500:
          case 0x600:
          case 0x700:
          case 0x400:
            return sid.read(address&63);
            
          case 0x800:
          case 0x900:
          case 0xa00:
          case 0xb00:
            return CRAM[address-0xD800];

          case 0xc00:
            return cia1.read(address&15);
            
          case 0xd00:
            return cia2.read(address&15);
            
          case 0xe00:
          break;
          case 0xf00:
            // expansions not supported
            //return reu.read(address&15);
            break;
        }
      }
    }
      break;
    case 0xE000:
    case 0xF000:
      return mapped_kernal[address];
//    case 0x8000:
//    case 0x9000:
//      return mapped_8000[address];
      
    default:
      break;
  }
  return(RAM[address]); //free RAM for user  
}

void poke(uint16_t address, uint8_t value)
{
  // data breakpoint support
  if (cpu.n_breakpoints>0)
  {
    Breakpoint* bp = cpu.findBreakpoint(address);
    if (bp != nullptr)
    {
      monitor();
      refreshscreen(ansi,use_ansi);
    }
  }

  if (unlikely((address&0xF000) == 0xD000))
  {
    if (unlikely(mapped_io!=nullptr))
    {
      mapped_io[address] = value;
    }
    else
    {
      switch( address & 0xF00 )
      {
        case 0x000:
        case 0x100:
        case 0x200:
        case 0x300:
          vic.write(address&0x3f,value);
          break;
        case 0x500:
        case 0x600:
        case 0x700:
            sid2.write(address,value);
          break;
        case 0x400:
            if (address >= 0xd420)
            sid2.write(address&31,value);
            else
            sid.write(address&31,value);
          break;
        case 0x800:
        case 0x900:
        case 0xa00:
        case 0xb00:
          CRAM[address-0xD800] = value;
          break;
        case 0xc00:
          cia1.write(address&15,value);
          break;
        case 0xd00:
          cia2.write(address&15,value);
          break;
        case 0xe00:
          break;
        case 0xf00:
            //reu.write(address&15,value);
          // expansions not yet supported
          break;
      }
    }
    return;
  }
  //if (address<RAMSIZE)
    RAM[address] = value;

    if (unlikely(address==1))
    {
      processorport = value;
      int index = (value & 7);
      mapped_basic = mappings[index][0];
      mapped_io = mappings[index][1];
      mapped_kernal = mappings[index][2];
      mapped_8000 = mappings[index][3];
    }
    //else if (unlikely(value==0xFF00))
    //{
    //  reu.latch(value);
    //}
}

// saves some cycles in read and write to memory, pre-substraction done here

void init_mappings()
{
  // 000 CHAR=0 HIRAM=0 LORAM=0
  mappings[0][0] = &RAM[0xA000] - 0xA000;
  mappings[0][1] = &RAM[0xD000] - 0xD000;
  mappings[0][2] = &RAM[0xE000] - 0xE000;
  mappings[0][3] = &RAM[0x8000] - 0x8000;

  // 001 CHAR=0 HIRAM=0 LORAM=1
  mappings[1][0] = &RAM[0xA000] - 0xA000;
  mappings[1][1] = (uint8_t*)chargen - 0xD000;
  mappings[1][2] = &RAM[0xE000] - 0xE000;
  mappings[1][3] = &RAM[0x8000] - 0x8000;

  // 010 CHAR=0 HIRAM=1 LORAM=0
  mappings[2][0] = &RAM[0xA000] - 0xA000;
  mappings[2][1] = (uint8_t*)chargen - 0xD000;
  mappings[2][2] = (uint8_t*)kernal - 0xE000;
  mappings[2][3] = &RAM[0x8000] - 0x8000;

  // 011 CHAR=0 HIRAM=1 LORAM=1
  mappings[3][0] = (uint8_t*)basic - 0xA000;
  mappings[3][1] = (uint8_t*)chargen - 0xD000;
  mappings[3][2] = (uint8_t*)kernal - 0xE000;
  mappings[3][3] = &RAM[0x8000] - 0x8000;

  // 100 CHAR=1 HIRAM=0 LORAM=0
  mappings[4][0] = &RAM[0xA000] - 0xA000;
  mappings[4][1] = &RAM[0xD000] - 0xD000;
  mappings[4][2] = &RAM[0xE000] - 0xE000;
  mappings[4][3] = &RAM[0x8000] - 0x8000;

  // 101 CHAR=1 HIRAM=0 LORAM=1
  mappings[5][0] = &RAM[0xA000] - 0xA000;
  mappings[5][1] = nullptr;
  mappings[5][2] = &RAM[0xE000] - 0xE000;
  mappings[5][3] = &RAM[0x8000] - 0x8000;

  // 110 CHAR=1 HIRAM=1 LORAM=0
  mappings[6][0] = &RAM[0xA000] - 0xA000;
  mappings[6][1] = nullptr;
  mappings[6][2] = (uint8_t*)kernal - 0xE000;
  mappings[6][3] = &RAM[0x8000] - 0x8000;

  // 111 CHAR=1 HIRAM=1 LORAM=1
  mappings[7][0] = (uint8_t*)basic - 0xA000;
  mappings[7][1] = nullptr;
  mappings[7][2] = (uint8_t*)kernal - 0xE000;
  mappings[7][3] = &RAM[0x8000] - 0x8000;
}

void memorymap()
{
  // VIC bank
  // 00 = $c000-$ffff
  // 01 = $8000-$bfff
  // 10 = $4000-$7fff
  // 11 = $0000-$3fff
  int bank = (~cia2.read(0)) & 3;
  vic.base = bank * 0x4000;
  if (curbank != bank)
  {
    curbank=bank;
  }

  byte* matrix = RAM + (vic.base + ((vic.mem & 0xF0) << 6   ) );
  byte* bitmap = RAM + (vic.base + ((vic.mem & 0x08) * 0x400) ); // 8KB steps

  byte p = vic.mem & 0x0e;
  uint16_t charoffset = (vic.mem & 0xe) << 10;

  byte* cgen = RAM + vic.base + charoffset;
  if (bank==0 /*|| bank==2*/)
    if (p==4 || p==6)
      cgen = (uint8_t*)chargen-0x1000 + charoffset;

  delayed_chargen = cgen;
  delayed_matrix = matrix;
  delayed_bitmap = bitmap;
}

void reset()
{
  Storage::chdir("/",false);
  Storage::chdir("/",true);

  cia1.reset();
  cia2.reset();
  vic.reset();
  kbd.reset();
  sid.reset();
  sid2.reset();
  //reu.reset();
  cpu.reset();

  memorymap(); // initialize memory mapping

  // fast-forward simulate until the dot of ready appears. 
  #if defined(ORIGINAL_KERNAL)
  RAM[1229]=' ';
  while (RAM[1229]!='.') cpu.fastclock();
  #endif
  for (int i=0; i<20000; ++i) cpu.fastclock();

  refreshscreen(ansi,use_ansi);
}


#define KERNAL_PRINT_MESSAGE 0xF12B
#define KERNAL_TOO_MANY_FILES 0xF6FB 
#define KERNAL_FILE_OPEN 0xF6FE       // file already open
#define KERNAL_FILE_NOT_OPEN 0xF701
#define KERNAL_FILE_NOT_FOUND 0xF704
#define KERNAL_DEVICE_NOT_PRESENT 0xF707
#define KERNAL_NOT_INPUT_FILE 0xF70A
#define KERNAL_NOT_OUTPUT_FILE 0xF70D
#define KERNAL_FILE_NAME_MISSING 0xF710
#define KERNAL_ILLEGAL_DEVICE_NO 0xF713

#define ZP_ERROR_DISPLAY RAM[0x9D]
#define ZP_ST RAM[0x90]
#define ZP_NUMFILESOPEN RAM[0x98]
#define ZP_CURINPUTDEVICE RAM[0x99]
#define ZP_CUROUTPUTDEVICE RAM[0x9A]
#define ZP_FILENO RAM[0xB8]
#define ZP_SECONDARYADDRESS RAM[0xB9]
#define ZP_DEVNO RAM[0xBA]
#define ZP_QUOTATIONMODE RAM[0xD4]
#define ZP_CURSORVISIBILITY RAM[0xCC]
#define ZP_CURRENTCOLOR RAM[646]
#define ZP_KEYBOARDBUFFERLENGTH RAM[198]
#define ZP_BASICSTART ((RAM[0x2C]<<8) + RAM[0x2B])
#define ZP_CURFILENAME ((RAM[0xBC]<<8) + RAM[0xBB])
#define ZP_CURFILENAMELENGTH RAM[0xb7]

#define ZP_FILENO_T(_i) RAM[0x259+(_i)]
#define ZP_SECADR_T(_i) RAM[0x26D+(_i)]
#define ZP_DEVNO_T(_i) RAM[0x263+(_i)]

#define FLAG_SHOW_IO_ERROR_MESSAGES 64
#define FLAG_SHOW_SYSTEM_MESSAGES   128

static const int editmode(int value, const char* escapesequence, CbmKeyType key, CbmKeyType mod)
{
  int k=value;
  if (use_ansi) {
    if (ZP_QUOTATIONMODE==0) {
      Serial.print(escapesequence);
    } else {
      Serial.write(k);
    }
  }else{
    kbd.type(key,(CbmKeyType)mod);
    k=-1;
  }
  return k;
}

class ConfigDialog : public Dialog
{
  Frame f1;
  Text t1;
  Checkbox cb_ansi;
  Slider sl_cycle;
public:
  ConfigDialog()
  : Dialog()
  , f1(1,1,40,25)
  , t1(4,1,"OPTIONS",LIGHTBLUE)
  , cb_ansi(4,4,"ANSI")
  , sl_cycle(4,8,10,-5,+5,"CYCLE ACCURRACY")
  {
    add(&f1);add(&t1);add(&cb_ansi);add(&sl_cycle);

    cb_ansi.value = use_ansi;
    cb_ansi.checked = [](Checkbox*cb)
    {
      use_ansi = cb->value;
    };

    sl_cycle.value = cpu.cyclehack;
    sl_cycle.changed = [](Slider*sl)
    {
      cpu.cyclehack = sl->value;
    };
  }

  int input() override
  {
    int c = Dialog::input();
    if (c == -1) return c;

    if (c==27)
    {
        result = Result::CANCEL;
        return true;
    }

    return c;
  }
};

void input()
{
  kbd.processKey();

  // hide cursor
  if (use_ansi)
  {
    if (last_curvis != ZP_CURSORVISIBILITY)
    {
      last_curvis = ZP_CURSORVISIBILITY;
      if (ZP_CURSORVISIBILITY==1)
        AnsiRenderer::hideCursor();
      else
        AnsiRenderer::showCursor();
    }
  }

  if (Serial.available()<=0) return;
  int k = Ansi::read(Serial);
  if (k==-1) return;
  switch(k)
  {
    case VK_HOME: k=editmode(19,"\e[0;0H",CBM_KEY_CLEARHOME,CBM_KEY_LSHIFT); break;// home
    case VK_F1: kbd.type(CBM_KEY_F1F2); return; // f1
    case VK_F2: kbd.type(CBM_KEY_F1F2,CBM_KEY_LSHIFT);return; // f2
    case VK_F3: kbd.type(CBM_KEY_F3F4); return; // f3
    case VK_F4: kbd.type(CBM_KEY_F3F4,CBM_KEY_LSHIFT); return; // f4
    case VK_F5: kbd.type(CBM_KEY_F5F6);  return; // f5
    case VK_F6: kbd.type(CBM_KEY_F5F6,CBM_KEY_LSHIFT);  return; // f6
    case VK_F7: kbd.type(CBM_KEY_F7F8);  return; // f7
    case VK_F8: kbd.type(CBM_KEY_F7F8,CBM_KEY_LSHIFT); return; // f8
    case VK_DELETE: k=editmode(20,"\e[1D\e[1P",CBM_KEY_INSDEL,CBM_KEY_NULL); break; // del
    case VK_END: k=editmode(147,"\e[2J\e[0;0H",CBM_KEY_CLEARHOME,CBM_KEY_NULL); break; // clr
    case VK_INSERT: k=editmode(148,"\e[1@",CBM_KEY_INSDEL,CBM_KEY_LSHIFT);break; // inst
    case VK_PGUP: k=142; break; // pgup
    case VK_PGDOWN: k=14; break; // pgdown

    case VK_DOWN: // cursor down
        k=editmode(17,"\e[1B",CBM_KEY_UPDOWN,CBM_KEY_NULL);
    break;
    case VK_UP: // cursor up
        k=editmode(145,"\e[1A",CBM_KEY_UPDOWN,CBM_KEY_LSHIFT);
    break;
    
    case VK_LEFT: // cursor left
        k=editmode(157,"\e[1D",CBM_KEY_LEFTRIGHT,CBM_KEY_LSHIFT);
    break;

    case VK_RIGHT: // cursor right
        k=editmode(29,"\e[1C",CBM_KEY_LEFTRIGHT,CBM_KEY_NULL);
    break;

      case 27:
      {
        k=-1;
        kbd.type(CBM_KEY_RUNSTOP);        
      }
      break;
      case 18: // CTRL-R
      {
        reset();
        return;
      }
      case VK_F10:
      {
        FileManagerDialog dlg;
        TextMatrix tm;
        dlg.drive = ZP_DEVNO;
        dlg.filepath = Storage::getcwd(ZP_DEVNO==8);

        dlg.run(nullptr,&tm,ansi);
        refreshscreen(ansi,use_ansi);
        if (dlg.result==Dialog::Result::OK)
        {
        }
      }
      return;
      case VK_F9:
      {
        ConfigDialog dlg;
        TextMatrix tm;
        dlg.run(nullptr,&tm,ansi);
        refreshscreen(ansi,use_ansi);
        if (dlg.result==Dialog::Result::OK)
        {
        }
      }
      return;
      case VK_F12:
        monitor();
        refreshscreen(ansi,use_ansi);
        return;
      case 2: // CTRL-B
      {
      }
      break;
      case 9: // TAB KEY
      {
        kbd.pressKey(CBM_KEY_RUNSTOP);
        cpu.nmi();  // restore
        for (int i=0; i<1000; ++i) cpu.fastclock();
        kbd.releaseKey(CBM_KEY_RUNSTOP);
      }
      return;
      case 7: // CTRL-G
        vic.regs[0x18] ^= 2;
        refreshscreen(ansi,use_ansi);
      return;
      case 8: // CTRL-H
        use_ansi=!use_ansi;
        refreshscreen(ansi,use_ansi);
      return;// intended fall-through

      case 4: // ctrl-d refresh text screen
      {
        refreshscreen(ansi,use_ansi);
      }
      return;

      case 3: // CTRL-C
      {
        //Serial.println("HELP: CTRL-R = RESET");
        while (Serial.available()==0);
        k = Serial.read();
        switch(k)
        {
        case 'd': debug_io = !debug_io; return;
        case '1': k=editmode(144,Ansi::Foreground[0],CBM_KEY_1,CBM_KEY_CONTROL); break;
        case '!': k=editmode(129,Ansi::Foreground[8],CBM_KEY_1,CBM_KEY_COMMODORE); break;
        case '2': k=editmode(  5,Ansi::Foreground[1],CBM_KEY_2,CBM_KEY_CONTROL); break;
        case '"': k=editmode(149,Ansi::Foreground[9],CBM_KEY_2,CBM_KEY_COMMODORE); break;
        case '3': k=editmode( 28,Ansi::Foreground[2],CBM_KEY_3,CBM_KEY_CONTROL); break;
        case '4': k=editmode(159,Ansi::Foreground[3],CBM_KEY_4,CBM_KEY_CONTROL); break;
        case '$': k=editmode(151,Ansi::Foreground[11],CBM_KEY_4,CBM_KEY_COMMODORE); break;
        case '5': k=editmode(156,Ansi::Foreground[4],CBM_KEY_5,CBM_KEY_CONTROL); break;
        case '%': k=editmode(152,Ansi::Foreground[12],CBM_KEY_5,CBM_KEY_COMMODORE); break;
        case '6': k=editmode( 30,Ansi::Foreground[5],CBM_KEY_6,CBM_KEY_CONTROL); break;
        case '&': k=editmode(153,Ansi::Foreground[13],CBM_KEY_6,CBM_KEY_COMMODORE); break;
        case '7': k=editmode( 31,Ansi::Foreground[6],CBM_KEY_7,CBM_KEY_CONTROL); break;
        case '/': k=editmode(154,Ansi::Foreground[14],CBM_KEY_7,CBM_KEY_COMMODORE); break;
        case '8': k=editmode(158,Ansi::Foreground[7],CBM_KEY_8,CBM_KEY_CONTROL); break;
        case '(': k=editmode(155,Ansi::Foreground[15],CBM_KEY_8,CBM_KEY_COMMODORE); break;
        case '9': k=editmode( 18,  "\e[7m",CBM_KEY_9,CBM_KEY_CONTROL);  break;
        case 'p': k=222; break; // pi
        case 'l': k=92; break; // pound
        case '0': k=146;
          if (use_ansi)
          {
            if (ZP_QUOTATIONMODE==0) {
              Serial.print("\e[0m"); Serial.print(Ansi::Background[peek(0xD021)&15]); Serial.print(Ansi::Foreground[ZP_CURRENTCOLOR&15]); 
            } else Serial.write((uint8_t)k);
          }
        break;
        case 194:
        {
          k=Serial.read();
          if (k==167)
          {
            k=editmode(150,Ansi::Foreground[10],CBM_KEY_3,CBM_KEY_COMMODORE); break;
          }
        }
          break;
        default:
          Serial.printf("\n\n\n%d %d\n\n\n",k,Serial.read());
          break;

        }
      }
      if (use_ansi) return;
      break;

      //case 32:
      //  kbd.push(7,4,1);
      //  kbd.push(7,4,0);
      //  return;
      //case 'z':
        //kbd.push(1,4,1);
        //kbd.push(1,4,0);
        //return;

      case 10: k=13;
        //if (!use_ansi)
        {
          kbd.press(CBM_KEY_RETURN);
          kbd.release(CBM_KEY_RETURN,2);
          return;
        }
        //break;
      case ' ':
        //if (!use_ansi)
        {
          kbd.press(CBM_KEY_SPACE);
          kbd.release(CBM_KEY_SPACE,2);
          return;
        }
         // break;
      case 127: k=editmode(20,"\e[1D\e[1P",CBM_KEY_INSDEL,CBM_KEY_NULL);// backspace
        break; 
      case 126: // ~??
        k=-1;
        break;
      default:
        if (k>='a' && k<='z')
          k -= 32;
        else if (k>='A' && k<='Z')
          k += 32;

        if (use_ansi)
          Serial.write((uint8_t)k);

        break;
    }

  if (k!=-1)
  {
    // Keyboard buffer
    RAM[631+ZP_KEYBOARDBUFFERLENGTH]=k;
    ZP_KEYBOARDBUFFERLENGTH=ZP_KEYBOARDBUFFERLENGTH+1;
  }
  
}

extern "C" void hijacked_load(const char* label);
extern "C" void hijacked_open(const char* label);
extern "C" void hijacked_save(const char* label);
extern "C" void hijacked_setnam(const char*label);
extern "C" void hijacked_chkin(const char* label);
extern "C" void hijacked_chkout(const char* label);
extern "C" void hijacked_chrin(const char* label);
extern "C" void hijacked_chrout(const char* label);
extern "C" void hijacked_close(const char* label);
extern "C" void hijacked_clrchn(const char* label);
extern "C" void hijacked_clall(const char* label);
extern "C" void hijacked_iecin(const char* label);
extern "C" void hijacked_iecout(const char* label);
extern "C" void hijacked_lstnsa(const char* label);
extern "C" void hijacked_talksa(const char* label);
extern "C" void hijacked_untalk(const char* label);
extern "C" void hijacked_unlstn(const char* label);
extern "C" void hijacked_listen(const char* label);
extern "C" void hijacked_talk(const char* label);

struct CartridgeHeader
{
  char signature[16]; // "C64  CARTRIDGE"
  uint32_t headerLength; // 64
  uint16_t version; 
  uint16_t type;
  uint8_t exrom;
  uint8_t game;
  uint8_t reserved[6];
  char cartname[32];
};

struct CartridgeContent
{
  char chip[4]; // CHIP
  uint32_t length;
  uint16_t chiptype;
  uint16_t banknumber;
  uint16_t loadaddr;
  uint16_t romsize;
};

static int petcii_getstring(uint16_t addr,uint8_t len, char *buf)
{
    int i=0;
    for (i=0; i<len; ++i)
    {
      char ch = peek(addr+i);
      buf[i]=ch;
    }
    buf[i]=0;
    return i;
}

// can't remember where I took this from
bool match(const char* _s, const char* _p, int _i=0, int _c=0)
{
  if (_p[_i] == '\0' )
  {
    return (_s[_c] == '\0' || _s[_c] ==0xA0);
  }
  else if (_p[_i] == '*')
  {
    for (; (_s[_c] != '\0' && _s[_c] !=0xA0); _c++)
    {
      if (match(_s,_p,  _i+1, _c))
        return true;
    }
    return match(_s, _p, _i+1, _c);
  }
  else if (_p[_i] != '?' && _p[_i] != _s[_c])
  {
    return false;
  }
  else
  {
    return match(_s,_p,_i+1,_c+1);
  }
}

// idea is to have a small buffer and pre-generated content
// this object intercepts all commands and will deliver the content.
// it´s a small state machine that knows about command and data phases
class IecCommand
{
public:
  uint8_t buffer[256];
  int write;
  int read;

/*
  new command
   N0:diskname,id

  init command (reads bam)
  I:

  rename command
  R0:new name=old name

  copy/merge command
  C0:BACKUP=0:ORIGINAL[,0:FILE, ...]

  scratch command
  S0:filename*

  validate command
  V0
  ---------------------------
  '#' = direct commands
   block read: "U1:channel,drive,track,sector"

open 15 command channel
open 2  data channel
do
close 2
close 15

U1 block read
B-P
U2 block write
M-R memory read
M-W memory write
B-A block allocate
B-F block free
M-E memory execute
B-E block execute







*/

  void open(char* command)
  {
    Serial.printf("\e]0; iec open %s \007",command);

    write = 0;
    read = 0;
    // 73,"CBM DOS V2.6 1541",00,00
//    write += sprintf((char*)buffer,"73,CBM DOS V2.6 1541,00,00");
    write += sprintf((char*)buffer,"73,VIRTUAL DRIVE EMULATION V2.2,00,00");
  }

  void close()
  {
    Serial.printf("\e]0; iec close \007");
  }

  void writeByte( uint8_t b )
  {
    Serial.printf("\e]0; iec %s \007",buffer);
    buffer[write] = b;
    write++;

  }

  int readByte()
  {
    if (read<write) return buffer[read++];
    return -1;
  }
};
IecCommand iec;

// parameter-less API, data is exchanged with C64 kernal directly
class VirtualFileSystem
{
public:
  virtual bool load() = 0;
};

class VirtualDevice
{
public:
  virtual bool open() = 0;
  virtual bool close() = 0;
  virtual bool read() = 0;
  virtual bool write() = 0;
};

class T64Handler// : public VirtualFileSystem
{
  File file;
  uint8_t header[32];
public:

  void mount(File _file)
  {
    file = _file;
    file.seek(32);
    file.read(header,32);
  }

  bool isMounted() const
  {
    return file;
  }

  void unmount()
  {
    file.close();
  }
  void directory()
  {
    uint16_t txttab = ZP_BASICSTART;
    Buffer buffer(RAM+txttab,0xA000-txttab);
    DirectoryWriter dw(&buffer);
    int numentries = (header[5] << 8) + header[4];

    dw.begin((char*)header+8,txttab);
    file.seek(64);
    for (int i=0; i<numentries; ++i)
    {
      uint8_t bytes[32];
      file.read(bytes,32);
      int org = (bytes[3] << 8) + bytes[2];
      int end = (bytes[5] << 8) + bytes[4];
      //int offset = (bytes[o + 11]<<24) + (bytes[o + 10] << 16) + (bytes[o + 9] << 8) + (bytes[o + 8]);
      dw.add((end-org)/256,(const char*)bytes+16,2);
    }
    dw.end(0);
  }

  bool load()
  {
    char str[64];
    petcii_getstring(ZP_CURFILENAME,ZP_CURFILENAMELENGTH,str);

    if (str[0]=='$')
    {
      directory();
    }
    else if (str[0]==0) // just "LOAD" will unload
    {
      unmount();
    }
    else
    {
      if (true)
      {
        file.seek(64);
        uint8_t bytes[32];
        file.read(bytes,32);
        int org = (bytes[3] << 8) + bytes[2];
        int end = (bytes[5] << 8) + bytes[4];
        int offset = (bytes[11]<<24) + (bytes[10] << 16) + (bytes[9] << 8) + (bytes[8]);

        uint16_t addr = org;
        uint16_t size = end-org;

        file.seek(offset);
        file.read(RAM+addr,size);

        //  memcpy(dest,block+skip,blocksize);
        //  size += blocksize;
        RAM[0xAF] = (addr + size) & 0xff;
        RAM[0xAE] = (addr + size) >> 8;

        cpu.y = 0x49;    // "LOADING"
        cpu.pc = KERNAL_PRINT_MESSAGE; // Check direct mode, print and return with carry cleared
        return true;
      }
      else
      {
        cpu.pc = KERNAL_FILE_NOT_FOUND;
        return false;
      }
    }

    ZP_ST = 0x00;
    cpu.status &=~ FLAG_CARRY;
    cpu.pc = 0xEE83; // rts

    return true;
  }
};

class RootHandler : public VirtualFileSystem
{
public:
};

class D64Handler : public VirtualFileSystem
{
public:
  File file;
  void reset();
  uint8_t bam[256];
  uint8_t block[256];
  struct {
    int offset; // for readbyte, writebyte
    int track;
    int sector;
    int blocks;
    int count;
  } stream;
  int numSectors;
  int lookupTrackOffset[40];

  static const int sectorsPerTrack(int track)
  {
      if (track < 18) return 21;
      if (track < 25) return 19;
      if (track < 31) return 18;
      return 17;
  }

  void getSector(int track, int sector, uint8_t *_block)
  {
    uint32_t offset = lookupTrackOffset[track - 1] + sector;
    offset *= 256;
    file.seek(offset);
    file.read(_block,256);
  }

  bool isBlockUsed(int track, int sector)
  {
      int offset = (track * 4) + (sector / 8);
      //int used = bam[offset];
      int bits = bam[offset+1];
      int mask = 1 << (sector & 7);
      return (bits & mask) != mask;
  }

  void mount(File _file)
  {
    file = _file;
    numSectors = file.size() / 256;
    int offset = 0;
    for (int i=0; i<40; ++i)
    {
        lookupTrackOffset[i] = offset;
        offset += sectorsPerTrack(i + 1);
    }
    
    getSector(18,0,bam);
  }

  void unmount()
  {
    file.close();
  }

  bool isMounted() const
  {
    return file;
  }
  
  bool read()
  {
    return false;
  }
  bool write()
  {
    return false;
  }

  bool open()
  {
    char str[64];
    petcii_getstring(ZP_CURFILENAME,ZP_CURFILENAMELENGTH,str);

    if (str[0]==0)
    {
      // empty string
      return true;
    }

    if (ZP_SECONDARYADDRESS==15)
    {
      iec.open(str);
      return true;
    }

    char* name = str;
    if (name[0]=='@') name+=2;

    if (search(name,&stream.track,&stream.sector,&stream.blocks))
    {
      stream.offset = 253;
      stream.count = 0;
      return true;
    }
    return false;
  }

  bool close()
  {
    return false;
  }

  int readbyte()
  {
    stream.offset++;
    if (stream.offset==254)
    {
      stream.offset--;
      if (stream.track==0) return -1; // EOF
      stream.offset = 0;
      getSector(stream.track,stream.sector,block);
      stream.count++;
      stream.track = block[0];
      stream.sector = block[1];
    }
    return block[2+stream.offset];
  }

  int writebyte()
  {
    return -1;
  }

  bool search(const char* name,int* track, int* sector, int* blocks)
  {
    for (int s=1; s<18; ++s)
    {
      getSector(18,s,block);
      if (!isBlockUsed(18, s)) break;
      for (int i=0; i<256; i+=0x20)
      {
          if (block[i + 4] == 0 && block[i + 3] == 0) break;
          //for (int k=i+5; k<i+5+16; ++k)
          //{
          //  if (block[k]==0xA0) block[k]=0;
          //}

          if (match((const char*)block+i+5,name))
          {
            if ((block[i+2]&15)!=2) continue; // only PRG supported
            *track = block[i + 3];
            *sector = block[i + 4];
            *blocks = block[i + 0x1e] + (block[i + 0x1f] * 256);
            return true;
          }
          //e.filetype = d[i + 2];
      }
      if (block[0] == 0 && block[1] == 0) break;
    }
    return false;
  }

  void directory()
  {
    uint16_t txttab = ZP_BASICSTART;
    Buffer buffer(RAM+txttab,0xA000-txttab);
    DirectoryWriter dw(&buffer);
    dw.begin((char*)bam+0x90,txttab);
    int freeblocks = numSectors - 1 - 18; // bam and directory
    int s=1;
    int t=18;
    while (t!=0)
    {
      getSector(t,s,block);
      if (!isBlockUsed(18, s)) break;
      t=block[0];
      s=block[1];
      for (int i=0; i<256; i+=0x20)
      {
          if (block[i + 4] == 0 && block[i + 3] == 0) break;
          int blocks = block[i + 0x1e] + (block[i + 0x1f] * 256);
          freeblocks -= blocks;
          dw.add(blocks,(const char*)block+i+5,block[i + 2]);
      }
    }
    dw.end(freeblocks);

  }

  bool save()
  {
    cpu.pc = KERNAL_FILE_NOT_FOUND;
    return false;
  }

  bool load()
  {
    char str[64];
    petcii_getstring(ZP_CURFILENAME,ZP_CURFILENAMELENGTH,str);

    if (str[0]=='$')
    {
      directory();
    }
    else if (str[0]==0) // just "LOAD" will unload
    {
      unmount();
    }
    else
    {
      int t,s,blocks;
      if (search(str,&t,&s,&blocks))
      {
        uint8_t *dest = 0;
        uint16_t addr = 0;
        uint32_t size = 0;
        int skip = 2;
        int blocksize = 254;

        while (t!=0)
        {
          getSector(t,s,block);
          t = block[0];
          s = block[1];

          if (addr==0)
          {
            if (ZP_SECONDARYADDRESS>0 || (ZP_DEVNO==1))
            {
              skip += 2;
              blocksize -= 2;
              addr = block[3] * 256 + block[2];
            }
            else
            {
              addr = (cpu.y * 256) + cpu.x;
            }
            dest = &RAM[addr];
          }

          memcpy(dest,block+skip,blocksize);
          size += blocksize;
          dest += blocksize;
          skip=2;
          blocksize=254;
        }

        RAM[0xAF] = (addr + size) & 0xff;
        RAM[0xAE] = (addr + size) >> 8;

        cpu.y = 0x49;    // "LOADING"
        cpu.pc = KERNAL_PRINT_MESSAGE; // Check direct mode, print and return with carry cleared
        return true;
      }
      else
      {
        cpu.pc = KERNAL_FILE_NOT_FOUND;
        return false;
      }
    }

    ZP_ST = 0x00;
    cpu.status &=~ FLAG_CARRY;
    cpu.pc = 0xEE83; // rts

    return true;
  }
};

// devices 8,9,10 and 11
VirtualDevice*  device[4];

class VirtualFile
{
  uint8_t fileno;
  uint8_t secondaryAddress;
  uint8_t device;
};

D64Handler d64;
T64Handler t64;

struct SIDPlay
{
  #define FMT_PSID 0x50534944 
  #define FMT_RSID 0x52534944 
  uint32_t magic;
  uint16_t version; // 1, 2, 3, 4
  uint16_t dataOffset;
  uint16_t loadAddr;
  uint16_t initAddr;
  uint16_t playAddr;
  uint16_t songs;
  uint16_t startSong;
  uint32_t speed;
  uint8_t name[32];
  uint8_t author[32];
  uint8_t copyright[32];
  uint16_t flags;           // 0x76
  uint8_t startPage;        // 0x78
  uint8_t pageLength;       // 0x79
  uint8_t secondSIDAddress; // 0x7a
  uint8_t thirdSIDAddress;  // 0x7b

};

class PathHelper
{
  char fullname[256];
  char pathname[256];
  char filename[256];
public:
  const char* path() const
  {
    return pathname;
  }
  const char* full() const
  {
    return fullname;
  }
  const char* name() const
  {
    return filename;
  }

  void setFilename(const char* _filename)
  {
    if (_filename==nullptr) return;
    strcpy(pathname,_filename);
    char* p = rindex(pathname,'/');
    if (p == pathname)
    {
      strcpy(fullname,_filename);
      *p=0;
      strcpy(filename,p+1);
    }
    else
    {
      *p=0;
      strcpy(filename,p+1);
      strcpy(fullname,pathname);
      if (fullname[strlen(fullname)-1]!='/') strcat(fullname,"/");
      strcat(fullname,filename);
    }
  }

  void changeDir(const char* _folder = nullptr)
  {
    if (_folder==nullptr)
    {
      strcpy(pathname,"/");
      fullname[0] = 0;
      filename[0] = 0;
    }
    else if (_folder[0]=='/')
    {
      strcpy(pathname,_folder);
      char* p = rindex(pathname,'/');
      if (p == pathname)
      {
        fullname[0] = 0;
        filename[0] = 0;
      }
      else
      {
        *p=0;
        strcpy(filename,p+1);
        strcpy(fullname,pathname);
        if (fullname[strlen(fullname)-1]!='/') strcat(fullname,"/");
        strcat(fullname,filename);
      }
    }
    else
    {
      strcpy(filename,_folder);
      strcpy(fullname,pathname);
      if (fullname[strlen(fullname)-1]!='/') strcat(fullname,"/");
      strcat(fullname,filename);
    }
  }
};
PathHelper pathname;

enum Tapedeck
{
  STOPPED,
  PLAYING,
  PAUSED,
  FFWD,
};

class Playlist
{
  struct PlayListEntry
  {
    PlayListEntry()
    {

    }
    PlayListEntry(const String& _filename, int _seconds = 10, int _drive = 1)
    :filename(_filename), seconds(_seconds), drive(_drive)
    {
    }
    String filename;
    int seconds;
    int drive;
    int song;
  };
  typedef std::vector<PlayListEntry> PlayList;
  PlayList entries;
  PlayList::iterator iter;
public:
  void load()
  {
    File f = Storage::open("/playlist.txt","r",false);
    if (f)
    {
      while (f.available()>0)
      {
        // filename,drive,song,minutes:seconds
        PlayListEntry e;
        String line = f.readStringUntil('\n');
        char* copy = strdup(line.c_str());
        char* rest = copy;
        char* token = strtok_r(rest,",",&rest);
        if (token)
        {
          e.filename = token;
          token = strtok_r(rest,",",&rest);
          if (token)
          {
            e.drive = atoi(token);
            token = strtok_r(rest,",",&rest);
            if (token)
            {
              e.song = atoi(token);
              token = strtok_r(rest,":",&rest);
              if (token)
              {
                e.seconds = atoi(token) * 60;
                token = strtok_r(rest,":",&rest);
                if (token)
                {
                  e.seconds += atoi(token);
                  entries.push_back(e);
                }
              }
            }
          }
        }
        free(copy);
      }
      f.close();
    }
    else
    {
      entries.push_back(PlayListEntry("/dummy.sid",1,8));
    }

  std::random_shuffle(entries.begin(),entries.end());

    iter = entries.begin();
  }
  void save()
  {
    /*
    File f = Storage::open("/playlist.txt","w",false);
    if (f)
    {
      for (auto& e : entries)
      {
        f.print(e.filename.c_str());
        f.print(";");
        f.write(e.seconds);
        f.print(";");
        f.write(e.drive);
        f.println();
      }
      f.close();
    }
    */
  }
  PlayListEntry cache;
  void add(const PlayListEntry& e)
  {
    entries.push_back(e);
  }
  const PlayListEntry& current()
  {
    return (*iter);
  }
  void next()
  {
    ++iter;
    if (iter == entries.end()) iter=entries.begin();
  }
};

class PlayerWidget : public Widget
{
    int cursor;
    Tapedeck control;
    Playlist playlist;
public:
    SIDPlay header;
    PlayerWidget()
    : Widget(0,0,40,25)
    {
        canFocus = true;
        enabled = true;
        control = STOPPED;
        active = 0;
        playlist.load();
    }
    int active;

    void nextsong()
    {
      playlist.next();

      File f = Storage::open(playlist.current().filename.c_str(),"r",playlist.current().drive==8);
      if (load(f))
      {
        init(playlist.current().song);
        playtime = playlist.current().seconds;
      }
    }

    bool input(int c) override
    {
      if (c==VK_F1) active=0;
      if (c==VK_F2) active=1;
      if (c==VK_F3) active=2;
      if (c==VK_F4) active=3;

      if (c==VK_F5)
      {
        nextsong();
      }
      if (c==VK_F6)
      {
        playlist.save();
      }
      if (c==VK_F7)
      {
        playlist.add(playlist.cache);
      }

      if (c==VK_F12)
      {
          monitor();
          refreshscreen(ansi,use_ansi);
      }
      if (c==VK_SPACE)
      {
        if (control==PLAYING)
        {
          control = PAUSED;
        }
        else if (control==PAUSED)
        {
          control = PLAYING;
        }
      }

      if (c=='?')
      {
        sid.reset();
        sid2.reset();
        HelpDialog dlg;
        dlg.run(parent,parent->matrix,parent->ansi);
        refreshscreen(ansi,use_ansi);
      }

      if (c=='o')
      {
        sid.reset();
        sid2.reset();
        FileManagerDialog dlg;
        TextMatrix tm;
        dlg.drive = ZP_DEVNO;
        dlg.filepath = Storage::getcwd(ZP_DEVNO==8);
        dlg.run(nullptr,&tm,ansi);
        refreshscreen(ansi,use_ansi);
        if (dlg.result==Dialog::Result::OK)
        {
          File f = Storage::open(dlg.selected(),"r",dlg.drive==8);
          if (load(f))
          {
            init(header.startSong);
            playlist.cache.filename = dlg.selected();
            playlist.cache.seconds = -1;
            playlist.cache.drive = dlg.drive;
          }
        }
      }
      if (c=='6')
      {
        sid.setChipType('6');
      }
      if (c=='n')
      {
        sid.setNTSC();
      }
      if (c=='p')
      {
        sid.setPAL();
      }
      if (c=='1')
      {
        sid.mute[0] = !sid.mute[0];
      }
      if (c=='2')
      {
        sid.mute[1] = !sid.mute[1];
      }
      if (c=='3')
      {
        sid.mute[2] = !sid.mute[2];
      }
      if (c=='8')
      {
        sid.setChipType('8');
      }
      if (c=='f')
      {
        sid.readFilterSettings();
      }
      if (c==VK_UP) if (cursor>0) cursor--;
      if (c==VK_DOWN) if (cursor<4) cursor++;
      if (c==VK_LEFT) { if (sid.filterStrength6581>0) sid.filterStrength6581--; sid.applyFilterSettings(); }
      if (c==VK_RIGHT) { if (sid.filterStrength6581<15) sid.filterStrength6581++; sid.applyFilterSettings(); }

      if (c=='+')
      {
        if (header.startSong<header.songs)
        {
          header.startSong++;
          init(header.startSong);
        }
      }
      if (c=='-')
      {
        if (header.startSong>1)
        {
          header.startSong--;
          init(header.startSong);
        }
      }
      return false;
    }

    uint16_t freq[3][16];
    uint32_t freqx[16];
    uint8_t freqi;
    uint32_t freqcounter;

    void output(TextMatrix* tm) override
    {
      if (!play())
      {
        tm->sync = false;
        return;
      }
        freq[0][freqi] = (sid.regs[4]&1) ? ((sid.regs[1+7]<<8)|sid.regs[0]) : 0;
        freq[1][freqi] = (sid.regs[4+7]&1) ? ((sid.regs[1+7]<<8)|sid.regs[0+7]) : 0;
        freq[2][freqi] = (sid.regs[4+7+7]&1) ? ((sid.regs[1+7+7]<<8)|sid.regs[0+7+7]) :0;
        freqx[freqi] = (((minutes*60)+seconds)*75) + (freqcounter%75);
        freqcounter++;
        freqi++;
        freqi&=15;

      tm->sync = true;

      tm->setColor(BLACK);
      tm->fill(1,1,40,25,0x20);

      tm->setColor(GRAY2);
      tm->setCursor(1,2);
      tm->print("title");
      tm->setCursor(1,4);
      tm->print("author");
      tm->setCursor(1,6);
      tm->print("release");
      tm->hline(9,40,3,OLINE);
      tm->hline(9,40,5,OLINE);
      tm->hline(9,40,7,OLINE);
      tm->setColor(GRAY1);
      tm->hline(2,39,8,ULINE);
      tm->hline(2,39,10,OLINE);

      tm->setColor(YELLOW);
      tm->setCursor(9,2);
      tm->print((char*)header.name);
      tm->setCursor(9,4);
      tm->print((char*)header.author);
      tm->setCursor(9,6);
      tm->print((char*)header.copyright);

      int total = (minutes*60 + seconds);

      if (playtime==-1)
      {
        tm->setColor(GRAY2);
        tm->setCursor(35,11);
        tm->printf("%02d:%02d",minutes,seconds);
      }
      else
      {
        int remaining = playtime - total;
        int s = remaining % 60;
        int m = remaining / 60;
        tm->setColor(LIGHTRED);
        tm->setCursor(35,11);
        tm->printf("%02d:%02d",m,s);
      }

      // 1,8 $66
      if (playtime!=-1 && total>=playtime)
      {
        nextsong();
      }
      float max = (playtime!=-1) ? playtime : total;
      int progress = (total / max) * 38;
      for (int i=0; i<38; ++i)
      {
        int p = ((8)*40)+(i+1);
        tm->text[p]=0x66;
        tm->cram[p]=i<progress ? LIGHTBLUE : GRAY3;
      }

      // tab panel

      tm->setColor(GRAY3);
      tm->frame(1,13,40,13,true);

      {
        static struct {
          uint8_t x, y, w, h;
          const char* label;
        } tabs[4] = {
          {1,11,8,3,"timer"},
          {8,11,8,3,"layout"},
          {15,11,8,3,"chip"},
          {22,11,8,3,"player"},
        };
        int i=0;
        while (i<active) { tm->tab(tabs[i].x,tabs[i].y,tabs[i].w,tabs[i].h,tabs[i].label,false); i++; }
        i=3;
        while (active<i) { tm->tab(tabs[i].x,tabs[i].y,tabs[i].w,tabs[i].h,tabs[i].label,false); i--; }
        i=active;
        tm->tab(tabs[i].x,tabs[i].y,tabs[i].w,tabs[i].h,tabs[i].label,true);
      }

      if (active==0)
      {
        tm->setColor(GRAY2);
        tm->setCursor(3,15);
        tm->print("timing");
        tm->setCursor(3,17);
        tm->print("\"digi\"");
        tm->setCursor(3,19);
        tm->print("clock");
        tm->setCursor(3,21);
        tm->print("system");
        tm->setCursor(3,23);
        tm->print("chip");

        tm->setColor(YELLOW);
        tm->setCursor(10,15);
        if (header.speed)
        {
          if (cia1.counterA>0)
            tm->printf("cia timer, %d hz (%dx) %d",sid.frequency / cia1.counterA,multiplier,cia1.counterA);
          else
            tm->printf("cia timer (%dx)",multiplier);
        }
        else
        {
          tm->print("raster interrupt (1x)");
        }
        tm->setCursor(10,17);
        if (cia2.counterA>0)
          tm->printf("%d hz",sid.frequency / cia2.counterA);
        tm->setCursor(10,19);
        tm->printf("%d hz",sid.frequency);
        if (MC6502::swap(header.dataOffset) == 0x7C)
        {
          const char* video[4] = {"unknown","pal, 50.125 hz","ntsc, 60 hz","pal and ntsc"};
          tm->setCursor(10,21);
          tm->print(video[(header.flags>>2) & 3]);
          const char* sidmodel[4] = {"unknown","6581","8580","6581 and 8580"};
          tm->setCursor(10,23);
          tm->printf("%s (armsid 2.12)",sidmodel[(header.flags>>4) & 3]);
          //tm->setCursor(10,24);
          //tm->printf("8580 (swinsid nano)");
        }
      }

      if (active==1)
      {
        int X=2;
        int Y=15;
        tm->setColor(GRAY1);
        tm->frame(X,Y,34,6,true);
        ushort addr = 0x000;

        tm->setCursor(X,Y-1);
        tm->setColor(GRAY1);
        tm->print("zp");
        tm->setCursor(X+21,Y-1);
        tm->setColor(LIGHTRED);
        tm->print("basic");
        tm->setCursor(X+27,Y-1);
        tm->setColor(BROWN);
        tm->print("io");
        tm->setCursor(X+30,Y-1);
        tm->setColor(ORANGE);
        tm->print("kernal");

        tm->setColor(GRAY2);
        for (int x=0; x<16; ++x)
        {
          static const char* const hex="0123456789\1\2\3\4\5\6";
          tm->set(X+(x*2)+1,Y+5,hex[x]);
        }

        for (int x=0; x<32; ++x)
        {
          for (int y=0; y<4; ++y)
          {
            uint8_t code = 0x2E;
            bool inrange = (addr>=header.loadAddr && (addr<(header.loadAddr+size)));
            tm->setColor(inrange?YELLOW:GRAY1);
            if (addr<0x800)
            {
              code = 0x66;
              tm->setColor(GRAY1);
            }
            if (addr>=0xA000 && addr<0xC000)
            {
              code = 0x66;
              tm->setColor(LIGHTRED);
            }
            if (addr>=0xD000 && addr<0xE000)
            {
              code = 0x66;
              tm->setColor(BROWN);
            }
            if (addr>=0xE000)
            {
              code = 0x66;
              tm->setColor(ORANGE);
            }
            tm->setReverse(false);
            if (inrange)
            {
              tm->setReverse(true);
              code = 0x20;
            }
            tm->set(X+1+x,Y+1+y,code);
            addr += 0x0200;
          }
        }
        tm->setReverse(false);

        tm->setColor(GRAY2);
        tm->setCursor(2,22);
        tm->print("init");
        tm->setCursor(2,23);
        tm->print("play");
        tm->setCursor(14,22);
        tm->print("start");
        tm->setCursor(14,23);
        tm->print("end");

        tm->setColor(YELLOW);
        tm->setCursor(7,22);
        tm->printf("$%04x",MC6502::swap(header.initAddr));
        tm->setCursor(7,23);
        tm->printf("$%04x",MC6502::swap(header.playAddr));
        tm->setCursor(20,22);
        tm->printf("$%04x",header.loadAddr);
        tm->setCursor(20,23);
        tm->printf("$%04x",header.loadAddr+size);

      }

      if (active==2)
      {
        tm->setColor(GRAY2);
        tm->setCursor(2,14);
        tm->print("# freq pwm adsr cr f");

        tm->setCursor(2,16);
        tm->print("1");
        tm->setCursor(2,18);
        tm->print("2");
        tm->setCursor(2,20);
        tm->print("3");

        tm->setCursor(23,15);
        tm->print("cutoff freq");
        tm->setCursor(23,16);
        tm->print("oscillator");
        tm->setCursor(23,17);
        tm->print("hullcurve");
        tm->setCursor(23,18);
        tm->print("ext.filter");
        tm->setCursor(23,19);
        tm->print("3rd voice filter");

        tm->setCursor(2,22);
        tm->print("chip       resonance");
        tm->setCursor(13,23);
        tm->print("volume");

        uint8_t bits = sid.regs[24];
        tm->setCursor(23,20);
        tm->setColor(bits & 128 ? YELLOW : GRAY1);
        tm->print("off");
        tm->setCursor(27,20);
        tm->setColor(bits & 64 ? YELLOW : GRAY1);
        tm->print("high");
        tm->setCursor(32,20);
        tm->setColor(bits & 32 ? YELLOW : GRAY1);
        tm->print("band");
        tm->setCursor(37,20);
        tm->setColor(bits & 16 ? YELLOW : GRAY1);
        tm->print("low");

        tm->setColor(GRAY1);
        tm->hline(2,21,15);
        tm->vline(14,20,3);
        tm->vline(14,20,8);
        tm->vline(14,20,12);
        tm->vline(14,20,17);
        tm->vline(14,20,20);

        tm->set(3,15,CROSSWAY);
        tm->set(8,15,CROSSWAY);
        tm->set(12,15,CROSSWAY);
        tm->set(17,15,CROSSWAY);
        tm->set(20,15,CROSSWAY);

        uint8_t* r = sid.regs;
        tm->setColor(YELLOW);
        tm->setCursor(7,22);
        tm->print(sid.model=='6' ? "6581" : "8580");

        for (int i=0; i<3; ++i)
        {
          tm->setCursor(4,16+2*i);
          tm->printf("%04x",(r[1]<<8)|r[0]);

          tm->setCursor(9,16+2*i);
          tm->printf("%03x",((r[3]<<8)|r[2]) & 0xFFF );
          tm->setCursor(13,16+2*i);
          tm->printf("%04x",(r[5]<<8)|r[6] );
          tm->setCursor(18,16+2*i);
          tm->printf("%02x",r[4] );
          tm->set(21,16+2*i,((sid.regs[23] & (1<<i))==(1<<i))?0x51:0x20);
          r += 7;
        }

#if 0
        r = sid2.regs;
        tm->setColor(YELLOW);
        tm->setCursor(7,23);
        tm->print(sid2.model=='6' ? "6581" : "8580");

        for (int i=0; i<3; ++i)
        {
          tm->setCursor(4,17+2*i);
          tm->printf("%04x",(r[1]<<8)|r[0]);

          tm->setCursor(9,17+2*i);
          tm->printf("%03x",((r[3]<<8)|r[2]) & 0xFFF );
          tm->setCursor(13,17+2*i);
          tm->printf("%04x",(r[5]<<8)|r[6] );
          tm->setCursor(18,17+2*i);
          tm->printf("%02x",r[4] );
          tm->set(21,17+2*i,((sid.regs[23] & (1<<i))==(1<<i))?0x51:0x20);
          r += 7;
        }
#endif
        tm->setCursor(35,15);
        tm->printf("%03x",(sid.regs[22]<<3)|(sid.regs[21]&7));
        tm->setCursor(35,16);
        tm->printf("%02x",sid.read(27));//sid.regs[27]);//
        tm->setCursor(35,17);
        tm->printf("%02x",sid.read(28));//sid.regs[28]);//

        tm->setCursor(35,18);
        tm->print( (sid.regs[23] & 8) ? "on " : "off");

        tm->progress(22,21,15,((sid.regs[23]>>4) & 15));
        tm->progress(22,22,15,(sid.regs[24] & 15));
      }

      if (active==3)
      {
        int X = 11;
        int Y = 14;

        tm->setCursor(24,15);
        tm->setColor(GRAY2);
        tm->print("midi out");
        tm->setCursor(33,15);
        tm->setColor(YELLOW);
        tm->print("off");

        tm->setColor(GRAY1);
        tm->vline(14,24,10);
        tm->vline(14,24,14);
        tm->vline(14,24,18);
        tm->vline(14,24,22);
        tm->setColor(GRAY2);
        
        for (int j=0; j<11; ++j)
        {
            uint16_t k = (freqi-1-j) & 15;
            tm->setCursor(2,24-j);
            int mins;
            int secs;
            int frames;
            int t = freqx[k];
            frames = t % 75;
            t /= 75;
            secs = t % 60;
            t /= 60;
            mins = t % 60;

            // 00:00:00.000
            tm->printf("%02d:%02d.%02d",mins,secs,frames);
        }
        for (int i=0; i<3; ++i)
        {
          for (int j=0; j<11; ++j)
          {
            uint16_t k = (freqi-1-j) & 15;
            uint32_t f = freq[i][k];
            const char* s = "---";
            tm->setColor(GRAY2);
            if (f>0)
            {
              const NoteEntry* n = findClosestNote(f);
              if (n)
              {
                tm->setColor(YELLOW);
                s = n->name;
              }
            }
            tm->setCursor(X+i*4,Y+10-j);
            tm->print(s);
            
          }
        }
      }
    }

    unsigned long last_timestamp;
    int delta_accumulator;
    int one_second_timer;
    int timervalue;
    int multiplier;
    int seconds;
    int minutes;
    int frequency;
    int frames;
    int playtime;
    uint16_t size;

    void init(uint8_t song)
    {
      if (header.playAddr!=0)
        poke(1,5);

      cpu.status |= FLAG_INTERRUPT;
      cpu.a = song-1;
      cpu.pc = 900;
      cia2.counterB = 0;
      cia2.counterA = 0;
      cia1.counterA = 0;
      cia1.counterB = 0;
      
      while (cpu.pc != 903)
      {
        cpu.fastclock();
        int key = Ansi::read(Serial);
        if (key == VK_F9)
        {
          monitor();
        }
      }
      cpu.status &=~ FLAG_INTERRUPT;

      if (header.playAddr==0)
      {
        cpu.irq();
        while (cpu.status & FLAG_INTERRUPT)
        {
          cpu.fastclock();
        }
      }
      else
      {
          poke(1,5);
          cpu.a = 0;
          cpu.pc = 903;
          cpu.status |= FLAG_INTERRUPT;
          uint16_t returnaddress = 906;
          while (cpu.pc != returnaddress)
          {
            cpu.fastclock();
          }
          cpu.status &=~ FLAG_INTERRUPT;
          poke(1,7);
      }
      
      seconds = 0;
      minutes = 0;
      frames = 0;
      playtime = -1;

      int flags = (header.flags>>2) & 3;

      int framecycles = (flags == 2) ? 17095 : (CYCLES_PER_RASTERLINE*RASTERLINES_PER_FRAME) ;
      if (flags == 2)
      {
        sid.setNTSC();
      }
      else
      {
        sid.setPAL();
      }
      timervalue = framecycles;
      if (header.speed!=0)
      {
        timervalue = cia1.counterA;
        if (timervalue==0) timervalue = 1;
      }
      multiplier = framecycles  / timervalue;
      frequency = sid.frequency / timervalue;

      delta_accumulator = 0;
      one_second_timer = 0;
      last_timestamp = micros();
      digi_counter = 0;
      play_counter = 0;
      freqcounter = 0;
      freqi = 0;
    }

    int digi_counter;
    int play_counter;

    void fastplay()
    {
      unsigned long now = micros();
      long delta = now-last_timestamp;
      last_timestamp = now;

      if (control==PAUSED)
      {
        delta = 0;
      }

      one_second_timer += delta;
      if (one_second_timer >= 1000000)
      {
        Serial.printf("\e]0; BALSTER PLAYER, %d bytes free, r:%d io/s w:%d io/s \007",ESP.getFreeHeap(),io.reads,io.writes);
        io.reads = io.writes = 0;

        one_second_timer -= 1000000;
        seconds++;
        freqcounter=0;
        if (seconds>59)
        {
          seconds=0;
          minutes++;
        }
      }

      play_counter += delta;

      if (header.playAddr==0)
      {
        if (cia2.counterA>0)
        {
          digi_counter += delta;
          while (digi_counter >= cia2.counterA)
          {
            digi_counter -= cia2.counterA;
            cpu.nmi();
            while (cpu.status & FLAG_INTERRUPT)
            {
              cpu.fastclock();
            }
          }
        }

        while (play_counter >= timervalue)
        {
          play_counter -= timervalue;
          cpu.status &=~ FLAG_INTERRUPT;
          cpu.pc = 0xe5cd;
          cpu.irq();
          while (cpu.status & FLAG_INTERRUPT)
          {
            cpu.fastclock();
          }
          cpu.pc = 0xe5cd;
        }
      }
      else
      {
        if (play_counter >= timervalue)
        {
          play_counter -= timervalue;
          poke(1,5);
          cpu.a = 0;
          cpu.pc = 903;
          cpu.status |= FLAG_INTERRUPT;
          uint16_t returnaddress = 906;
          while (cpu.pc != returnaddress)
          {
            cpu.fastclock();
            if (cpu.hitsBreakpoint())
            {
              monitor();
            }
          }
          cpu.status &=~ FLAG_INTERRUPT;
          poke(1,7);
        }
      }
      
    }

    bool play()
    {
      unsigned long tt = micros() + 1000*50;
      while (tt > micros())
      {
        fastplay();
      }
      return true;
    }

  bool load(File file)
  {
    sid.reset();
    sid2.reset();
    uint16_t offset = 0;
    file.read((uint8_t*)&header.magic,sizeof(header.magic));
    file.read((uint8_t*)&header.version,sizeof(header.version));
    file.read((uint8_t*)&header.dataOffset,sizeof(header.dataOffset));
    offset = MC6502::swap(header.dataOffset);
    file.read((uint8_t*)&header.loadAddr,sizeof(header.loadAddr));
    file.read((uint8_t*)&header.initAddr,sizeof(header.initAddr));
    file.read((uint8_t*)&header.playAddr,sizeof(header.playAddr));

    file.read((uint8_t*)&header.songs,sizeof(header.songs));
    file.read((uint8_t*)&header.startSong,sizeof(header.startSong));

    header.songs = MC6502::swap(header.songs);
    header.startSong = MC6502::swap(header.startSong);

    file.read((uint8_t*)&header.speed,sizeof(header.speed));
    file.read((uint8_t*)&header.name,sizeof(header.name));
    file.read((uint8_t*)&header.author,sizeof(header.author));
    file.read((uint8_t*)&header.copyright,sizeof(header.copyright));

    for (int i=0; i<32; ++i)
    {
      header.name[i]=tolower(header.name[i]);
      header.author[i]=tolower(header.author[i]);
      header.copyright[i]=tolower(header.copyright[i]);
    }

    if (offset == 0x7C)
    {
      file.read((uint8_t*)&header.flags,sizeof(header.flags));
      header.flags = MC6502::swap(header.flags);
      file.read((uint8_t*)&header.startPage,sizeof(header.startPage));
      file.read((uint8_t*)&header.pageLength,sizeof(header.pageLength));
      file.read((uint8_t*)&header.secondSIDAddress,sizeof(header.secondSIDAddress));
      file.read((uint8_t*)&header.thirdSIDAddress,sizeof(header.thirdSIDAddress));

      switch((header.flags>>4) & 3)
      {
        case 0:
        sid.setChipType('6');
        break;
        case 1:
        sid.setChipType('6');
        break;
        case 2:
        sid.setChipType('8');
        break;
        case 3:
        sid.setChipType('8');
        break;
      }
    }

    file.seek(offset,SeekMode::SeekSet);
    if (header.loadAddr==0)
    {
      file.read((uint8_t*)&header.loadAddr,2);
      header.loadAddr = MC6502::swap(header.loadAddr);
      offset-=2;
    }

    header.loadAddr = MC6502::swap(header.loadAddr);

    size = file.size() - offset;
    file.read(&RAM[header.loadAddr],size);
    file.close();

    Assembler as;
    as.define("init",MC6502::swap(header.initAddr));
    as.define("play",MC6502::swap(header.playAddr));

    as.compile(
      "*=$384\0"   // compile to this address, origin. 192 bytes unused here
      "jsr init\0"
      "jsr play\0"
      "\0\0\0\0");

      //monitor();
      //refreshscreen(ansi,use_ansi);

    return true;
  }
};

class SidPlayer : public Dialog
{
public:
  PlayerWidget player;
  SidPlayer()
  : Dialog()
  , player()
  {
    add(&player);
  }

  int input() override
  {
      int key = Dialog::input();
      if (key==27)
      {
          result = Result::CANCEL;
          reset();
          return key;
      }
      return -1;
  }
};

void hijacked_load(const char* label)
{
  File globalfile;

  if (d64.isMounted())
  {
    d64.load();
    return;
  }
  else if (t64.isMounted())
  {
    t64.load();
    return;
  }

  char str[64];
  petcii_getstring(ZP_CURFILENAME,ZP_CURFILENAMELENGTH,str);

  // cpu.a 0==load, else verify"WIFI"
  // x,y = load address

  if (cpu.a==1)
  {
    if (0==strncmp(str,"CYCLE=",6))
    {
      cpu.cyclehack = strtoul(str+6,0,0);
    }
    #if defined(USEWIFI)
    if (0==strcmp(str,"WIFI"))
    {
      WiFi.begin(MY_SSID, MY_PASS);
      server.addHandler(new SPIFFSEditor(LITTLEFS));
      server.begin();
    }
    #endif
    if (0==strcmp(str,"FORMAT"))
    {
      LITTLEFS.format();
    }
    if (0==strcmp(str,"6581"))
    {
      sid.setChipType('6');
    }
    if (0==strcmp(str,"8580"))
    {
      sid.setChipType('8');
    }

    cpu.y = 0x49;    // "LOADING"
    cpu.pc = KERNAL_PRINT_MESSAGE; // Check direct mode, print and return with carry cleared
    return;
  }

  if (ZP_CURFILENAMELENGTH==0)
  {
    FileManagerDialog dlg;
    TextMatrix tm;
    dlg.drive = ZP_DEVNO;
    dlg.filepath = "/";
    dlg.run(nullptr,&tm,ansi);
    refreshscreen(ansi,use_ansi);
    if (dlg.result==Dialog::Result::OK)
    {
      ZP_DEVNO = dlg.drive;
      ZP_SECONDARYADDRESS = 1;
      pathname.setFilename(dlg.selected());
      strcpy(str,pathname.name());
    }
  }

  // load"$",8
  if (RAM[ZP_CURFILENAME] == '$' && ZP_CURFILENAMELENGTH == 1)
  {
    globalfile = Storage::open(pathname.path(),"r",ZP_DEVNO==8);

    uint16_t txttab = ZP_BASICSTART;
    Buffer buffer(RAM+txttab,0xA000-txttab);
    DirectoryWriter dw(&buffer);

    dw.begin(pathname.path(),txttab);
    uint32_t total = 0;
		while (true) {
			File entry = globalfile.openNextFile();
			if (! entry) break;
				int blocks = entry.isDirectory() ? 0 : ceil((float)entry.size()/256.0f);
        char* s = str;
        const char* name = rindex(entry.name(),'/');
        if (name==nullptr) name=entry.name(); else name++;
        strncpy(s,name,17);
				//entry.getName(s, 17);
				while(*s) {*s = toupper(*s); s++;}

        // TODO: .prg -> type 2
        // .seq? .rel? .usr?

        int type=2;
        if (entry.isDirectory()) type = 7;
        if (strstr(name,".SID")) type=6;
        dw.add(blocks,str,type);
        total+=entry.size();
			
			entry.close();
		}
		globalfile.close();
    
    uint32_t freeblocks = Storage::freeblocks(ZP_DEVNO==8);

    dw.end(freeblocks);
    RAM[0xAF] = (dw.addr) & 0xff;
    RAM[0xAE] = (dw.addr) >> 8;
    
		cpu.y = 0x49; //Offset for "LOADING"
		cpu.pc = 0xF12B; //Print and return
		return;
  }
  else
  if (RAM[ZP_CURFILENAME] == '%' && ZP_CURFILENAMELENGTH == 1)
  {
    Serial.println("");
    Serial.println("");
    Serial.printf("DBALSTER'S ESP32 C64 EMULATOR\n");
    Serial.println("----------------------------");
        ;
    //Serial.printf("SD : %d,%lld,%lld,%lld\n",  SD.cardType(),SD.cardSize(),SD.totalBytes(),SD.usedBytes());
    Serial.printf("CPU FREQ   : %d MHZ\n",ESP.getCpuFreqMHz());
    Serial.printf("FREE MEMORY: %d OF %d KB\n",ESP.getFreeHeap()/1024,ESP.getHeapSize()/1024);
    Serial.printf("SDK VERSION: %s\n",ESP.getSdkVersion());
    //Serial.printf("psram: %d\n",ESP.getPsramSize());
    //Serial.printf(".free: %d\n",ESP.getFreePsram());

    cpu.status &=~ FLAG_CARRY;
    cpu.pc = 0xEE83; // rts

		return;
  }
  else
  {
    char buf[65];

    bool found = false;
    pathname.changeDir(str);

    if (strlen(pathname.name())==0)
    {
      Storage::chdir(pathname.path(),ZP_DEVNO==8);
      cpu.y = 0x49;    // "LOADING"
      // good loading
      cpu.pc = 0xF5A9; //KERNAL_PRINT_MESSAGE; // Check direct mode, print and return with carry cleared
      return;
    }
    if (Storage::exists(pathname.full(),ZP_DEVNO==8))
    {
      globalfile = Storage::open(pathname.full(),"r",ZP_DEVNO==8);
      found=true;
      strncpy(buf,pathname.name(),64);
      for (int i=0; buf[i]; ++i) buf[i]=toupper(buf[i]);
    }
    
    if (!found)
    {
      File sdf = Storage::open(pathname.path(),"r",ZP_DEVNO==8);
      while (true) {
        File entry = sdf.openNextFile();
        if (! entry) break;
        if (!entry.isDirectory()) {

          const char* name = rindex(entry.name(),'/');
          if (name==nullptr) name=entry.name(); else name++;
          strncpy(buf,name,64);
          for (int i=0; buf[i]; ++i) buf[i]=toupper(buf[i]);
          if (match(buf,pathname.name()))
          {
            pathname.changeDir(buf); // unmatch
            globalfile = Storage::open(pathname.full(),"r",ZP_DEVNO==8);
            found = true;
            break;
          }
        }
        entry.close();
      }
      sdf.close();
    }
    
    if (!found)
    {
      cpu.pc = KERNAL_FILE_NOT_FOUND;
      return;
    }
    if (!globalfile)
    {
      cpu.pc = KERNAL_FILE_NOT_OPEN;
      return;
    }

    if (strstr(buf,".D64"))
    {
      d64.mount(globalfile);
      cpu.y = 0x49;    // "LOADING"
      cpu.pc = KERNAL_PRINT_MESSAGE; // Check direct mode, print and return with carry cleared
    }
    else if (strstr(buf,".T64"))
    {
      t64.mount(globalfile);
      cpu.y = 0x49;    // "LOADING"
      cpu.pc = KERNAL_PRINT_MESSAGE; // Check direct mode, print and return with carry cleared
    }
    else if (strstr(buf,".SID"))
    {
      SidPlayer dlg;
      if (dlg.player.load(globalfile))
      {
        dlg.player.init(dlg.player.header.startSong);
        TextMatrix tm;
        tm.clear(0);
        dlg.run(nullptr,&tm,ansi,true);
        refreshscreen(ansi,use_ansi);
      }
      cpu.y = 0x49;    // "LOADING"
      cpu.pc = KERNAL_PRINT_MESSAGE; // Check direct mode, print and return with carry cleared

      cpu.y = 0x49;    // "LOADING"
      // good loading
      cpu.pc = 0xF5A9; //KERNAL_PRINT_MESSAGE; // Check direct mode, print and return with carry cleared

    }
    else
    {
      uint16_t addr = 0;
      uint32_t size = globalfile.size();
      if (ZP_SECONDARYADDRESS>0  || (ZP_DEVNO==1))
      {
        uint8_t buf[2];
        globalfile.read(buf, 2);
        size -= 2;
        addr = buf[1] * 256 + buf[0];
      }
      else
      {
        addr = (cpu.y * 256) + cpu.x;
      }
      globalfile.read(&RAM[addr], size);
      globalfile.close();

      RAM[0xAE] = (addr + size) & 0xff;
      RAM[0xAF] = (addr + size) >> 8;

      cpu.y = 0x49;    // "LOADING"
      // good loading
      cpu.pc = 0xF5A9; //KERNAL_PRINT_MESSAGE; // Check direct mode, print and return with carry cleared
    }
  }
}

void hijacked_close(const char* label)
{
  int index = -1;

  for (int i=0; i<ZP_NUMFILESOPEN; ++i)
  {
    if (ZP_FILENO_T(i) != cpu.a) continue;
    index = i;
    break;
  }
  if (index==-1)
  {
    cpu.pc = 0xf294; // silent exit, file already closed
    return;
  }
  int devno = ZP_DEVNO_T(index);
  if (!((devno==1) || (devno>=8 && devno<=11)))
  {
    return;
  }

  cpu.pc = 0xF2F2; // continue in CLOSE KERNAL CALL
  
  kfm.close(ZP_FILENO_T(index),ZP_SECADR_T(index),ZP_DEVNO_T(index));
}

/*
TODO: need to manage logical file handles
*/
void hijacked_iecin(const char* label)
{
  int byte = -1;

  if (d64.isMounted())
  {
    byte = d64.readbyte();
  }
  else
  {
    if (ZP_SECONDARYADDRESS==15)
    {
      byte = iec.readByte();
    }
    else
    {
      KernalFile* fe = kfm.get(ZP_FILENO,ZP_SECONDARYADDRESS,ZP_DEVNO);
      if (fe)
      {
        byte = fe->read();  
      }
    }
  }

  if (byte==-1) // EOF
  {
    ZP_ST = 0x40;
    cpu.status |= FLAG_CARRY;
  }
  else
  {
    ZP_ST = 0;
    cpu.status &=~ FLAG_CARRY;
  }

  cpu.a = byte;
  cpu.pc = 0xEE83; // rts
}

// A = byte to output
void hijacked_iecout(const char* label)
{
  if (ZP_SECONDARYADDRESS==15)
  {
    iec.writeByte(cpu.a);
  }
  else
  {
      KernalFile* fe = kfm.get(ZP_FILENO,ZP_SECONDARYADDRESS,ZP_DEVNO);
      if (fe)
      {
        fe->write(cpu.a);  
      }
  }
  cpu.status &=~ FLAG_CARRY;
  cpu.pc = 0xEE83; // rts
}

// A = secondary address
void hijacked_lstnsa(const char* label)
{
  cpu.status &=~ FLAG_CARRY;
  cpu.pc = 0xF1B4; // rts
}

// A = secondary address
void hijacked_talksa(const char* label)
{
  cpu.status &=~ FLAG_CARRY;
  cpu.pc = 0xF1B4; // rts
}

void hijacked_untalk(const char* label)
{
  cpu.status &=~ FLAG_CARRY;
  cpu.pc = 0xF1B4; // rts
}

void hijacked_unlstn(const char* label)
{
  cpu.status &=~ FLAG_CARRY;
  cpu.pc = 0xF1B4; // rts
}

// A = device number
void hijacked_listen(const char* label)
{
  cpu.status &=~ FLAG_CARRY;
  cpu.pc = 0xF1B4; // rts
}

// A = device number
void hijacked_talk(const char* label)
{
  cpu.status &=~ FLAG_CARRY;
  cpu.pc = 0xF1B4; // rts
}

void hijacked_chrin(const char* label)
{
  //if (ZP_CURINPUTDEVICE==0) return; // keyboard
  //if (ZP_CURINPUTDEVICE==3) return; // screen
}

void hijacked_clall(const char* label)
{
  kfm.reset();
}

void hijacked_clrchn(const char* label)
{
}

void hijacked_chrout(const char* label)
{
  if (use_ansi && ZP_CUROUTPUTDEVICE==3)
  {
    uint8_t a = cpu.a;
    switch(a)
    {
      case 13: Serial.println(); break; // RETURN
      case 10:  break; // ?
      case 5:   Serial.print(Ansi::Foreground[1]); break; // WHT
      case 28:  Serial.print(Ansi::Foreground[2]); break; // RED
      case 30:  Serial.print(Ansi::Foreground[5]); break; // GRN
      case 31:  Serial.print(Ansi::Foreground[6]); break; // BLU
      case 129: Serial.print(Ansi::Foreground[8]); break; // ORN
      case 144: Serial.print(Ansi::Foreground[0]); break; // BLK
      case 149: Serial.print(Ansi::Foreground[9]); break; // BRN
      case 150: Serial.print(Ansi::Foreground[10]); break; // LRED
      case 151: Serial.print(Ansi::Foreground[11]); break; // GRY1
      case 152: Serial.print(Ansi::Foreground[12]); break; // GRY2
      case 153: Serial.print(Ansi::Foreground[13]); break; // LGRN
      case 154: Serial.print(Ansi::Foreground[14]); break; // LBLU
      case 155: Serial.print(Ansi::Foreground[15]); break; // GRY3
      case 156: Serial.print(Ansi::Foreground[4]); break; // PUR
      case 158: Serial.print(Ansi::Foreground[7]); break; // YEL
      case 159: Serial.print(Ansi::Foreground[3]); break; // CYN
      case 17: Serial.print("\e[1B"); break; // cursor down
      case 18: Serial.print("\e[7m"); break; // rvs on
      case 146: Serial.print("\e[0m"); Serial.print(Ansi::Background[6]); Serial.print(Ansi::Foreground[ZP_CURRENTCOLOR]); break; // rvs off
      case 19: Serial.print("\e[0;0H"); break; // home
      case 20: Serial.print(" "); break; // del
      case 29: Serial.print("\e[1C"); break; // cursor right
      case 145: Serial.print("\e[1A"); break; // cursor up
      case 147: Serial.print("\e[2J"); break; // clr
      case 148: Serial.print(""); break; // inst
      case 157: Serial.print("\e[1D"); break; // cursor left
      default:
        Serial.write(a);
        break;
    }
    return;
  }
}

void hijacked_chkin(const char* label)
{
  int fileno = -1;
  int devno = -1;

  for (int i=0; i<ZP_NUMFILESOPEN; ++i)
  {
    if (ZP_FILENO_T(i) != cpu.x) continue;
    fileno = cpu.x;
    devno = ZP_DEVNO_T(i);
    break;
  }
  if (fileno==-1)
  {
    cpu.pc = KERNAL_FILE_NOT_OPEN;
    cpu.a = 3;
    cpu.status |= FLAG_CARRY;
    return;
  }

  if (!((devno==1) || (devno>=8 && devno<=11)))
  {
    return;
  }

  cpu.a = devno;
  ZP_CURINPUTDEVICE = devno;
  cpu.status &= FLAG_CARRY;
  cpu.pc = 0xF233;
}

void hijacked_chkout(const char* label)
{
  int fileno = -1;
  int devno = -1;

  for (int i=0; i<ZP_NUMFILESOPEN; ++i)
  {
    if (ZP_FILENO_T(i) != cpu.x) continue;
    fileno = cpu.x;
    devno = ZP_DEVNO_T(i);
    break;
  }
  if (fileno==-1)
  {
    cpu.pc = KERNAL_FILE_NOT_OPEN;
    cpu.a = 3;
    cpu.status |= FLAG_CARRY;
    return;
  }

  if (!((devno==1) || (devno>=8 && devno<=11)))
  {
    return;
  }

  cpu.a = devno;
  ZP_CUROUTPUTDEVICE = devno;
  cpu.pc = 0xF275;
}

void hijacked_open(const char* label)
{
  if (!((ZP_DEVNO==1) || (ZP_DEVNO>=8 && ZP_DEVNO<=11))) return;

  ZP_ST = 0; // clear serial status byte
  if (ZP_FILENO==0)
  {
    cpu.pc = KERNAL_NOT_INPUT_FILE;
    return;
  }
  for (int i=0; i<ZP_NUMFILESOPEN; ++i)
  {
    if (ZP_FILENO_T(i) == ZP_FILENO)
    {
      cpu.pc = KERNAL_FILE_OPEN;
      return;
    }
  }
  if (ZP_NUMFILESOPEN==10)
  {
    cpu.pc = KERNAL_TOO_MANY_FILES;
    return;
  }

  char str[65];
  str[0]='/';
  petcii_getstring(ZP_CURFILENAME,ZP_CURFILENAMELENGTH,str+1);

  if (d64.isMounted())
  {
    if (!d64.open())
    {
      cpu.pc = KERNAL_FILE_NOT_FOUND;
      return;
    }
  }
  else
  {
    // TODO: check for # filename, the buffer
    if (ZP_SECONDARYADDRESS==15)
    {
      iec.open(str);
      // command channel
    }
    else
    {
      Serial.printf("\e]0; open %d,%d,%d %s \007",ZP_FILENO,ZP_DEVNO,ZP_SECONDARYADDRESS,str);
      if (str[1]=='$')
      {
        
        KernalFile* fe = kfm.open(ZP_FILENO,ZP_SECONDARYADDRESS,ZP_DEVNO);
        if (fe==nullptr)
        {
          cpu.pc = KERNAL_TOO_MANY_FILES;
          return;
        }

        fe->buffer = new Buffer(4096);

        uint16_t txttab = ZP_BASICSTART;
        DirectoryWriter dw(fe->buffer);

        File file = Storage::open(pathname.path(),"r",ZP_DEVNO==8);

        dw.begin("INTERNAL SPIFFS",txttab);
        uint32_t total = 0;
        while (true) {
          File entry = file.openNextFile();
          if (! entry) break;
          if (!entry.isDirectory()) {
            int blocks = ceil((float)entry.size()/256.0f);
            char* s = str;
            strncpy(s,entry.name()+1,17);
            while(*s) {*s = toupper(*s); s++;}
            dw.add(blocks,str,2);
            total+=entry.size();
          }
          entry.close();
        }
        file.close();
        dw.end(0);

      }
      else if (str[0]!=0)
      {
        const char* mode = "r";
        char* iter = strtok(str,",");
        const char* name = iter;
        while (iter)
        {
          if (*iter=='W') mode = "w";
          if (*iter=='A') mode = "a";
          iter = strtok(NULL,",");
        }

        pathname.changeDir(name);

        if ((mode[0]=='r') && !Storage::exists(pathname.full(),ZP_DEVNO==8))
        {
          cpu.pc = KERNAL_FILE_NOT_FOUND;
          return;
        }

        KernalFile* fe = kfm.open(ZP_FILENO,ZP_SECONDARYADDRESS,ZP_DEVNO);
        if (fe==nullptr)
        {
          cpu.pc = KERNAL_TOO_MANY_FILES;
          return;
        }

        File file = Storage::open(pathname.full(),mode,ZP_DEVNO==8);
        if (!file)
        {
          kfm.close(ZP_FILENO,ZP_SECONDARYADDRESS,ZP_DEVNO);
          cpu.pc = KERNAL_FILE_NOT_OPEN;
          return;
        }
        
        fe->file = file;
      }
    }
  }

  // this is what the kernal does
  ZP_FILENO_T(ZP_NUMFILESOPEN) = ZP_FILENO;        // file no
  ZP_SECADR_T(ZP_NUMFILESOPEN) = ZP_SECONDARYADDRESS | 0x60;
  ZP_DEVNO_T(ZP_NUMFILESOPEN) = ZP_DEVNO;        // device no
  ZP_NUMFILESOPEN++;

  cpu.status &=~ FLAG_CARRY;
  cpu.pc = 0xF3D3; 
}

void hijacked_save(const char* label)
{
  char bbb[65];
  const char* filename = bbb;
  bbb[0] = '/';
  petcii_getstring(ZP_CURFILENAME,ZP_CURFILENAMELENGTH,bbb+1);

  if (filename[0]=='@') filename+=3; // ??

  uint16_t addr,size;

	addr = RAM[cpu.a + 1] * 256 + RAM[cpu.a];
	size = (cpu.y * 256 + cpu.x) - addr;

  uint8_t buffer[2];
	buffer[0] = addr & 0xff;
	buffer[1] = addr >> 8;

	if (Storage::exists(filename,ZP_DEVNO==8)) Storage::remove(filename,ZP_DEVNO==8);
	File file = Storage::open(filename, "w",ZP_DEVNO==8);
	if (!file) {
		cpu.pc = 0xf530; //Jump to $F530
		return;
	}
  if (ZP_SECONDARYADDRESS!=0 || (ZP_DEVNO==1))
  {
	  file.write(buffer, 2);
  }
	file.write(&RAM[addr], size);
	file.close();

  if (ZP_ERROR_DISPLAY & FLAG_SHOW_IO_ERROR_MESSAGES)
  {
  }

  cpu.a = 0;
  cpu.pc = 0xF68D; // clc rts
}

void web_index(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

void http_200_ok(AsyncWebServerRequest *request)
{
    request->send(200);
}

void http_prg_upload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
{
  static uint16_t addr = 0;
  if(!index)
  {
    addr = data[1]*256 + data[0];
    memcpy(&RAM[addr],data+2,len-2);
  }
  else
  {
    memcpy(&RAM[addr+index-2],data,len);
  }

  if (final)
  {
    RAM[0xAE] = (addr + index -2) & 0xff;
    RAM[0xAF] = (addr + index -2) >> 8;
  }
}

void setup()
{
  Serial.begin(921600);
  while (!Serial) delay(100);

  //setCpuFrequencyMhz(240);

  pinMode(SD_MISO, PULLUP);
  pinMode(SD_MOSI, PULLUP);
  pinMode(SD_SCK, PULLUP); 
  Serial.println("[SD/MMC]...");
  SD_MMC.begin("/sdcard",true,true,SDMMC_FREQ_52M,10);
//#define SDMMC_FREQ_52M          52000       /*!< MMC 52MHz speed */
  //bool setPins(int clk, int cmd, int d0, int d1, int d2, int d3);
  //  bool begin(const char * mountpoint="/sdcard", bool mode1bit=false, bool format_if_mount_failed=false, int sdmmc_frequency=BOARD_MAX_SDMMC_FREQ, uint8_t maxOpenFiles = 5);

  Serial.println("[LittleFS]...");
  if (!LITTLEFS.begin(true))
  

  {
    Serial.printf("formatting LITTLEFS...\n");
    if (!LITTLEFS.format())
    {
      Serial.printf("FILESYSTEM NOT FORMATTED\n");
    }
    else Serial.printf("formatted!\n");
    LITTLEFS.begin();
  }

  Serial.println("[WiFi]...");
  WiFi.begin(MY_SSID, MY_PASS);
  server.addHandler(new SPIFFSEditor(LITTLEFS));
  server.on("/",HTTP_POST,http_200_ok,http_prg_upload);
  server.on("/run",HTTP_POST,http_200_ok,[](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    //reset();
    http_prg_upload(request,filename,index,data,len,final);

    if (final)
    {
        Assembler as;
        as.compile(
          "*=$c000\0"   // compile to this address, origin. 192 bytes unused here
          "jsr $A659\0"
          "jmp $A7AE\0"
          "\0\0\0\0");
          cpu.pc = 0xC000;
    }
  });

  server.begin();

  //disableCore0WDT();
  //disableCore1WDT();
  disableLoopWDT();

  ansi = new AnsiRenderer();
 
  io.init();
  //reu.init(2);
  memory_init();
  init_mappings();
  monitor_init();
  vic.setup();

  cpu.init();
  cpu.setpatch(0xf49e,hijacked_load,"LOAD");
  cpu.setpatch(0xF34A,hijacked_open,"OPEN");
  cpu.setpatch(0xf5dd,hijacked_save,"SAVE");
  cpu.setpatch(0xF20E ,hijacked_chkin,"CHKIN");
  cpu.setpatch(0xF250 ,hijacked_chkout,"CHKOUT");
  cpu.setpatch(0xF291 ,hijacked_close,"CLOSE");
  cpu.setpatch(0xF157 ,hijacked_chrin,"CHRIN");
  cpu.setpatch(0xF1CA ,hijacked_chrout,"CHROUT");
  cpu.setpatch(0xF333 ,hijacked_clrchn,"CLRCHN");
  cpu.setpatch(0xEE13 ,hijacked_iecin,"IECIN");
  cpu.setpatch(0xEDDD ,hijacked_iecout,"IECOUT");
  cpu.setpatch(0xEDB9 ,hijacked_lstnsa,"LSTNSA");
  cpu.setpatch(0xEDC7 ,hijacked_talksa,"TALKSA");
  cpu.setpatch(0xEDEF ,hijacked_untalk,"UNTALK");
  cpu.setpatch(0xEDFE ,hijacked_unlstn,"UNLSTN");
  cpu.setpatch(0xED0C ,hijacked_listen,"LISTEN");
  cpu.setpatch(0xED09 ,hijacked_talk,"TALK");
  cpu.setpatch(0xFFE7, hijacked_clall,"CLALL");

  reset();
  pathname.changeDir();

  last_timestamp = micros();
  delta_accumulator = 0;
}

void loop()
{
  unsigned long now = micros();
  long delta = now-last_timestamp;
  last_timestamp = now;

  accurracy += delta;

  delta_accumulator += delta;
  // 20000µs = 20ms = 1000ms / 50fps = PAL
  if (delta_accumulator < 20000) return;
  delta_accumulator = 0;

  cia1.prefetch();
  cia2.prefetch();
  for (uint32_t y=0; y<RASTERLINES_PER_FRAME; ++y)
  {
    vic.begin(y);
    for (vic.cycle=1; vic.cycle<=CYCLES_PER_RASTERLINE;++vic.cycle)
    {
      vic.clock(); // just check for irq
      cia1.clock();
      cia2.clock();
      sid.clock();
      //reu.clock();
      if (vic.cpu_is_rdy)
      {
        cpu.clock();
        if (cpu.hitsBreakpoint())
        {
          monitor();
          refreshscreen(ansi,use_ansi);
        }
      }
    }
  }
  input();

  if (!use_ansi)
  {
    ansi->sync(delayed_matrix,CRAM,vic.read(0x21));
  }

  frame++;
  if (frame>=50)
  {
    //ArduinoOTA.handle();
    float percentage = (20.0f/(accurracy/50.0f/1000.0f))*100.0f;
    Serial.printf("\e]0; %s %s %.1f%% r:%d io/s w:%d io/s \007",use_ansi?"ANSI":"POLL",
    #if defined(USEWIFI)
    WiFi.localIP().toString().c_str(),
    #else
      "disabled",
    #endif
    percentage,io.reads,io.writes    );
    io.reads = io.writes = 0;
    accurracy = 0;
    frame=0;
  }

}
