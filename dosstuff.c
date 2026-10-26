#include <dos.h>
#include <stdlib.h>

#include "dosstuff.h"

#ifdef PC98
#define PIT_DIVISOR 2458
#define PIT_TARGET 24576
#define PIC_PORT 0x00
#define PIT_CH0_PORT 0x71
#define PIT_CTRL_PORT 0x77
#define PIT_CTRL_VAL 0x34
#define PIC_VAL 0x60
#else
#define PIT_DIVISOR 1165
#define PIT_TARGET 65536
#define PIC_PORT 0x20
#define PIT_CH0_PORT 0x40
#define PIT_CTRL_PORT 0x43
#define PIT_CTRL_VAL 0x36
#define PIC_VAL 0x20
#endif

static void (_interrupt _far *old_timer)(void);
static volatile unsigned long current_time = 0;

static void _interrupt _far new_timer(void) {
    static int needs_reinit = 0;
    static volatile unsigned long count = 0;
    ++current_time;
    count += PIT_DIVISOR;
    if (count >= PIT_TARGET) {
        count -= PIT_TARGET;
        needs_reinit = 1;
        _chain_intr(old_timer);
    } else {
        if (needs_reinit) {
            needs_reinit = 0;
            outp(PIT_CTRL_PORT, PIT_CTRL_VAL);
            outp(PIT_CH0_PORT, PIT_DIVISOR & 0xff);
            outp(PIT_CH0_PORT, (PIT_DIVISOR >> 8) & 0xff);
        }
        outp(PIC_PORT, PIC_VAL);
    }
}

void dosstuff_stop_timer(void) {
    _disable();
    _dos_setvect(0x08, old_timer);
    outp(PIT_CTRL_PORT, PIT_CTRL_VAL);
    outp(PIT_CH0_PORT, 0x00);
    outp(PIT_CH0_PORT, 0x00);
    _enable();
}

void dosstuff_start_timer(void) {
    unsigned char mask;
    old_timer = _dos_getvect(0x08);
    current_time = 0;
    _disable();
    _dos_setvect(0x08, new_timer);
    outp(PIT_CTRL_PORT, PIT_CTRL_VAL);
    outp(PIT_CH0_PORT, PIT_DIVISOR & 0xff);
    outp(PIT_CH0_PORT, (PIT_DIVISOR >> 8) & 0xff);
#ifdef PC98
    mask = inp(0x02);
    outp(0x02, mask & ~0x01);
#endif
    _enable();
    atexit(dosstuff_stop_timer);
}

void dosstuff_read_timer(unsigned long *res) {
    _disable();
    *res = current_time;
    _enable();
}

void dosstuff_sleep_ms(unsigned long ms) {
    unsigned long t1, t2;
    dosstuff_read_timer(&t1);
    for (;;) {
        dosstuff_read_timer(&t2);
        if (t2 < t1) {
            break;
        } else if (t2 - t1 >= ms) {
            break;
        }
    }
}

void dosstuff_write_reg(unsigned port, int reg, int data) {
    int i;
    outp(port, reg);
    i = 6;
    while (i--) inp(port);
#ifdef PC98
    outp(port + 2, data);
#else
    outp(port + 1, data);
#endif
    i = 6;
    while (i--) inp(port);
}
