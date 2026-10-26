#ifndef VGMCONV_NA2L3_H
#define VGMCONV_NA2L3_H

#include "ntypes.h"

void init_opna_to_opl3();

void opna_to_opl3(u8_t reg, u8_t data, u8_t port_idx);

void init_ssg_to_opl3();

void ssg_to_opl3(u8_t reg, u8_t data, u8_t port_idx);

void init_ryt_to_opl3();

void ryt_to_opl3(u8_t reg, u8_t data, u8_t port_idx);

#endif
