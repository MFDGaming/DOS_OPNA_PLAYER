#ifndef VGMCONV_PLAYER_H
#define VGMCONV_PLAYER_H

#include "ntypes.h"

void player_setup();

void player_wait(u32_t samples);

void player_write_register(u8_t port_id, u8_t reg_id, u8_t reg_data);

void player_write_data(u32_t addr, u8_t data);

#endif
