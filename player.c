/*

ALGORITHM_CONV_MAP:
    ALG0 -> CNT0 = 0, CNT1 = 0
    
*/

#include "dosstuff.h"
#include "player.h"
#include "na2l3.h"
#include <stdlib.h>
#include <stdio.h>

void player_setup() {
    dosstuff_start_timer();
#ifdef PC98
    outp(0xa460, (inp(0xa460) | 1) & 0xfd);
#else
    init_opna_to_opl3();
#endif
}

void player_wait(u32_t samples) {
    dosstuff_sleep_ms((samples * 1000) / 44100);
}

void player_write_register(u8_t port_id, u8_t reg_id, u8_t reg_data) {
#ifdef PC98
    dosstuff_write_reg(port_id ? 0x18c : 0x188, reg_id, reg_data);
#else
    opna_to_opl3(reg_id, reg_data, port_id);
#endif
}

void player_write_data(u32_t addr, u8_t data) {
#ifdef PC98
    addr >>= 5;
    dosstuff_write_reg(0x18c, 0x00, 0x60);
    dosstuff_write_reg(0x18c, 0x02, addr & 0xff);
    dosstuff_write_reg(0x18c, 0x03, (addr >> 8) & 0xff);
    dosstuff_write_reg(0x18c, 0x08, data);
    dosstuff_write_reg(0x18c, 0x00, 0x00);
#endif
}
