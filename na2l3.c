#include <math.h>
#include <string.h>

#include "dosstuff.h"
#include "na2l3.h"
#include "opna.h"
#include "opl3.h"

#define OPL3_MASTER_CLOCK 14318180.0
#define OPNA_MASTER_CLOCK 7987200.0

typedef struct {
    u8_t multi[16];
    u8_t dt[16];
    u8_t tl[16];
    u8_t ar[16];
    u8_t dr[16];
    u8_t rr[16];
    u8_t sl[16];
    u8_t sr[16];
    u8_t ks[16];
    u8_t amon[16];
    u8_t ssg_eg[16];
    u8_t pms[4];
    u8_t ams[4];
    u8_t block[4];
    u16_t fnum[4];
    u8_t connect[4];
    u8_t fb[4];
    u8_t ch[8];
    u8_t r[4];
    u8_t l[4];
    u8_t opl3_conn;
} vopna_state_t;

vopna_state_t v_state_p0;
vopna_state_t v_state_p1;

vopna_state_t *v_states[2] = {&v_state_p0, &v_state_p1};

u8_t op_map[16] = {
    /*CH0-op0-0*/ 0x00, /*CH1-op0-1*/ 0x01, /*CH2-op0-2*/ 0x02, /*RESERVED-3*/ 0x06,
    /*CH0-op1-4*/ 0x08, /*CH1-op1-5*/ 0x09, /*CH2-op1-6*/ 0x0a, /*RESERVED-7*/ 0x07,
    /*CH0-op2-8*/ 0x03, /*CH1-op2-9*/ 0x04, /*CH2-op2-a*/ 0x05, /*RESERVED-b*/ 0x0e,
    /*CH0-op3-c*/ 0x0b, /*CH1-op3-d*/ 0x0c, /*CH2-op3-e*/ 0x0d, /*RESERVED-f*/ 0x0f
};

u8_t op_to_ch_map[16] = {
    /*CH0-op0-0*/ 0x00, /*CH1-op0-1*/ 0x01, /*CH2-op0-2*/ 0x02, /*RESERVED-3*/ 0x03,
    /*CH0-op1-4*/ 0x00, /*CH1-op1-5*/ 0x01, /*CH2-op1-6*/ 0x02, /*RESERVED-7*/ 0x03,
    /*CH0-op2-8*/ 0x00, /*CH1-op2-9*/ 0x01, /*CH2-op2-a*/ 0x02, /*RESERVED-b*/ 0x03,
    /*CH0-op3-c*/ 0x00, /*CH1-op3-d*/ 0x01, /*CH2-op3-e*/ 0x02, /*RESERVED-f*/ 0x03
};

/*
FULL CONV:
ALG0 -> CONN=1 CNT0=0 CNT1=0
ALG4 -> CONN=0 CNT0=0 CNT1=0
ALG6 -> CONN=0 CNT0=0 CNT1=1
ALG7 -> CONN=0 CNT0=1 CNT1=1

APPROXIMATE CONV:
ALG1 -> CONN=1 CNT0=0 CNT1=0
ALG2 -> CONN=1 CNT0=0 CNT1=0
ALG3 -> CONN=1 CNT0=0 CNT1=0
ALG5 -> CONN=0 CNT0=0 CNT1=1

*/

u8_t connection_map[8] = {1, 1, 1, 1, 0, 0, 0, 0};
u8_t cnt0_map[8] =       {0, 0, 0, 0, 0, 0, 0, 1};
u8_t cnt1_map[8] =       {0, 0, 0, 0, 0, 1, 1, 1};

u8_t slot_bit[4] = {1, 2, 4, 8};

void init_opna_to_opl3() {
    memset(&v_state_p0, 0, sizeof(v_state_p0));
    memset(&v_state_p1, 0, sizeof(v_state_p1));
    v_state_p0.r[0] = 1;
    v_state_p0.r[1] = 1;
    v_state_p0.r[2] = 1;
    v_state_p0.r[3] = 1;

    v_state_p1.r[0] = 1;
    v_state_p1.r[1] = 1;
    v_state_p1.r[2] = 1;
    v_state_p1.r[3] = 1;
    dosstuff_write_reg(0x388, OPL3_REG_L_ST1_ST2_MT2_MT1_RST, 0x80);
    dosstuff_write_reg(0x38a, OPL3_REG_H_NEW, 1);
    init_ssg_to_opl3();
    init_ryt_to_opl3();
}

static u8_t rel_rate(u8_t rr) {
    return rr ? rr : 1;
}
 
static u8_t is_operator_active(u8_t idx, u8_t port_idx) {
    u8_t chan = v_state_p0.ch[op_to_ch_map[idx] + (port_idx << 2)];
    return !chan || (chan & slot_bit[idx >> 2]);
}

static u8_t is_operator_keyed(u8_t idx, u8_t port_idx) {
    return (v_state_p0.ch[op_to_ch_map[idx] + (port_idx << 2)] & slot_bit[idx >> 2]) ? 1 : 0;
}

static void opl_write_freq(u16_t port_base, u8_t ch, u16_t opna_fnum, u8_t opna_block, u8_t kon) {
    unsigned long fn = (unsigned long)(((double)opna_fnum * OPNA_MASTER_CLOCK) / OPL3_MASTER_CLOCK);
    u8_t block = opna_block;
    u8_t hi;
    while (fn > 1023 && block < 7) {
        fn >>= 1;
        ++block;
    }
    if (fn > 1023) fn = 1023;
    hi = (u8_t)(((fn >> 8) & 0x03) | (block << 2) | (kon << 5));
    dosstuff_write_reg(port_base, OPL3_REG_F_NUMBER_L_0 + ch, (u8_t)(fn & 0xff));
    dosstuff_write_reg(port_base, OPL3_REG_F_NUMBER_L_3 + ch, (u8_t)(fn & 0xff));
    dosstuff_write_reg(port_base, OPL3_REG_FNUM_H_BLOCK_KON_0 + ch, hi);
    dosstuff_write_reg(port_base, OPL3_REG_FNUM_H_BLOCK_KON_3 + ch, hi);
}
 
void opna_to_opl3(u8_t reg, u8_t data, u8_t port_idx) {
    vopna_state_t *v_state = v_states[port_idx];
    u16_t port_base = 0x388 + (port_idx << 1);
 
    if (reg >= 0x30 && reg <= 0x9f && (reg & 3) == 3) return;

    if (reg == OPNA_FM_REG_CH_SLOT && port_idx == 0) {
        int i;
        u8_t ch_full = data & 0x07;
        u8_t ch_idx = ch_full;
        u8_t slot = data >> 4;
        u8_t prev;
        u8_t restart;
        u8_t reg_idx;
 
        if ((ch_full & 3) == 3) return;
 
        prev = v_state_p0.ch[ch_full];
        v_state_p0.ch[ch_full] = slot;
        if (ch_idx >= 4) {
            port_base = 0x38a;
            v_state = v_states[1];
            ch_idx %= 4;
        }
 
        restart = slot && (!prev || (slot & ~prev));
        if (restart && prev) {
            opl_write_freq(port_base, ch_idx, v_state->fnum[ch_idx], v_state->block[ch_idx], 0);
        }
 
        for (i = 0; i < 4; ++i) {
            u8_t on = (slot & slot_bit[i]) || !slot;
            u8_t ar, dr, rr, sl, egt;
            reg_idx = ch_idx + (i << 2);
            ar = v_state->ar[reg_idx];
            if (on) {
                dr = v_state->dr[reg_idx];
                rr = (slot && v_state->sr[reg_idx]) ? v_state->sr[reg_idx] : v_state->rr[reg_idx];
                sl = v_state->sl[reg_idx];
                egt = (slot && !v_state->sr[reg_idx]) ? 1 : 0;
            } else {
                dr = rel_rate(v_state->rr[reg_idx]);
                rr = 15;
                sl = 15;
                egt = 0;
                if (restart) ar = 0;
            }
            dosstuff_write_reg(port_base, OPL3_REG_TL_KSL_0 + op_map[reg_idx], v_state->tl[reg_idx]);
            dosstuff_write_reg(port_base, OPL3_REG_DR_AR_0 + op_map[reg_idx], dr | (ar << 4));
            dosstuff_write_reg(port_base, OPL3_REG_RR_SL_0 + op_map[reg_idx], rr | (sl << 4));
            dosstuff_write_reg(port_base, OPL3_REG_MULT_KSR_EGT_VIB_AM_0 + op_map[reg_idx],
                v_state->multi[reg_idx] |
                (v_state->ks[reg_idx] << 4) |
                (egt << 5) |
                ((v_state->pms[op_to_ch_map[reg_idx]] >= 2 ? 1 : 0) << 6) |
                (v_state->amon[reg_idx] << 7)
            );
        }
 
        opl_write_freq(port_base, ch_idx, v_state->fnum[ch_idx], v_state->block[ch_idx], slot ? 1 : 0);
    } else if (reg >= OPNA_FM_REG_TL_0 && reg <= OPNA_FM_REG_TL_11) {
        u8_t tl = data & 0x7f;
        u8_t idx = reg & 15;
        if (tl > 63) tl = 63;
        v_state->tl[idx] = tl;
        dosstuff_write_reg(port_base, OPL3_REG_TL_KSL_0 + op_map[idx], tl);
    } else if (reg >= OPNA_FM_REG_MULTI_DT_0 && reg <= OPNA_FM_REG_MULTI_DT_11) {
        u8_t multi = data & 0x0f;
        u8_t dt = (data >> 4) & 0x0f;
        u8_t idx = reg & 15;
        u8_t keyed = is_operator_keyed(idx, port_idx);
        u8_t opl3_data = (
            multi |
            (v_state->ks[idx] << 4) |
            (((keyed && !v_state->sr[idx]) ? 1 : 0) << 5) |
            ((v_state->pms[op_to_ch_map[idx]] >= 2 ? 1 : 0) << 6) |
            (v_state->amon[idx] << 7)
        );
        v_state->multi[idx] = multi;
        v_state->dt[idx] = dt;
        dosstuff_write_reg(port_base, OPL3_REG_MULT_KSR_EGT_VIB_AM_0 + op_map[idx], opl3_data);
    } else if (reg >= OPNA_FM_REG_AR_KS_0 && reg <= OPNA_FM_REG_AR_KS_11) {
        u8_t ar = (data & 0x1f) >> 1;
        u8_t ks = ((data >> 6) & 0x03) >> 1;
        u8_t idx = reg & 15;
        u8_t active = is_operator_active(idx, port_idx);
        u8_t keyed = is_operator_keyed(idx, port_idx);
        u8_t opl3_data = (
            v_state->multi[idx] |
            (ks << 4) |
            (((keyed && !v_state->sr[idx]) ? 1 : 0) << 5) |
            ((v_state->pms[op_to_ch_map[idx]] >= 2 ? 1 : 0) << 6) |
            (v_state->amon[idx] << 7)
        );
        v_state->ks[idx] = ks;
        v_state->ar[idx] = ar;
        dosstuff_write_reg(port_base, OPL3_REG_MULT_KSR_EGT_VIB_AM_0 + op_map[idx], opl3_data);
        opl3_data = (active ? v_state->dr[idx] : rel_rate(v_state->rr[idx])) | (ar << 4);
        dosstuff_write_reg(port_base, OPL3_REG_DR_AR_0 + op_map[idx], opl3_data);
    } else if (reg >= OPNA_FM_REG_DR_AMON_0 && reg <= OPNA_FM_REG_DR_AMON_11) {
        u8_t dr = (data & 0x1f) >> 1;
        u8_t amon = (data >> 7) & 0x01;
        u8_t idx = reg & 15;
        u8_t active = is_operator_active(idx, port_idx);
        u8_t keyed = is_operator_keyed(idx, port_idx);
        u8_t opl3_data = (
            v_state->multi[idx] |
            (v_state->ks[idx] << 4) |
            (((keyed && !v_state->sr[idx]) ? 1 : 0) << 5) |
            ((v_state->pms[op_to_ch_map[idx]] >= 2 ? 1 : 0) << 6) |
            (amon << 7)
        );
        v_state->amon[idx] = amon;
        v_state->dr[idx] = dr;
        dosstuff_write_reg(port_base, OPL3_REG_MULT_KSR_EGT_VIB_AM_0 + op_map[idx], opl3_data);
        opl3_data = (active ? dr : rel_rate(v_state->rr[idx])) | (v_state->ar[idx] << 4);
        dosstuff_write_reg(port_base, OPL3_REG_DR_AR_0 + op_map[idx], opl3_data);
    } else if (reg >= OPNA_FM_REG_RR_SL_0 && reg <= OPNA_FM_REG_RR_SL_11) {
        u8_t rr = data & 0x0f;
        u8_t sl = data >> 4;
        u8_t idx = reg & 15;
        u8_t active = is_operator_active(idx, port_idx);
        u8_t keyed = is_operator_keyed(idx, port_idx);
        u8_t opl3_data = (active ? ((v_state->sr[idx] && keyed) ? v_state->sr[idx] : rr) : 15) |
                         ((active ? sl : 15) << 4);
        v_state->rr[idx] = rr;
        v_state->sl[idx] = sl;
        dosstuff_write_reg(port_base, OPL3_REG_RR_SL_0 + op_map[idx], opl3_data);
    } else if (reg >= OPNA_FM_REG_SR_0 && reg <= OPNA_FM_REG_SR_11) {
        u8_t sr = (data & 0x1f) >> 1;
        u8_t idx = reg & 15;
        u8_t active = is_operator_active(idx, port_idx);
        u8_t keyed = is_operator_keyed(idx, port_idx);
        u8_t opl3_data = (active ? ((sr && keyed) ? sr : v_state->rr[idx]) : 15) |
                         ((active ? v_state->sl[idx] : 15) << 4);
        v_state->sr[idx] = sr;
        dosstuff_write_reg(port_base, OPL3_REG_RR_SL_0 + op_map[idx], opl3_data);
        opl3_data = (
            v_state->multi[idx] |
            (v_state->ks[idx] << 4) |
            (((keyed && !sr) ? 1 : 0) << 5) |
            ((v_state->pms[op_to_ch_map[idx]] >= 2 ? 1 : 0) << 6) |
            (v_state->amon[idx] << 7)
        );
        dosstuff_write_reg(port_base, OPL3_REG_MULT_KSR_EGT_VIB_AM_0 + op_map[idx], opl3_data);
    } else if (reg >= OPNA_FM_REG_FNUM1_0 && reg <= OPNA_FM_REG_FNUM1_2) {
        u8_t idx = reg - OPNA_FM_REG_FNUM1_0;
        u8_t ch_kon = v_state_p0.ch[idx + (port_idx << 2)] != 0;
        v_state->fnum[idx] = (v_state->fnum[idx] & 0xff00) | data;
        opl_write_freq(port_base, idx, v_state->fnum[idx], v_state->block[idx], ch_kon);
    } else if (reg >= OPNA_FM_REG_FNUM2_BLOCK_0 && reg <= OPNA_FM_REG_FNUM2_BLOCK_2) {
        u8_t idx = reg - OPNA_FM_REG_FNUM2_BLOCK_0;
        u8_t ch_kon = v_state_p0.ch[idx + (port_idx << 2)] != 0;
        v_state->fnum[idx] = (v_state->fnum[idx] & 0xff) | ((u16_t)(data & 0x07) << 8);
        v_state->block[idx] = (data >> 3) & 0x07;
        opl_write_freq(port_base, idx, v_state->fnum[idx], v_state->block[idx], ch_kon);
    } else if (reg >= OPNA_FM_REG_CONNECT_FB_0 && reg <= OPNA_FM_REG_CONNECT_FB_2) {
        u8_t idx = reg - OPNA_FM_REG_CONNECT_FB_0;
        u8_t connect = data & 0x07;
        u8_t fb = (data >> 3) & 0x07;
        u8_t bit = 1 << (idx + port_idx * 3);
        if (connection_map[connect]) {
            v_state_p1.opl3_conn |= bit;
        } else {
            v_state_p1.opl3_conn &= ~bit;
        }
        v_state->connect[idx] = connect;
        v_state->fb[idx] = fb;
        dosstuff_write_reg(port_base, OPL3_REG_CNT_FB_R_L_EX0_EX1_0 + idx, 0xc0 | (v_state->l[idx] << 4) | (v_state->r[idx] << 5) | (fb << 1) | cnt0_map[connect]);
        dosstuff_write_reg(port_base, OPL3_REG_CNT_FB_R_L_EX0_EX1_3 + idx, 0xc0 | (v_state->l[idx] << 4) | (v_state->r[idx] << 5) | cnt1_map[connect]);
        dosstuff_write_reg(0x38a, OPL3_REG_H_CONNECTION_SEL, v_state_p1.opl3_conn);
    } else if (reg >= OPNA_FM_REG_PMS_AMS_R_L_0 && reg <= OPNA_FM_REG_PMS_AMS_R_L_2) {
        u8_t idx = reg - OPNA_FM_REG_PMS_AMS_R_L_0;
        u8_t pms = data & 0x07;
        u8_t ams = (data >> 4) & 0x03;
        u8_t r = (data >> 6) & 0x01;
        u8_t l = data >> 7;
        v_state->pms[idx] = pms;
        v_state->ams[idx] = ams;
        v_state->r[idx] = r;
        v_state->l[idx] = l;
        dosstuff_write_reg(port_base, OPL3_REG_CNT_FB_R_L_EX0_EX1_0 + idx, 0xc0 | (l << 4) | (r << 5) | (v_state->fb[idx] << 1) | cnt0_map[v_state->connect[idx]]);
        dosstuff_write_reg(port_base, OPL3_REG_CNT_FB_R_L_EX0_EX1_3 + idx, 0xc0 | (l << 4) | (r << 5) | cnt1_map[v_state->connect[idx]]);
    }
    ssg_to_opl3(reg, data, port_idx);
    ryt_to_opl3(reg, data, port_idx);
}

u16_t ssg_note[3] = {0, 0, 0};
u8_t ssg_note_on = 0;
u8_t ssg_vol[3] = {0, 0, 0};

u8_t ssg_vol_map[16] = {63, 63, 63, 63, 60, 56, 52, 48, 44, 40, 36, 32, 28, 24, 20, 16};

static u8_t ssg_tl(u8_t vol) {
    unsigned tl;
    if (vol == 0) return 63;
    tl = 4 * (15 - vol) + 15;
    return (u8_t)(tl > 63 ? 63 : tl);
}

void init_ssg_to_opl3() {
    dosstuff_write_reg(0x38a, OPL3_REG_DR_AR_12, 0x00);
    dosstuff_write_reg(0x38a, OPL3_REG_DR_AR_15, 0xf0);
    dosstuff_write_reg(0x38a, OPL3_REG_RR_SL_12, 0x0f);
    dosstuff_write_reg(0x38a, OPL3_REG_RR_SL_15, 0x0f);
    dosstuff_write_reg(0x38a, OPL3_REG_TL_KSL_12, 0);
    dosstuff_write_reg(0x38a, OPL3_REG_TL_KSL_15, 63);
    dosstuff_write_reg(0x38a, OPL3_REG_MULT_KSR_EGT_VIB_AM_12, 1 | 0x20);
    dosstuff_write_reg(0x38a, OPL3_REG_MULT_KSR_EGT_VIB_AM_15, 1 | 0x20);
    dosstuff_write_reg(0x38a, OPL3_REG_WS_12, 6);
    dosstuff_write_reg(0x38a, OPL3_REG_WS_15, 6);

    dosstuff_write_reg(0x38a, OPL3_REG_DR_AR_13, 0x00);
    dosstuff_write_reg(0x38a, OPL3_REG_DR_AR_16, 0xf0);
    dosstuff_write_reg(0x38a, OPL3_REG_RR_SL_13, 0x0f);
    dosstuff_write_reg(0x38a, OPL3_REG_RR_SL_16, 0x0f);
    dosstuff_write_reg(0x38a, OPL3_REG_TL_KSL_13, 0);
    dosstuff_write_reg(0x38a, OPL3_REG_TL_KSL_16, 63);
    dosstuff_write_reg(0x38a, OPL3_REG_MULT_KSR_EGT_VIB_AM_13, 1 | 0x20);
    dosstuff_write_reg(0x38a, OPL3_REG_MULT_KSR_EGT_VIB_AM_16, 1 | 0x20);
    dosstuff_write_reg(0x38a, OPL3_REG_WS_13, 6);
    dosstuff_write_reg(0x38a, OPL3_REG_WS_16, 6);

    dosstuff_write_reg(0x38a, OPL3_REG_DR_AR_14, 0x00);
    dosstuff_write_reg(0x38a, OPL3_REG_DR_AR_17, 0xf0);
    dosstuff_write_reg(0x38a, OPL3_REG_RR_SL_14, 0x0f);
    dosstuff_write_reg(0x38a, OPL3_REG_RR_SL_17, 0x0f);
    dosstuff_write_reg(0x38a, OPL3_REG_TL_KSL_14, 0);
    dosstuff_write_reg(0x38a, OPL3_REG_TL_KSL_17, 63);
    dosstuff_write_reg(0x38a, OPL3_REG_MULT_KSR_EGT_VIB_AM_14, 1 | 0x20);
    dosstuff_write_reg(0x38a, OPL3_REG_MULT_KSR_EGT_VIB_AM_17, 1 | 0x20);
    dosstuff_write_reg(0x38a, OPL3_REG_WS_14, 6);
    dosstuff_write_reg(0x38a, OPL3_REG_WS_17, 6);
}

static void ssg_update(u8_t ch) {
    u16_t period = ssg_note[ch] ? ssg_note[ch] : 1;
    double ftone = OPNA_MASTER_CLOCK / (64.0 * period);
    unsigned long fnum = (unsigned long)((ftone * 301989888.0) / OPL3_MASTER_CLOCK);
    u8_t block = 0;
    u8_t data;
    u8_t kon = !((ssg_note_on >> ch) & 1);

    while (fnum > 1023 && block < 7) {
        fnum >>= 1;
        ++block;
    }
    if (fnum > 1023) fnum = 1023;

    if (ssg_vol[ch]) {
        dosstuff_write_reg(0x38a, OPL3_REG_TL_KSL_15 + ch, ssg_vol_map[ssg_vol[ch]]);
    } else {
        kon = 0;
    }

    dosstuff_write_reg(0x38a, OPL3_REG_F_NUMBER_L_6 + ch, fnum & 0xff);
    data = ((fnum >> 8) & 0x03) | (block << 2) | (kon << 5);
    dosstuff_write_reg(0x38a, OPL3_REG_FNUM_H_BLOCK_KON_6 + ch, data);
}

void ssg_to_opl3(u8_t reg, u8_t data, u8_t port_idx) {
    if (reg >= 0x00 && reg <= 0x05) {
        u8_t ch = reg >> 1;
        if (reg & 1) {
            ssg_note[ch] &= 0xff;
            ssg_note[ch] |= (data & 0x0f) << 8;
        } else {
            ssg_note[ch] &= 0xff00;
            ssg_note[ch] |= data;
        }
        ssg_update(ch);
    } else if (reg == 0x07) {
        int ch;
        ssg_note_on = (data & 0x07);
        for (ch = 0; ch < 3; ++ch) {
            ssg_update(ch);
        }
    } else if (reg >= 0x08 && reg <= 0x0a) {
        u8_t ch = reg - 8;
        if ((data >> 4) & 1) {
            ssg_vol[ch] = 0;
        } else {
            ssg_vol[ch] = data & 0x0f;
        }
        ssg_update(ch);
    }
}

void init_ryt_to_opl3() {
    unsigned long fnum;
    u8_t block = 0;
    u8_t data;
    dosstuff_write_reg(0x388, OPL3_REG_HH_TC_TOM_SD_BD_RYT_DVB_DAM, 0x20);

    /* BD */
    dosstuff_write_reg(0x388, OPL3_REG_DR_AR_12, 0xfc);
    dosstuff_write_reg(0x388, OPL3_REG_DR_AR_15, 0xfa);
    dosstuff_write_reg(0x388, OPL3_REG_RR_SL_12, 0xc3);
    dosstuff_write_reg(0x388, OPL3_REG_RR_SL_15, 0x06);
    dosstuff_write_reg(0x388, OPL3_REG_TL_KSL_12, 11);
    dosstuff_write_reg(0x388, OPL3_REG_TL_KSL_15, 11);
    dosstuff_write_reg(0x388, OPL3_REG_MULT_KSR_EGT_VIB_AM_12, 0x20);
    dosstuff_write_reg(0x388, OPL3_REG_MULT_KSR_EGT_VIB_AM_15, 0x01);
    dosstuff_write_reg(0x388, OPL3_REG_WS_12, 5);
    dosstuff_write_reg(0x388, OPL3_REG_WS_15, 0);
    
    /* SD */
    dosstuff_write_reg(0x388, OPL3_REG_DR_AR_16, 0xf7);
    dosstuff_write_reg(0x388, OPL3_REG_RR_SL_16, 0x56);
    dosstuff_write_reg(0x388, OPL3_REG_TL_KSL_16, 11);
    dosstuff_write_reg(0x388, OPL3_REG_MULT_KSR_EGT_VIB_AM_16, 1);
    dosstuff_write_reg(0x388, OPL3_REG_WS_16, 2);

    /* TOM */
    dosstuff_write_reg(0x388, OPL3_REG_DR_AR_14, 0xf5);
    dosstuff_write_reg(0x388, OPL3_REG_RR_SL_14, 0x85);
    dosstuff_write_reg(0x388, OPL3_REG_TL_KSL_14, 11);
    dosstuff_write_reg(0x388, OPL3_REG_MULT_KSR_EGT_VIB_AM_14, 1);
    dosstuff_write_reg(0x388, OPL3_REG_WS_14, 0);

    /* RIM */
    dosstuff_write_reg(0x388, OPL3_REG_DR_AR_17, 0xfa);
    dosstuff_write_reg(0x388, OPL3_REG_RR_SL_17, 0xaa);
    dosstuff_write_reg(0x388, OPL3_REG_TL_KSL_17, 11);
    dosstuff_write_reg(0x388, OPL3_REG_MULT_KSR_EGT_VIB_AM_17, 1);
    dosstuff_write_reg(0x388, OPL3_REG_WS_17, 0);

    /* HH */
    dosstuff_write_reg(0x388, OPL3_REG_DR_AR_13, 0xf8);
    dosstuff_write_reg(0x388, OPL3_REG_RR_SL_13, 0x08);
    dosstuff_write_reg(0x388, OPL3_REG_TL_KSL_13, 11);
    dosstuff_write_reg(0x388, OPL3_REG_MULT_KSR_EGT_VIB_AM_13, 1);
    dosstuff_write_reg(0x388, OPL3_REG_WS_13, 2);

    dosstuff_write_reg(0x388, OPL3_REG_F_NUMBER_L_6, 177);
    dosstuff_write_reg(0x388, OPL3_REG_FNUM_H_BLOCK_KON_6, 0x06);
    dosstuff_write_reg(0x388, OPL3_REG_F_NUMBER_L_7, 177);
    dosstuff_write_reg(0x388, OPL3_REG_FNUM_H_BLOCK_KON_7, 0x06);
    dosstuff_write_reg(0x388, OPL3_REG_F_NUMBER_L_8, 177);
    dosstuff_write_reg(0x388, OPL3_REG_FNUM_H_BLOCK_KON_8, 0x06);
}

u8_t ryt_state = 0;

void ryt_to_opl3(u8_t reg, u8_t data, u8_t port_idx) {
    if (reg == 0x10) {
        if (!(data >> 7)) {
            u8_t unset_ryt_state = ryt_state;
            if (data & 1) { /* BD */
                ryt_state |= 0x10;
                unset_ryt_state &= ~0x10;
            } else {
                ryt_state &= ~0x10;
            }
            if ((data >> 1) & 1) { /* SD */
                ryt_state |= 0x08;
                unset_ryt_state &= ~0x08;
            } else {
                ryt_state &= ~0x08;
            }
            if ((data >> 3) & 1) { /* HH */
                ryt_state |= 0x01;
                unset_ryt_state &= ~0x01;
            } else {
                ryt_state &= ~0x01;
            }
            if ((data >> 4) & 1) { /* TOM */
                ryt_state |= 0x04;
                unset_ryt_state &= ~0x04;
            } else {
                ryt_state &= ~0x04;
            }
            if ((data >> 5) & 1) { /* RIM */
                ryt_state |= 0x02;
                unset_ryt_state &= ~0x02;
            } else {
                ryt_state &= ~0x02;
            }

            dosstuff_write_reg(0x388, OPL3_REG_HH_TC_TOM_SD_BD_RYT_DVB_DAM, 0x20 | unset_ryt_state);
            dosstuff_write_reg(0x388, OPL3_REG_HH_TC_TOM_SD_BD_RYT_DVB_DAM, 0x20 | ryt_state);
        }
    }
}
