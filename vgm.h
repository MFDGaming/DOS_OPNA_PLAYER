#ifndef VGMCONV_VGM_H
#define VGMCONV_VGM_H

#include "ntypes.h"

typedef struct {
    char ident[4];
    u32_t eof_offset;
    u32_t version;
    u32_t sn76489_clock;
    u32_t ym2413_clock;
    u32_t gd3_offset;
    u32_t total_samples;
    u32_t loop_offset;
    u32_t loop_samples;
    u32_t rate;
    u32_t snfb_snw_sf;
    u32_t ym2612_clock;
    u32_t ym2151_clock;
    u32_t vgm_data_offset;
    u32_t segapcm_clock;
    u32_t spcm_interface;
    u32_t rf5c68_clock;
    u32_t ym2203_clock;
    u32_t ym2608_clock;
    u32_t ym2610_clock;
    u32_t ym3812_clock;
    u32_t ym3526_clock;
    u32_t y8950_clock;
    u32_t ymf262_clock;
    u32_t ymf278b_clock;
    u32_t ymf271_clock;
    u32_t ymz280b_clock;
    u32_t rf5c164_clock;
    u32_t pwm_clock;
    u32_t ay8910_clock;
    u32_t ayt_ayflags;
    u32_t vm_lb_lm;
    u32_t gbdmg_clock;
    u32_t nesapu_clock;
    u32_t multipcm_clock;
    u32_t upd7759_clock;
    u32_t msm6258_clock;
    u32_t of_kf_cf;
    u32_t msm6295_clock; 
    u32_t k051649_clock;
    u32_t k064539_clock;
    u32_t huc6280_clock;
    u32_t c140_clock;
    u32_t k053260_clock;
    u32_t pokey_clock;
    u32_t qsound_clock;
    u32_t scsp_clock;
    u32_t extra_hdr_ofs;
    u32_t wonderswan_clock;
    u32_t vsu_clock;
    u32_t saa1099_clock;
    u32_t es5503_clock;
    u32_t es5506_clock;
    u32_t eschns_cd;
    u32_t x1010_clock;
    u32_t c342_clock;
    u32_t ga20_clock;
    u32_t mickey_clock;
    u32_t reserved[6];
} vgm_hdr_t;

int read_vgm(char *path);

#endif
