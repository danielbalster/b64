/*

I lost track from where this originates exactly, I assume it's a blend of

http://rubbermallet.org/fake6502.c
https://github.com/dirkwhoffmann/virtualc64/blob/master/Emulator/CPU/CPUInstructions.cpp

*/

#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define FLAG_CARRY     0x01
#define FLAG_ZERO      0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL   0x08
#define FLAG_BREAK     0x10
#define FLAG_CONSTANT  0x20
#define FLAG_OVERFLOW  0x40
#define FLAG_SIGN      0x80

extern uint8_t peek(uint16_t address);
extern void poke(uint16_t address, uint8_t value);

#define getvalue16() ((uint16_t)peek(ea) | ((uint16_t)peek(ea+1) << 8))

#define push8(__val__) poke(0x100 + sp--, __val__)
#define pull8() (uint8_t)(peek(0x100 + ++sp))

#define setcarry() status |= (FLAG_CARRY|FLAG_CONSTANT)
#define clearcarry() status &= (~FLAG_CARRY)
#define setzero() status |= (FLAG_ZERO|FLAG_CONSTANT)
#define clearzero() status &= (~FLAG_ZERO)
#define setinterrupt() status |= (FLAG_INTERRUPT|FLAG_CONSTANT)
#define clearinterrupt() status &= (~FLAG_INTERRUPT)
#define setdecimal() status |= (FLAG_DECIMAL|FLAG_CONSTANT)
#define cleardecimal() status &= (~FLAG_DECIMAL)
#define setoverflow() status |= (FLAG_OVERFLOW|FLAG_CONSTANT)
#define clearoverflow() status &= (~FLAG_OVERFLOW)
#define setsign() status |= (FLAG_SIGN|FLAG_CONSTANT)
#define clearsign() status &= (~FLAG_SIGN)


#define zerocalc(n) {\
    if (((n) & 0x00FF)==0) setzero();\
}

#define signcalc(n) {\
    status |= n&0x80;\
}

#define carrycalc(n) {\
    status |= (n>>8)&1;\
}

#define overflowcalc(n, m, o) {\
    status |= (((n) ^ (uint16_t)(m)) & ((n) ^ (o)) & 0x0080) >> 1;\
}

struct Breakpoint
{
    enum {
        ENABLE=1,
        TRACE=2,
        CONDITION_A=4,
        CONDITION_X=8,
        CONDITION_Y=16,
    };
    uint16_t pc;
    uint8_t a,x,y,flags;
};

struct MC6502 {
    int16_t cycles;
    uint16_t pc,ea,ra;
    uint8_t sp,a,x,y,status,opcode;
    int cyclehack;
    uint32_t clockcycles;

    struct {
        uint16_t pc;
        uint8_t sp,a,x,y,status;
    } last;
    
    uint8_t paused;
    static const int BREAKPOINT_COUNT = 20;
    Breakpoint breakpoints[BREAKPOINT_COUNT];
    int n_breakpoints;
    void addBreakpoint(uint16_t _pc, uint8_t _flags=Breakpoint::ENABLE, uint8_t _a=0, uint8_t _x=0, uint8_t _y=0);
    void removeBreakpoint(uint16_t _pc);
    Breakpoint* hitsBreakpoint();
    void (*onHit)(Breakpoint*);
    Breakpoint* findBreakpoint(uint16_t _pc);

    static uint16_t swap(uint16_t v)
    {
        return ((v>>8)&255) | ((v&255)<<8);
    }

    void setpatch(uint16_t ea, void (*func)(const char* name),const char* name);
    void enable_patches(int on);
    void reset();
    void irq();
    void init();
    void nmi();
    void clock();
    void fastclock();
    void stepIn()
    {
        last.pc = pc;
        last.sp = sp;
        last.a = a;
        last.x = x;
        last.y = y;
        last.status = status;
        cycles=1;
        clock();
    }
    void stepOut()
    {
        last.pc = pc;
        last.sp = sp;
        last.a = a;
        last.x = x;
        last.y = y;
        last.status = status;
        do
        {
            cycles=1;
            clock();
        }
        while ((opcode!=0x40) && (opcode!=0x60) && (last.pc != pc));   // RTS=0x60, RTI=0x40 and avoid infinite loop
    }
    void stepOver()
    {
        if (opcode==0x20)
        {
            stepOut();
        }
        else
        {
            stepIn();
        }
    }
    uint16_t getEffectiveAddress();

    
// a few general functions used by various other functions
inline void push16(uint16_t pushval)
{
    poke(0x100 + sp, (pushval >> 8) & 0xFF);
    poke(0x100 + ((sp - 1) & 0xFF), pushval & 0xFF);
    sp -= 2;
}


inline uint16_t pull16()
{
    uint16_t t = peek(0x100 + ((sp + 1) & 0xFF)) | ((uint16_t)peek(0x100 + ((sp + 2) & 0xFF)) << 8);
    sp += 2;
    return t;
}

//instruction handler functions
inline void adc()
{
    uint16_t value = peek(ea);
    uint16_t result = (uint16_t)a + value + (uint16_t)(status & FLAG_CARRY);
   
    if (unlikely(status & FLAG_DECIMAL)) {

        uint8_t hi = (a >> 4) + (value >> 4);
        uint8_t lo = (a & 0x0F) + (value & 0x0F) + ((uint16_t)(status & FLAG_CARRY));

        // Check for overflow conditions
        // If an overflow occurs on a BCD digit, it needs to be fixed by adding the pseudo-tetrade 0110 (=6)
        if (lo > 9) {
            lo = lo + 6;
        }
        if (lo > 0x0F) {
            hi++;
        }
        
        status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_OVERFLOW|FLAG_SIGN);
        if ((result & 0xFF) == 0) setzero();
        if (hi & 0x08) setsign();
        if ((((hi << 4) ^ a) & 0x80) && !((a ^ value) & 0x80)) setoverflow();
        
        if (hi > 9) {
            hi = (hi + 6);
        }
        if (hi > 0x0F)
            setcarry();
        
        lo &= 0x0F;
        a = (uint8_t)((hi << 4) | lo);
        return;
    }
    status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_OVERFLOW|FLAG_SIGN);
    carrycalc(result);
    zerocalc(result);
    overflowcalc(result, a, value);
    signcalc(result);

    a=(uint8_t)result;
}

inline void And()
{
    uint16_t value = peek(ea);
    uint16_t result = (uint16_t)a & value;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(result);
    signcalc(result);
   
    a=(uint8_t)result;
}

inline void asl()
{
    uint16_t value = (uint16_t)peek(ea);
    uint16_t result = value << 1;

    status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_SIGN);
    carrycalc(result);
    zerocalc(result);
    signcalc(result);
   
    poke(ea,result);
}

inline void asl_a()
{
    uint16_t value = (uint16_t)a;
    uint16_t result = value << 1;

    status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_SIGN);
    carrycalc(result);
    zerocalc(result);
    signcalc(result);

    a=(uint8_t)result;
}

inline void bcc()
{
    if ((status & FLAG_CARRY) == 0) {
        //oldpc = pc;
        pc += ra;
//        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
//            else clockticks6502++;
    }
}

inline void bcs()
{
    if ((status & FLAG_CARRY) == FLAG_CARRY) {
        //oldpc = pc;
        pc += ra;
//        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
//            else clockticks6502++;
    }
}

inline void beq()
{
    if ((status & FLAG_ZERO) == FLAG_ZERO) {
        //oldpc = pc;
        pc += ra;
//        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
//            else clockticks6502++;
    }
}

inline void Bit()
{
    uint16_t value = peek(ea);
    uint16_t result = (uint16_t)a & value;
   
    status &=~ (FLAG_ZERO);
    zerocalc(result);
    status = (status & 0x3F) | (value & 0xC0);
}

inline void bmi()
{
    if ((status & FLAG_SIGN) == FLAG_SIGN) {
        //oldpc = pc;
        pc += ra;
//        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
//            else clockticks6502++;
    }
}

inline void bne()
{
    if ((status & FLAG_ZERO) == 0) {
        //oldpc = pc;
        pc += ra;
//        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
//            else clockticks6502++;
    }
}

inline void bpl()
{
    if ((status & FLAG_SIGN) == 0) {
        //oldpc = pc;
        pc += ra;
//        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
//            else clockticks6502++;
    }
}

inline void brk()
{
    pc++;
    status |= FLAG_CONSTANT;
    push16(pc); //push next instruction address onto stack
    push8(status | FLAG_BREAK); //push CPU status to stack
    setinterrupt(); //set interrupt flag
    pc = (uint16_t)peek(0xFFFE) | ((uint16_t)peek(0xFFFF) << 8);
}

inline void bvc()
{
    if ((status & FLAG_OVERFLOW) == 0) {
        //oldpc = pc;
        pc += ra;
//        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
//            else clockticks6502++;
    }
}

inline void bvs()
{
    if ((status & FLAG_OVERFLOW) == FLAG_OVERFLOW) {
        //oldpc = pc;
        pc += ra;
//        if ((oldpc & 0xFF00) != (pc & 0xFF00)) clockticks6502 += 2; //check if jump crossed a page boundary
//            else clockticks6502++;
    }
}

inline void clc()
{
    clearcarry();
}

inline void cld()
{
    cleardecimal();
}

inline void cli6502()
{
    clearinterrupt();
}

inline void clv()
{
    clearoverflow();
}

inline void cmp()
{
    uint16_t value = peek(ea);
    uint16_t result = (uint16_t)a - value;
   
    status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_SIGN);
    if (unlikely(a >= (uint8_t)(value & 0x00FF))) setcarry();
    if (unlikely(a == (uint8_t)(value & 0x00FF))) setzero();
    signcalc(result);
}

inline void cpx()
{
    uint16_t value = peek(ea);
    uint16_t result = (uint16_t)x - value;
   
    status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_SIGN);
    if (unlikely(x >= (uint8_t)(value & 0x00FF))) setcarry();
    if (unlikely(x == (uint8_t)(value & 0x00FF))) setzero();
    signcalc(result);
}

inline void cpy()
{
    uint16_t value = peek(ea);
    uint16_t result = (uint16_t)y - value;
   
    status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_SIGN);
    if (unlikely(y >= (uint8_t)(value & 0x00FF))) setcarry();
    if (unlikely(y == (uint8_t)(value & 0x00FF))) setzero();
    signcalc(result);
}

inline void dec()
{
    uint16_t value = peek(ea);
    uint16_t result = value - 1;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(result);
    signcalc(result);
   
    poke(ea,result);
}

inline void dex()
{
    x--;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(x);
    signcalc(x);
}

inline void dey()
{
    y--;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(y);
    signcalc(y);
}

inline void eor()
{
    uint16_t value = peek(ea);
    uint16_t result = (uint16_t)a ^ value;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(result);
    signcalc(result);
   
    a=(uint8_t)result;
}

inline void inc()
{
    uint16_t value = peek(ea);
    uint16_t result = value + 1;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(result);
    signcalc(result);
   
    poke(ea,result);
}

inline void inx()
{
    x++;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(x);
    signcalc(x);
}

inline void iny()
{
    y++;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(y);
    signcalc(y);
}

void jmp();
void jsr();

inline void lda()
{
    uint16_t value = peek(ea);
    a = (uint8_t)(value & 0x00FF);
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(a);
    signcalc(a);
}

inline void ldx()
{
    uint16_t value = peek(ea);
    x = (uint8_t)(value & 0x00FF);
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(x);
    signcalc(x);
}

inline void ldy()
{
    uint16_t value = peek(ea);
    y = (uint8_t)(value & 0x00FF);
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(y);
    signcalc(y);
}

inline void lsr()
{
    uint16_t value = peek(ea);
    uint16_t result = value >> 1;
   
    status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_SIGN);
    if (value & 1) setcarry();
    zerocalc(result);
    signcalc(result);
   
    poke(ea,result);
}

inline void lsr_a()
{
    uint16_t value = (uint16_t)a;
    uint16_t result = value >> 1;
   
    status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_SIGN);
    if (value & 1) setcarry();
    zerocalc(result);
    signcalc(result);
    a=(uint8_t)result;
}

inline void nop()
{
}

inline void noP()
{
}

inline void ora()
{
    uint16_t value = peek(ea);
    uint16_t result = (uint16_t)a | value;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(result);
    signcalc(result);
   
    a=(uint8_t)result;
}

inline void pha()
{
    push8(a);
}

inline void php()
{
    push8(status);// | FLAG_BREAK);
}

inline void pla()
{
    a = pull8();
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(a);
    signcalc(a);
}

inline void plp()
{
    status = pull8() | FLAG_CONSTANT;
}

inline void rol()
{
    uint16_t value = peek(ea);
    uint16_t result = (value << 1) | (status & FLAG_CARRY);
   
    status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_SIGN);
    carrycalc(result);
    zerocalc(result);
    signcalc(result);
   
    poke(ea,result);
}

inline void rol_a()
{
    uint16_t value =  (uint16_t)a;
    uint16_t result = (value << 1) | (status & FLAG_CARRY);
   
    status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_SIGN);
    carrycalc(result);
    zerocalc(result);
    signcalc(result);
    a=(uint8_t)result;
}

inline void ror()
{
    uint16_t value = peek(ea);
    uint16_t result = (value >> 1) | ((status & FLAG_CARRY) << 7);
   
    status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_SIGN);
    if (value & 1) setcarry();
    zerocalc(result);
    signcalc(result);
   
    poke(ea,result);
}

inline void ror_a()
{
    uint16_t value =  (uint16_t)a;
    uint16_t result = (value >> 1) | ((status & FLAG_CARRY) << 7);
   
    status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_SIGN);
    if (value & 1) setcarry();
    zerocalc(result);
    signcalc(result);
    a=(uint8_t)result;
}

inline void rti()
{
    status = pull8() | FLAG_CONSTANT;
    uint16_t value = pull16();
    pc = value;
}

inline void rts()
{
    uint16_t value = pull16();
    pc = value + 1;
}

inline void sbc()
{
    uint16_t value = peek(ea) ^ 0x00FF;
    uint16_t result = (uint16_t)a + value + (uint16_t)(status & FLAG_CARRY);

    if (unlikely(status & FLAG_DECIMAL)) {
        uint8_t  hi = (a >> 4) - (value >> 4);
        uint8_t  lo  = (a & 0x0F) - (value & 0x0F) - (status & FLAG_CARRY);
        
        // Check for underflow conditions
        // If an overflow occurs on a BCD digit, it needs to be fixed by subtracting the pseudo-tetrade 0110 (=6)
        if (lo & 0x10) {
            lo = lo - 6;
            hi--;
        }
        if (hi & 0x10) {
            hi = hi - 6;
        }
        
        status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_OVERFLOW|FLAG_SIGN);
        if (result < 0x100) setcarry();
        if (((a ^ result) & 0x80) && ((a ^ value) & 0x80)) setoverflow();
        if ((result & 0xFF) == 0) setzero();
        if (result & 0x80) setsign();
        
        a = (uint8_t)((hi << 4) | (lo & 0x0f));

        return;        
    }
   
    status &=~ (FLAG_CARRY|FLAG_ZERO|FLAG_OVERFLOW|FLAG_SIGN);
    carrycalc(result);
    zerocalc(result);
    overflowcalc(result, a, value);
    signcalc(result);
    a=(uint8_t)result;
}

inline void sec()
{
    setcarry();
}

inline void sed()
{
    setdecimal();
}

inline void sei6502()
{
    setinterrupt();
}

inline void sta()
{
    poke(ea,a);
}

inline void stx()
{
    poke(ea,x);
}

inline void sty()
{
    poke(ea,y);
}

inline void tax()
{
    x = a;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(x);
    signcalc(x);
}

inline void tay()
{
    y = a;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(y);
    signcalc(y);
}

inline void tsx()
{
    x = sp;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(x);
    signcalc(x);
}

inline void txa()
{
    a = x;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(a);
    signcalc(a);
}

inline void txs()
{
    sp = x;
}

inline void tya()
{
    a = y;
   
    status &=~ (FLAG_ZERO|FLAG_SIGN);
    zerocalc(a);
    signcalc(a);
}

    inline void lax() {
        lda();
        ldx();
    }

    inline void sax() {
        sta();
        stx();
        poke(ea,a & x);
    }

    inline void dcp() {
        dec();
        cmp();
    }

    inline void isb() {
        inc();
        sbc();
    }

    inline void slo() {
        asl();
        ora();
    }

    inline void rla() {
        rol();
        And();
    }

    inline void sre() {
        lsr();
        eor();
    }

    inline void rra() {
        ror();
        adc();
    }


};

extern MC6502 cpu;

#endif
