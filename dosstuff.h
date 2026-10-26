#ifndef VGMCONV_DOSSTUFF_H
#define VGMCONV_DOSSTUFF_H

void dosstuff_stop_timer(void);

void dosstuff_start_timer(void);

void dosstuff_read_timer(unsigned long *res);

void dosstuff_sleep_ms(unsigned long ms);

void dosstuff_write_reg(unsigned port, int reg, int data);

#endif
