/*

I lost track from where this originates exactly, I assume it's a blend of

http://rubbermallet.org/fake6502.c
https://github.com/dirkwhoffmann/virtualc64/blob/master/Emulator/CPU/CPUInstructions.cpp

*/

#include <stdio.h>
#include <stdlib.h>

#include "cpu.h"
MC6502 cpu;

// begin kernal patcher

struct patch_t {
    uint16_t ea;
    void (*func)(const char* name);
    const char* name;
};
 struct patch_t patches[32];
 int npatches=0;

inline int patch_cmp (const void * _a, const void * _b) {
    struct patch_t* a = (struct patch_t*)_a;
    struct patch_t* b = (struct patch_t*)_b;
   return a->ea - b->ea;
}
void MC6502::enable_patches(int on)
{
}
void MC6502::setpatch(uint16_t ea, void (*func)(const char* name),const char* name)
{
    patches[npatches].ea = ea;
    patches[npatches].func = func;
    patches[npatches].name = name;
    npatches++;
    qsort(patches, npatches, sizeof(patches[0]), patch_cmp);
}

patch_t* patch_lookup(uint16_t _pc)
{
    if(npatches > 0)
    {
        int low = 0;
        int high = npatches - 1;
        while(low < high)
        {
            int mid = low + (high-low)/2;
            if(patches[mid].ea < _pc)
                low = mid + 1;
            else
                high = mid;
        }
        if(patches[low].ea == _pc)
            return patches+low;
    }
    return nullptr;
}

void MC6502::jmp() {
    pc = ea;

    patch_t* p = patch_lookup(ea);
    if (p) p->func(p->name);
}

// end kernal patcher

void MC6502::jsr() {
    push16(pc - 1);
    pc = ea;
    patch_t* p = patch_lookup(ea);
    if (p) p->func(p->name);
}

void MC6502::nmi()
{
    push16(pc);
    status |= FLAG_CONSTANT;
    push8(status);
    status |= FLAG_INTERRUPT;
    pc = (uint16_t)peek(0xFFFA) | ((uint16_t)peek(0xFFFB) << 8);
}

void MC6502::irq()
{
    status |= FLAG_CONSTANT;
    if (status & FLAG_INTERRUPT) return;
    push16(pc);
    push8(status);
    status |= FLAG_INTERRUPT;
    pc = (uint16_t)peek(0xFFFE) | ((uint16_t)peek(0xFFFF) << 8);
}

void MC6502::init()
{
    n_breakpoints = 0;
    cyclehack = 0;
}

void MC6502::addBreakpoint(uint16_t _pc, uint8_t _flags, uint8_t _a, uint8_t _x, uint8_t _y)
{
    if (n_breakpoints>=BREAKPOINT_COUNT) return;
    Breakpoint* bp = breakpoints+n_breakpoints;
    n_breakpoints++;
    bp->pc = _pc;
    bp->a = _a;
    bp->x = _x;
    bp->y = _y;
    bp->flags = _flags;
}

Breakpoint* MC6502::hitsBreakpoint()
{
    Breakpoint* bp = nullptr;
    if (n_breakpoints==0) return bp;
    for (int i=0; i<n_breakpoints; ++i)
    {
        if (breakpoints[i].pc!=pc) continue;
        bp = breakpoints+i;
    }
    if (bp==nullptr) return nullptr;

    if (bp->flags & Breakpoint::ENABLE)
    {
        if ((bp->flags & Breakpoint::CONDITION_A) && bp->a != cpu.a) return nullptr;
        if ((bp->flags & Breakpoint::CONDITION_X) && bp->x != cpu.x) return nullptr;
        if ((bp->flags & Breakpoint::CONDITION_Y) && bp->y != cpu.y) return nullptr;

        if (onHit) onHit(bp);

        return bp;
    }
    return nullptr;
}

Breakpoint* MC6502::findBreakpoint(uint16_t _pc)
{
    for (int i=0; i<n_breakpoints; ++i)
    {
        if (breakpoints[i].pc==_pc) return breakpoints+i;
    }
    return nullptr;
}

void MC6502::removeBreakpoint(uint16_t _pc)
{
    int i=0;
    for (;i<n_breakpoints; ++i)
    {
        if (breakpoints[i].pc!=_pc) continue;
        n_breakpoints--;
        if (i != n_breakpoints)
        {
            breakpoints[i] = breakpoints[n_breakpoints];
        }
        return;
    }
}

void MC6502::reset()
{
    poke(0,0x2F);
    poke(1,0x37);
    pc = (uint16_t)peek(0xFFFC) | ((uint16_t)peek(0xFFFD) << 8);
    a = 0;
    x = 0;
    y = 0;
    sp = 0xFD;
    cycles = 0;
    status = FLAG_CONSTANT;
    paused = 0;
    cyclehack = 0;
}

void MC6502::clock()
{
    if (--cycles>cyclehack) return;
    opcode = peek(pc); pc++;

    switch(opcode)
    {
        case 0x0: // brk 
        {
            cycles=7;
            brk();
        }
        break;
        case 0x1: // ora (ea,x)
        {
            cycles=6;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            ora();
        }
        break;
        case 0x2: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0x3: // slo (ea,x)
        {
            cycles=8;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            slo();
        }
        break;
        case 0x4: // nop (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            nop();
        }
        break;
        case 0x5: // ora (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            ora();
        }
        break;
        case 0x6: // asl (zp)
        {
            cycles=5;
            ea = (uint16_t)peek(pc++);
            asl();
        }
        break;
        case 0x7: // slo (zp)
        {
            cycles=5;
            ea = (uint16_t)peek(pc++);
            slo();
        }
        break;
        case 0x8: // php 
        {
            cycles=3;
            php();
        }
        break;
        case 0x9: // ora #imm
        {
            cycles=2;
            ea = pc++;
            ora();
        }
        break;
        case 0xa: // asl_a a
        {
            cycles=2;
            asl_a();
        }
        break;
        case 0xb: // nop #imm
        {
            cycles=2;
            ea = pc++;
            nop();
        }
        break;
        case 0xc: // nop ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            nop();
        }
        break;
        case 0xd: // ora ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            ora();
        }
        break;
        case 0xe: // asl ea
        {
            cycles=6;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            asl();
        }
        break;
        case 0xf: // slo ea
        {
            cycles=6;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            slo();
        }
        break;
        case 0x10: // bpl rea
        {
            cycles=2;
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bpl();
        }
        break;
        case 0x11: // ora (ea),y
        {
            cycles=5;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            ora();
        }
        break;
        case 0x12: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0x13: // slo (ea),y
        {
            cycles=8;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            slo();
        }
        break;
        case 0x14: // nop (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            nop();
        }
        break;
        case 0x15: // ora (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            ora();
        }
        break;
        case 0x16: // asl (zp),x
        {
            cycles=6;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            asl();
        }
        break;
        case 0x17: // slo (zp),x
        {
            cycles=6;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            slo();
        }
        break;
        case 0x18: // clc 
        {
            cycles=2;
            clc();
        }
        break;
        case 0x19: // ora ea,y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            ora();
        }
        break;
        case 0x1a: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0x1b: // slo ea,y
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            slo();
        }
        break;
        case 0x1c: // noP ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0x1d: // ora ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            ora();
        }
        break;
        case 0x1e: // asl ea,x
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            asl();
        }
        break;
        case 0x1f: // slo ea,x
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            slo();
        }
        break;
        case 0x20: // jsr ea
        {
            cycles=6;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            jsr();
        }
        break;
        case 0x21: // and (ea,x)
        {
            cycles=6;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            And();
        }
        break;
        case 0x22: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0x23: // rla (ea,x)
        {
            cycles=8;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            rla();
        }
        break;
        case 0x24: // bit (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            Bit();
        }
        break;
        case 0x25: // and (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            And();
        }
        break;
        case 0x26: // rol (zp)
        {
            cycles=5;
            ea = (uint16_t)peek(pc++);
            rol();
        }
        break;
        case 0x27: // rla (zp)
        {
            cycles=5;
            ea = (uint16_t)peek(pc++);
            rla();
        }
        break;
        case 0x28: // plp 
        {
            cycles=4;
            plp();
        }
        break;
        case 0x29: // and #imm
        {
            cycles=2;
            ea = pc++;
            And();
        }
        break;
        case 0x2a: // rol_a a
        {
            cycles=2;
            rol_a();
        }
        break;
        case 0x2b: // nop #imm
        {
            cycles=2;
            ea = pc++;
            nop();
        }
        break;
        case 0x2c: // bit ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            Bit();
        }
        break;
        case 0x2d: // and ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            And();
        }
        break;
        case 0x2e: // rol ea
        {
            cycles=6;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            rol();
        }
        break;
        case 0x2f: // rla ea
        {
            cycles=6;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            rla();
        }
        break;
        case 0x30: // bmi rea
        {
            cycles=2;
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bmi();
        }
        break;
        case 0x31: // and (ea),y
        {
            cycles=5;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            And();
        }
        break;
        case 0x32: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0x33: // rla (ea),y
        {
            cycles=8;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            rla();
        }
        break;
        case 0x34: // nop (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            nop();
        }
        break;
        case 0x35: // and (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            And();
        }
        break;
        case 0x36: // rol (zp),x
        {
            cycles=6;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            rol();
        }
        break;
        case 0x37: // rla (zp),x
        {
            cycles=6;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            rla();
        }
        break;
        case 0x38: // sec 
        {
            cycles=2;
            sec();
        }
        break;
        case 0x39: // and ea,y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            And();
        }
        break;
        case 0x3a: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0x3b: // rla ea,y
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            rla();
        }
        break;
        case 0x3c: // noP ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0x3d: // and ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            And();
        }
        break;
        case 0x3e: // rol ea,x
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            rol();
        }
        break;
        case 0x3f: // rla ea,x
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            rla();
        }
        break;
        case 0x40: // rti 
        {
            cycles=6;
            rti();
        }
        break;
        case 0x41: // eor (ea,x)
        {
            cycles=6;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            eor();
        }
        break;
        case 0x42: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0x43: // sre (ea,x)
        {
            cycles=8;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            sre();
        }
        break;
        case 0x44: // nop (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            nop();
        }
        break;
        case 0x45: // eor (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            eor();
        }
        break;
        case 0x46: // lsr (zp)
        {
            cycles=5;
            ea = (uint16_t)peek(pc++);
            lsr();
        }
        break;
        case 0x47: // sre (zp)
        {
            cycles=5;
            ea = (uint16_t)peek(pc++);
            sre();
        }
        break;
        case 0x48: // pha 
        {
            cycles=3;
            pha();
        }
        break;
        case 0x49: // eor #imm
        {
            cycles=2;
            ea = pc++;
            eor();
        }
        break;
        case 0x4a: // lsr_a a
        {
            cycles=2;
            lsr_a();
        }
        break;
        case 0x4b: // nop #imm
        {
            cycles=2;
            ea = pc++;
            nop();
        }
        break;
        case 0x4c: // jmp ea
        {
            cycles=3;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            jmp();
        }
        break;
        case 0x4d: // eor ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            eor();
        }
        break;
        case 0x4e: // lsr ea
        {
            cycles=6;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            lsr();
        }
        break;
        case 0x4f: // sre ea
        {
            cycles=6;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            sre();
        }
        break;
        case 0x50: // bvc rea
        {
            cycles=2;
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bvc();
        }
        break;
        case 0x51: // eor (ea),y
        {
            cycles=5;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            eor();
        }
        break;
        case 0x52: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0x53: // sre (ea),y
        {
            cycles=8;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            sre();
        }
        break;
        case 0x54: // nop (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            nop();
        }
        break;
        case 0x55: // eor (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            eor();
        }
        break;
        case 0x56: // lsr (zp),x
        {
            cycles=6;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            lsr();
        }
        break;
        case 0x57: // sre (zp),x
        {
            cycles=6;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            sre();
        }
        break;
        case 0x58: // cli 
        {
            cycles=2;
            cli6502();
        }
        break;
        case 0x59: // eor ea,y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            eor();
        }
        break;
        case 0x5a: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0x5b: // sre ea,y
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            sre();
        }
        break;
        case 0x5c: // noP ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0x5d: // eor ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            eor();
        }
        break;
        case 0x5e: // lsr ea,x
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            lsr();
        }
        break;
        case 0x5f: // sre ea,x
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            sre();
        }
        break;
        case 0x60: // rts 
        {
            cycles=6;
            rts();
        }
        break;
        case 0x61: // adc (ea,x)
        {
            cycles=6;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            adc();
        }
        break;
        case 0x62: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0x63: // rra (ea,x)
        {
            cycles=8;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            rra();
        }
        break;
        case 0x64: // nop (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            nop();
        }
        break;
        case 0x65: // adc (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            adc();
        }
        break;
        case 0x66: // ror (zp)
        {
            cycles=5;
            ea = (uint16_t)peek(pc++);
            ror();
        }
        break;
        case 0x67: // rra (zp)
        {
            cycles=5;
            ea = (uint16_t)peek(pc++);
            rra();
        }
        break;
        case 0x68: // pla 
        {
            cycles=4;
            pla();
        }
        break;
        case 0x69: // adc #imm
        {
            cycles=2;
            ea = pc++;
            adc();
        }
        break;
        case 0x6a: // ror_a a
        {
            cycles=2;
            ror_a();
        }
        break;
        case 0x6b: // nop #imm
        {
            cycles=2;
            ea = pc++;
            nop();
        }
        break;
        case 0x6c: // jmp (ea)
        {
            cycles=5;
            uint16_t u = (uint16_t)peek(pc++) | (uint16_t)((uint16_t)peek(pc++) << 8);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0xFF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            jmp();
        }
        break;
        case 0x6d: // adc ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            adc();
        }
        break;
        case 0x6e: // ror ea
        {
            cycles=6;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            ror();
        }
        break;
        case 0x6f: // rra ea
        {
            cycles=6;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            rra();
        }
        break;
        case 0x70: // bvs rea
        {
            cycles=2;
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bvs();
        }
        break;
        case 0x71: // adc (ea),y
        {
            cycles=5;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            adc();
        }
        break;
        case 0x72: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0x73: // rra (ea),y
        {
            cycles=8;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            rra();
        }
        break;
        case 0x74: // nop (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            nop();
        }
        break;
        case 0x75: // adc (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            adc();
        }
        break;
        case 0x76: // ror (zp),x
        {
            cycles=6;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            ror();
        }
        break;
        case 0x77: // rra (zp),x
        {
            cycles=6;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            rra();
        }
        break;
        case 0x78: // sei 
        {
            cycles=2;
            sei6502();
        }
        break;
        case 0x79: // adc ea,y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            adc();
        }
        break;
        case 0x7a: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0x7b: // rra ea,y
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            rra();
        }
        break;
        case 0x7c: // noP ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0x7d: // adc ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            adc();
        }
        break;
        case 0x7e: // ror ea,x
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            ror();
        }
        break;
        case 0x7f: // rra ea,x
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            rra();
        }
        break;
        case 0x80: // nop #imm
        {
            cycles=2;
            ea = pc++;
            nop();
        }
        break;
        case 0x81: // sta (ea,x)
        {
            cycles=6;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            sta();
        }
        break;
        case 0x82: // nop #imm
        {
            cycles=2;
            ea = pc++;
            nop();
        }
        break;
        case 0x83: // sax (ea,x)
        {
            cycles=6;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            sax();
        }
        break;
        case 0x84: // sty (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            sty();
        }
        break;
        case 0x85: // sta (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            sta();
        }
        break;
        case 0x86: // stx (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            stx();
        }
        break;
        case 0x87: // sax (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            sax();
        }
        break;
        case 0x88: // dey 
        {
            cycles=2;
            dey();
        }
        break;
        case 0x89: // nop #imm
        {
            cycles=2;
            ea = pc++;
            nop();
        }
        break;
        case 0x8a: // txa 
        {
            cycles=2;
            txa();
        }
        break;
        case 0x8b: // nop #imm
        {
            cycles=2;
            ea = pc++;
            nop();
        }
        break;
        case 0x8c: // sty ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            sty();
        }
        break;
        case 0x8d: // sta ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            sta();
        }
        break;
        case 0x8e: // stx ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            stx();
        }
        break;
        case 0x8f: // sax ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            sax();
        }
        break;
        case 0x90: // bcc rea
        {
            cycles=2;
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bcc();
        }
        break;
        case 0x91: // sta (ea),y
        {
            cycles=6;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            sta();
        }
        break;
        case 0x92: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0x93: // nop (ea),y
        {
            cycles=6;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            nop();
        }
        break;
        case 0x94: // sty (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            sty();
        }
        break;
        case 0x95: // sta (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            sta();
        }
        break;
        case 0x96: // stx (zp),y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            stx();
        }
        break;
        case 0x97: // sax (zp),y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            sax();
        }
        break;
        case 0x98: // tya 
        {
            cycles=2;
            tya();
        }
        break;
        case 0x99: // sta ea,y
        {
            cycles=5;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            sta();
        }
        break;
        case 0x9a: // txs 
        {
            cycles=2;
            txs();
        }
        break;
        case 0x9b: // nop ea,y
        {
            cycles=5;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            nop();
        }
        break;
        case 0x9c: // noP ea,x
        {
            cycles=5;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0x9d: // sta ea,x
        {
            cycles=5;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            sta();
        }
        break;
        case 0x9e: // nop ea,y
        {
            cycles=5;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            nop();
        }
        break;
        case 0x9f: // nop ea,y
        {
            cycles=5;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            nop();
        }
        break;
        case 0xa0: // ldy #imm
        {
            cycles=2;
            ea = pc++;
            ldy();
        }
        break;
        case 0xa1: // lda (ea,x)
        {
            cycles=6;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            lda();
        }
        break;
        case 0xa2: // ldx #imm
        {
            cycles=2;
            ea = pc++;
            ldx();
        }
        break;
        case 0xa3: // lax (ea,x)
        {
            cycles=6;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            lax();
        }
        break;
        case 0xa4: // ldy (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            ldy();
        }
        break;
        case 0xa5: // lda (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            lda();
        }
        break;
        case 0xa6: // ldx (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            ldx();
        }
        break;
        case 0xa7: // lax (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            lax();
        }
        break;
        case 0xa8: // tay 
        {
            cycles=2;
            tay();
        }
        break;
        case 0xa9: // lda #imm
        {
            cycles=2;
            ea = pc++;
            lda();
        }
        break;
        case 0xaa: // tax 
        {
            cycles=2;
            tax();
        }
        break;
        case 0xab: // nop #imm
        {
            cycles=2;
            ea = pc++;
            nop();
        }
        break;
        case 0xac: // ldy ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            ldy();
        }
        break;
        case 0xad: // lda ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            lda();
        }
        break;
        case 0xae: // ldx ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            ldx();
        }
        break;
        case 0xaf: // lax ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            lax();
        }
        break;
        case 0xb0: // bcs rea
        {
            cycles=2;
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bcs();
        }
        break;
        case 0xb1: // lda (ea),y
        {
            cycles=5;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            lda();
        }
        break;
        case 0xb2: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0xb3: // lax (ea),y
        {
            cycles=5;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            lax();
        }
        break;
        case 0xb4: // ldy (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            ldy();
        }
        break;
        case 0xb5: // lda (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            lda();
        }
        break;
        case 0xb6: // ldx (zp),y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            ldx();
        }
        break;
        case 0xb7: // lax (zp),y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            lax();
        }
        break;
        case 0xb8: // clv 
        {
            cycles=2;
            clv();
        }
        break;
        case 0xb9: // lda ea,y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            lda();
        }
        break;
        case 0xba: // tsx 
        {
            cycles=2;
            tsx();
        }
        break;
        case 0xbb: // lax ea,y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            lax();
        }
        break;
        case 0xbc: // ldy ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            ldy();
        }
        break;
        case 0xbd: // lda ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            lda();
        }
        break;
        case 0xbe: // ldx ea,y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            ldx();
        }
        break;
        case 0xbf: // lax ea,y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            lax();
        }
        break;
        case 0xc0: // cpy #imm
        {
            cycles=2;
            ea = pc++;
            cpy();
        }
        break;
        case 0xc1: // cmp (ea,x)
        {
            cycles=6;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            cmp();
        }
        break;
        case 0xc2: // nop #imm
        {
            cycles=2;
            ea = pc++;
            nop();
        }
        break;
        case 0xc3: // dcp (ea,x)
        {
            cycles=8;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            dcp();
        }
        break;
        case 0xc4: // cpy (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            cpy();
        }
        break;
        case 0xc5: // cmp (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            cmp();
        }
        break;
        case 0xc6: // dec (zp)
        {
            cycles=5;
            ea = (uint16_t)peek(pc++);
            dec();
        }
        break;
        case 0xc7: // dcp (zp)
        {
            cycles=5;
            ea = (uint16_t)peek(pc++);
            dcp();
        }
        break;
        case 0xc8: // iny 
        {
            cycles=2;
            iny();
        }
        break;
        case 0xc9: // cmp #imm
        {
            cycles=2;
            ea = pc++;
            cmp();
        }
        break;
        case 0xca: // dex 
        {
            cycles=2;
            dex();
        }
        break;
        case 0xcb: // nop #imm
        {
            cycles=2;
            ea = pc++;
            nop();
        }
        break;
        case 0xcc: // cpy ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            cpy();
        }
        break;
        case 0xcd: // cmp ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            cmp();
        }
        break;
        case 0xce: // dec ea
        {
            cycles=6;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            dec();
        }
        break;
        case 0xcf: // dcp ea
        {
            cycles=6;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            dcp();
        }
        break;
        case 0xd0: // bne rea
        {
            cycles=2;
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bne();
        }
        break;
        case 0xd1: // cmp (ea),y
        {
            cycles=5;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            cmp();
        }
        break;
        case 0xd2: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0xd3: // dcp (ea),y
        {
            cycles=8;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            dcp();
        }
        break;
        case 0xd4: // nop (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            nop();
        }
        break;
        case 0xd5: // cmp (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            cmp();
        }
        break;
        case 0xd6: // dec (zp),x
        {
            cycles=6;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            dec();
        }
        break;
        case 0xd7: // dcp (zp),x
        {
            cycles=6;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            dcp();
        }
        break;
        case 0xd8: // cld 
        {
            cycles=2;
            cld();
        }
        break;
        case 0xd9: // cmp ea,y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            cmp();
        }
        break;
        case 0xda: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0xdb: // dcp ea,y
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            dcp();
        }
        break;
        case 0xdc: // noP ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0xdd: // cmp ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            cmp();
        }
        break;
        case 0xde: // dec ea,x
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            dec();
        }
        break;
        case 0xdf: // dcp ea,x
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            dcp();
        }
        break;
        case 0xe0: // cpx #imm
        {
            cycles=2;
            ea = pc++;
            cpx();
        }
        break;
        case 0xe1: // sbc (ea,x)
        {
            cycles=6;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            sbc();
        }
        break;
        case 0xe2: // nop #imm
        {
            cycles=2;
            ea = pc++;
            nop();
        }
        break;
        case 0xe3: // isb (ea,x)
        {
            cycles=8;
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            isb();
        }
        break;
        case 0xe4: // cpx (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            cpx();
        }
        break;
        case 0xe5: // sbc (zp)
        {
            cycles=3;
            ea = (uint16_t)peek(pc++);
            sbc();
        }
        break;
        case 0xe6: // inc (zp)
        {
            cycles=5;
            ea = (uint16_t)peek(pc++);
            inc();
        }
        break;
        case 0xe7: // isb (zp)
        {
            cycles=5;
            ea = (uint16_t)peek(pc++);
            isb();
        }
        break;
        case 0xe8: // inx 
        {
            cycles=2;
            inx();
        }
        break;
        case 0xe9: // sbc #imm
        {
            cycles=2;
            ea = pc++;
            sbc();
        }
        break;
        case 0xea: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0xeb: // sbc #imm
        {
            cycles=2;
            ea = pc++;
            sbc();
        }
        break;
        case 0xec: // cpx ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            cpx();
        }
        break;
        case 0xed: // sbc ea
        {
            cycles=4;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            sbc();
        }
        break;
        case 0xee: // inc ea
        {
            cycles=6;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            inc();
        }
        break;
        case 0xef: // isb ea
        {
            cycles=6;
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            isb();
        }
        break;
        case 0xf0: // beq rea
        {
            cycles=2;
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            beq();
        }
        break;
        case 0xf1: // sbc (ea),y
        {
            cycles=5;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            sbc();
        }
        break;
        case 0xf2: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0xf3: // isb (ea),y
        {
            cycles=8;
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            isb();
        }
        break;
        case 0xf4: // nop (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            nop();
        }
        break;
        case 0xf5: // sbc (zp),x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            sbc();
        }
        break;
        case 0xf6: // inc (zp),x
        {
            cycles=6;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            inc();
        }
        break;
        case 0xf7: // isb (zp),x
        {
            cycles=6;
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            isb();
        }
        break;
        case 0xf8: // sed 
        {
            cycles=2;
            sed();
        }
        break;
        case 0xf9: // sbc ea,y
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            sbc();
        }
        break;
        case 0xfa: // nop 
        {
            cycles=2;
            nop();
        }
        break;
        case 0xfb: // isb ea,y
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            isb();
        }
        break;
        case 0xfc: // noP ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0xfd: // sbc ea,x
        {
            cycles=4;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            sbc();
        }
        break;
        case 0xfe: // inc ea,x
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            inc();
        }
        break;
        case 0xff: // isb ea,x
        {
            cycles=7;
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            isb();
        }
        break;
    }
}

void MC6502::fastclock()
{
    opcode = peek(pc); pc++;

    switch(opcode)
    {
        case 0x0: // brk 
        {
            brk();
        }
        break;
        case 0x1: // ora (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            ora();
        }
        break;
        case 0x2: // nop 
        {
            nop();
        }
        break;
        case 0x3: // slo (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            slo();
        }
        break;
        case 0x4: // nop (zp)
        {
            ea = (uint16_t)peek(pc++);
            nop();
        }
        break;
        case 0x5: // ora (zp)
        {
            ea = (uint16_t)peek(pc++);
            ora();
        }
        break;
        case 0x6: // asl (zp)
        {
            ea = (uint16_t)peek(pc++);
            asl();
        }
        break;
        case 0x7: // slo (zp)
        {
            ea = (uint16_t)peek(pc++);
            slo();
        }
        break;
        case 0x8: // php 
        {
            php();
        }
        break;
        case 0x9: // ora #imm
        {
            ea = pc++;
            ora();
        }
        break;
        case 0xa: // asl_a a
        {
            asl_a();
        }
        break;
        case 0xb: // nop #imm
        {
            ea = pc++;
            nop();
        }
        break;
        case 0xc: // nop ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            nop();
        }
        break;
        case 0xd: // ora ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            ora();
        }
        break;
        case 0xe: // asl ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            asl();
        }
        break;
        case 0xf: // slo ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            slo();
        }
        break;
        case 0x10: // bpl rea
        {
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bpl();
        }
        break;
        case 0x11: // ora (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            ora();
        }
        break;
        case 0x12: // nop 
        {
            nop();
        }
        break;
        case 0x13: // slo (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            slo();
        }
        break;
        case 0x14: // nop (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            nop();
        }
        break;
        case 0x15: // ora (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            ora();
        }
        break;
        case 0x16: // asl (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            asl();
        }
        break;
        case 0x17: // slo (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            slo();
        }
        break;
        case 0x18: // clc 
        {
            clc();
        }
        break;
        case 0x19: // ora ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            ora();
        }
        break;
        case 0x1a: // nop 
        {
            nop();
        }
        break;
        case 0x1b: // slo ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            slo();
        }
        break;
        case 0x1c: // noP ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0x1d: // ora ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            ora();
        }
        break;
        case 0x1e: // asl ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            asl();
        }
        break;
        case 0x1f: // slo ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            slo();
        }
        break;
        case 0x20: // jsr ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            jsr();
        }
        break;
        case 0x21: // and (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            And();
        }
        break;
        case 0x22: // nop 
        {
            nop();
        }
        break;
        case 0x23: // rla (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            rla();
        }
        break;
        case 0x24: // bit (zp)
        {
            ea = (uint16_t)peek(pc++);
            Bit();
        }
        break;
        case 0x25: // and (zp)
        {
            ea = (uint16_t)peek(pc++);
            And();
        }
        break;
        case 0x26: // rol (zp)
        {
            ea = (uint16_t)peek(pc++);
            rol();
        }
        break;
        case 0x27: // rla (zp)
        {
            ea = (uint16_t)peek(pc++);
            rla();
        }
        break;
        case 0x28: // plp 
        {
            plp();
        }
        break;
        case 0x29: // and #imm
        {
            ea = pc++;
            And();
        }
        break;
        case 0x2a: // rol_a a
        {
            rol_a();
        }
        break;
        case 0x2b: // nop #imm
        {
            ea = pc++;
            nop();
        }
        break;
        case 0x2c: // bit ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            Bit();
        }
        break;
        case 0x2d: // and ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            And();
        }
        break;
        case 0x2e: // rol ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            rol();
        }
        break;
        case 0x2f: // rla ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            rla();
        }
        break;
        case 0x30: // bmi rea
        {
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bmi();
        }
        break;
        case 0x31: // and (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            And();
        }
        break;
        case 0x32: // nop 
        {
            nop();
        }
        break;
        case 0x33: // rla (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            rla();
        }
        break;
        case 0x34: // nop (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            nop();
        }
        break;
        case 0x35: // and (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            And();
        }
        break;
        case 0x36: // rol (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            rol();
        }
        break;
        case 0x37: // rla (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            rla();
        }
        break;
        case 0x38: // sec 
        {
            sec();
        }
        break;
        case 0x39: // and ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            And();
        }
        break;
        case 0x3a: // nop 
        {
            nop();
        }
        break;
        case 0x3b: // rla ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            rla();
        }
        break;
        case 0x3c: // noP ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0x3d: // and ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            And();
        }
        break;
        case 0x3e: // rol ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            rol();
        }
        break;
        case 0x3f: // rla ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            rla();
        }
        break;
        case 0x40: // rti 
        {
            rti();
        }
        break;
        case 0x41: // eor (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            eor();
        }
        break;
        case 0x42: // nop 
        {
            nop();
        }
        break;
        case 0x43: // sre (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            sre();
        }
        break;
        case 0x44: // nop (zp)
        {
            ea = (uint16_t)peek(pc++);
            nop();
        }
        break;
        case 0x45: // eor (zp)
        {
            ea = (uint16_t)peek(pc++);
            eor();
        }
        break;
        case 0x46: // lsr (zp)
        {
            ea = (uint16_t)peek(pc++);
            lsr();
        }
        break;
        case 0x47: // sre (zp)
        {
            ea = (uint16_t)peek(pc++);
            sre();
        }
        break;
        case 0x48: // pha 
        {
            pha();
        }
        break;
        case 0x49: // eor #imm
        {
            ea = pc++;
            eor();
        }
        break;
        case 0x4a: // lsr_a a
        {
            lsr_a();
        }
        break;
        case 0x4b: // nop #imm
        {
            ea = pc++;
            nop();
        }
        break;
        case 0x4c: // jmp ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            jmp();
        }
        break;
        case 0x4d: // eor ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            eor();
        }
        break;
        case 0x4e: // lsr ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            lsr();
        }
        break;
        case 0x4f: // sre ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            sre();
        }
        break;
        case 0x50: // bvc rea
        {
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bvc();
        }
        break;
        case 0x51: // eor (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            eor();
        }
        break;
        case 0x52: // nop 
        {
            nop();
        }
        break;
        case 0x53: // sre (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            sre();
        }
        break;
        case 0x54: // nop (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            nop();
        }
        break;
        case 0x55: // eor (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            eor();
        }
        break;
        case 0x56: // lsr (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            lsr();
        }
        break;
        case 0x57: // sre (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            sre();
        }
        break;
        case 0x58: // cli 
        {
            cli6502();
        }
        break;
        case 0x59: // eor ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            eor();
        }
        break;
        case 0x5a: // nop 
        {
            nop();
        }
        break;
        case 0x5b: // sre ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            sre();
        }
        break;
        case 0x5c: // noP ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0x5d: // eor ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            eor();
        }
        break;
        case 0x5e: // lsr ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            lsr();
        }
        break;
        case 0x5f: // sre ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            sre();
        }
        break;
        case 0x60: // rts 
        {
            rts();
        }
        break;
        case 0x61: // adc (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            adc();
        }
        break;
        case 0x62: // nop 
        {
            nop();
        }
        break;
        case 0x63: // rra (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            rra();
        }
        break;
        case 0x64: // nop (zp)
        {
            ea = (uint16_t)peek(pc++);
            nop();
        }
        break;
        case 0x65: // adc (zp)
        {
            ea = (uint16_t)peek(pc++);
            adc();
        }
        break;
        case 0x66: // ror (zp)
        {
            ea = (uint16_t)peek(pc++);
            ror();
        }
        break;
        case 0x67: // rra (zp)
        {
            ea = (uint16_t)peek(pc++);
            rra();
        }
        break;
        case 0x68: // pla 
        {
            pla();
        }
        break;
        case 0x69: // adc #imm
        {
            ea = pc++;
            adc();
        }
        break;
        case 0x6a: // ror_a a
        {
            ror_a();
        }
        break;
        case 0x6b: // nop #imm
        {
            ea = pc++;
            nop();
        }
        break;
        case 0x6c: // jmp (ea)
        {
            uint16_t u = (uint16_t)peek(pc++) | (uint16_t)((uint16_t)peek(pc++) << 8);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0xFF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            jmp();
        }
        break;
        case 0x6d: // adc ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            adc();
        }
        break;
        case 0x6e: // ror ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            ror();
        }
        break;
        case 0x6f: // rra ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            rra();
        }
        break;
        case 0x70: // bvs rea
        {
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bvs();
        }
        break;
        case 0x71: // adc (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            adc();
        }
        break;
        case 0x72: // nop 
        {
            nop();
        }
        break;
        case 0x73: // rra (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            rra();
        }
        break;
        case 0x74: // nop (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            nop();
        }
        break;
        case 0x75: // adc (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            adc();
        }
        break;
        case 0x76: // ror (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            ror();
        }
        break;
        case 0x77: // rra (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            rra();
        }
        break;
        case 0x78: // sei 
        {
            sei6502();
        }
        break;
        case 0x79: // adc ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            adc();
        }
        break;
        case 0x7a: // nop 
        {
            nop();
        }
        break;
        case 0x7b: // rra ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            rra();
        }
        break;
        case 0x7c: // noP ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0x7d: // adc ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            adc();
        }
        break;
        case 0x7e: // ror ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            ror();
        }
        break;
        case 0x7f: // rra ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            rra();
        }
        break;
        case 0x80: // nop #imm
        {
            ea = pc++;
            nop();
        }
        break;
        case 0x81: // sta (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            sta();
        }
        break;
        case 0x82: // nop #imm
        {
            ea = pc++;
            nop();
        }
        break;
        case 0x83: // sax (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            sax();
        }
        break;
        case 0x84: // sty (zp)
        {
            ea = (uint16_t)peek(pc++);
            sty();
        }
        break;
        case 0x85: // sta (zp)
        {
            ea = (uint16_t)peek(pc++);
            sta();
        }
        break;
        case 0x86: // stx (zp)
        {
            ea = (uint16_t)peek(pc++);
            stx();
        }
        break;
        case 0x87: // sax (zp)
        {
            ea = (uint16_t)peek(pc++);
            sax();
        }
        break;
        case 0x88: // dey 
        {
            dey();
        }
        break;
        case 0x89: // nop #imm
        {
            ea = pc++;
            nop();
        }
        break;
        case 0x8a: // txa 
        {
            txa();
        }
        break;
        case 0x8b: // nop #imm
        {
            ea = pc++;
            nop();
        }
        break;
        case 0x8c: // sty ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            sty();
        }
        break;
        case 0x8d: // sta ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            sta();
        }
        break;
        case 0x8e: // stx ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            stx();
        }
        break;
        case 0x8f: // sax ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            sax();
        }
        break;
        case 0x90: // bcc rea
        {
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bcc();
        }
        break;
        case 0x91: // sta (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            sta();
        }
        break;
        case 0x92: // nop 
        {
            nop();
        }
        break;
        case 0x93: // nop (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            nop();
        }
        break;
        case 0x94: // sty (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            sty();
        }
        break;
        case 0x95: // sta (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            sta();
        }
        break;
        case 0x96: // stx (zp),y
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            stx();
        }
        break;
        case 0x97: // sax (zp),y
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            sax();
        }
        break;
        case 0x98: // tya 
        {
            tya();
        }
        break;
        case 0x99: // sta ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            sta();
        }
        break;
        case 0x9a: // txs 
        {
            txs();
        }
        break;
        case 0x9b: // nop ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            nop();
        }
        break;
        case 0x9c: // noP ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0x9d: // sta ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            sta();
        }
        break;
        case 0x9e: // nop ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            nop();
        }
        break;
        case 0x9f: // nop ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            nop();
        }
        break;
        case 0xa0: // ldy #imm
        {
            ea = pc++;
            ldy();
        }
        break;
        case 0xa1: // lda (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            lda();
        }
        break;
        case 0xa2: // ldx #imm
        {
            ea = pc++;
            ldx();
        }
        break;
        case 0xa3: // lax (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            lax();
        }
        break;
        case 0xa4: // ldy (zp)
        {
            ea = (uint16_t)peek(pc++);
            ldy();
        }
        break;
        case 0xa5: // lda (zp)
        {
            ea = (uint16_t)peek(pc++);
            lda();
        }
        break;
        case 0xa6: // ldx (zp)
        {
            ea = (uint16_t)peek(pc++);
            ldx();
        }
        break;
        case 0xa7: // lax (zp)
        {
            ea = (uint16_t)peek(pc++);
            lax();
        }
        break;
        case 0xa8: // tay 
        {
            tay();
        }
        break;
        case 0xa9: // lda #imm
        {
            ea = pc++;
            lda();
        }
        break;
        case 0xaa: // tax 
        {
            tax();
        }
        break;
        case 0xab: // nop #imm
        {
            ea = pc++;
            nop();
        }
        break;
        case 0xac: // ldy ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            ldy();
        }
        break;
        case 0xad: // lda ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            lda();
        }
        break;
        case 0xae: // ldx ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            ldx();
        }
        break;
        case 0xaf: // lax ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            lax();
        }
        break;
        case 0xb0: // bcs rea
        {
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bcs();
        }
        break;
        case 0xb1: // lda (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            lda();
        }
        break;
        case 0xb2: // nop 
        {
            nop();
        }
        break;
        case 0xb3: // lax (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            lax();
        }
        break;
        case 0xb4: // ldy (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            ldy();
        }
        break;
        case 0xb5: // lda (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            lda();
        }
        break;
        case 0xb6: // ldx (zp),y
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            ldx();
        }
        break;
        case 0xb7: // lax (zp),y
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            lax();
        }
        break;
        case 0xb8: // clv 
        {
            clv();
        }
        break;
        case 0xb9: // lda ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            lda();
        }
        break;
        case 0xba: // tsx 
        {
            tsx();
        }
        break;
        case 0xbb: // lax ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            lax();
        }
        break;
        case 0xbc: // ldy ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            ldy();
        }
        break;
        case 0xbd: // lda ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            lda();
        }
        break;
        case 0xbe: // ldx ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            ldx();
        }
        break;
        case 0xbf: // lax ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            lax();
        }
        break;
        case 0xc0: // cpy #imm
        {
            ea = pc++;
            cpy();
        }
        break;
        case 0xc1: // cmp (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            cmp();
        }
        break;
        case 0xc2: // nop #imm
        {
            ea = pc++;
            nop();
        }
        break;
        case 0xc3: // dcp (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            dcp();
        }
        break;
        case 0xc4: // cpy (zp)
        {
            ea = (uint16_t)peek(pc++);
            cpy();
        }
        break;
        case 0xc5: // cmp (zp)
        {
            ea = (uint16_t)peek(pc++);
            cmp();
        }
        break;
        case 0xc6: // dec (zp)
        {
            ea = (uint16_t)peek(pc++);
            dec();
        }
        break;
        case 0xc7: // dcp (zp)
        {
            ea = (uint16_t)peek(pc++);
            dcp();
        }
        break;
        case 0xc8: // iny 
        {
            iny();
        }
        break;
        case 0xc9: // cmp #imm
        {
            ea = pc++;
            cmp();
        }
        break;
        case 0xca: // dex 
        {
            dex();
        }
        break;
        case 0xcb: // nop #imm
        {
            ea = pc++;
            nop();
        }
        break;
        case 0xcc: // cpy ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            cpy();
        }
        break;
        case 0xcd: // cmp ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            cmp();
        }
        break;
        case 0xce: // dec ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            dec();
        }
        break;
        case 0xcf: // dcp ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            dcp();
        }
        break;
        case 0xd0: // bne rea
        {
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            bne();
        }
        break;
        case 0xd1: // cmp (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            cmp();
        }
        break;
        case 0xd2: // nop 
        {
            nop();
        }
        break;
        case 0xd3: // dcp (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            dcp();
        }
        break;
        case 0xd4: // nop (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            nop();
        }
        break;
        case 0xd5: // cmp (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            cmp();
        }
        break;
        case 0xd6: // dec (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            dec();
        }
        break;
        case 0xd7: // dcp (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            dcp();
        }
        break;
        case 0xd8: // cld 
        {
            cld();
        }
        break;
        case 0xd9: // cmp ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            cmp();
        }
        break;
        case 0xda: // nop 
        {
            nop();
        }
        break;
        case 0xdb: // dcp ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            dcp();
        }
        break;
        case 0xdc: // noP ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0xdd: // cmp ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            cmp();
        }
        break;
        case 0xde: // dec ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            dec();
        }
        break;
        case 0xdf: // dcp ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            dcp();
        }
        break;
        case 0xe0: // cpx #imm
        {
            ea = pc++;
            cpx();
        }
        break;
        case 0xe1: // sbc (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            sbc();
        }
        break;
        case 0xe2: // nop #imm
        {
            ea = pc++;
            nop();
        }
        break;
        case 0xe3: // isb (ea,x)
        {
            uint16_t u = (uint16_t)(((uint16_t)peek(pc++) + (uint16_t)x) & 0xFF);
            ea = (uint16_t)peek(u & 0xFF) | ((uint16_t)peek((u + 1) & 0xFF) << 8);
            isb();
        }
        break;
        case 0xe4: // cpx (zp)
        {
            ea = (uint16_t)peek(pc++);
            cpx();
        }
        break;
        case 0xe5: // sbc (zp)
        {
            ea = (uint16_t)peek(pc++);
            sbc();
        }
        break;
        case 0xe6: // inc (zp)
        {
            ea = (uint16_t)peek(pc++);
            inc();
        }
        break;
        case 0xe7: // isb (zp)
        {
            ea = (uint16_t)peek(pc++);
            isb();
        }
        break;
        case 0xe8: // inx 
        {
            inx();
        }
        break;
        case 0xe9: // sbc #imm
        {
            ea = pc++;
            sbc();
        }
        break;
        case 0xea: // nop 
        {
            nop();
        }
        break;
        case 0xeb: // sbc #imm
        {
            ea = pc++;
            sbc();
        }
        break;
        case 0xec: // cpx ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            cpx();
        }
        break;
        case 0xed: // sbc ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            sbc();
        }
        break;
        case 0xee: // inc ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            inc();
        }
        break;
        case 0xef: // isb ea
        {
            ea = (uint16_t)peek(pc++) | ((uint16_t)peek(pc++)<<8);
            isb();
        }
        break;
        case 0xf0: // beq rea
        {
            ra = (uint16_t)peek(pc++); if (ra&0x80) ra |= 0xFF00;
            beq();
        }
        break;
        case 0xf1: // sbc (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            sbc();
        }
        break;
        case 0xf2: // nop 
        {
            nop();
        }
        break;
        case 0xf3: // isb (ea),y
        {
            uint16_t u = (uint16_t)peek(pc++);
            uint16_t v = (u & 0xFF00) | ((u + 1) & 0x00FF);
            ea = (uint16_t)peek(u) | ((uint16_t)peek(v) << 8);
            ea += (uint16_t)y;
            isb();
        }
        break;
        case 0xf4: // nop (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            nop();
        }
        break;
        case 0xf5: // sbc (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            sbc();
        }
        break;
        case 0xf6: // inc (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            inc();
        }
        break;
        case 0xf7: // isb (zp),x
        {
            ea = ((uint16_t)peek(pc++) + (uint16_t)x) & 0xff;
            isb();
        }
        break;
        case 0xf8: // sed 
        {
            sed();
        }
        break;
        case 0xf9: // sbc ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            sbc();
        }
        break;
        case 0xfa: // nop 
        {
            nop();
        }
        break;
        case 0xfb: // isb ea,y
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)y;
            isb();
        }
        break;
        case 0xfc: // noP ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            noP();
        }
        break;
        case 0xfd: // sbc ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            sbc();
        }
        break;
        case 0xfe: // inc ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            inc();
        }
        break;
        case 0xff: // isb ea,x
        {
            ea = ((uint16_t)peek(pc++) | ((uint16_t)peek(pc++) << 8)); ea += (uint16_t)x;
            isb();
        }
        break;
    }
}
