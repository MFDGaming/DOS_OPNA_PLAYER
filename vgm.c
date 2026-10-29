#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ntypes.h"
#include "player.h"
#include "vgm.h"
#include "opna.h"

static vgm_hdr_t hdr;
static FILE *in_fp;
static u32_t vgm_data_off;
static u32_t vgm_gd3_off;
static u32_t vgm_loop_off;
static u32_t vgm_extra_hdr_off;

u8_t psg_freq_lo[3] = {0, 0, 0};
u8_t psg_freq_hi[3] = {0, 0, 0};
u8_t psg_idx = 0;
u8_t psg_iont = 0x3F;
u8_t psg_llwt = 0;

u8_t psg_to_ssg_att[16] =  {15, 14, 14, 13, 12, 12, 11, 10, 10, 9, 8, 8, 7, 6, 6, 0};

static void end_playback() {
    player_write_register(0, 0x28, 0);
    player_write_register(0, 0x28, 1);
    player_write_register(0, 0x28, 2);
    player_write_register(0, 0x28, 4);
    player_write_register(0, 0x28, 5);
    player_write_register(0, 0x28, 6);
    player_write_register(0, 0x07, 0x3f);
}

#define PSG_ATT_BIT 0x10
#define PSG_REG_MASK 0x70

void update_psg_freq() {
    u8_t idx = psg_idx << 1;
    u16_t n = ((psg_freq_hi[psg_idx]) << 4) | psg_freq_lo[psg_idx];
    u32_t clk = hdr.sn76489_clock & ~0x80000000;
    double f;
    u16_t tp;
    if (!n) {
        n = 1024;
    }
    if (!clk) {
        clk = 3579545;
    }
    f = clk / (n * 32.0);
    tp = (u16_t)(hdr.ym2612_clock / (64.0 * f));
    player_write_register(0, 0x00 + idx, tp & 0xff);
    player_write_register(0, 0x01 + idx, (tp >> 8) & 0x0f);
}

int parse_cmds() {
    u8_t done_loop = 0;

    player_setup();
    
    fseek(in_fp, vgm_data_off, SEEK_SET);
    for (; ftell(in_fp) < hdr.eof_offset;) {
        u8_t cmd, reg_id, reg_data;
        u16_t ws;
        u32_t data_size;
        u8_t data_type, data_pad;
        if (kbhit()) {
            end_playback();
            break;
        }

        if (fread(&cmd, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
            fprintf(stderr, "Unexpected EOF while reading the vgm command\n");
            return -1;
        }
        if (cmd == 0x50 || cmd == 0x4f) {
            if (fread(&reg_data, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the SN76489 register data\n");
                return -1;
            }
            if (reg_data & 0x80) {
                u8_t psg_reg_addr = reg_data & PSG_REG_MASK;
                u8_t psg_data = reg_data & 0x0f;
                psg_llwt = 0;
                if (psg_reg_addr <= 0x50) {
                    u8_t idx = (psg_reg_addr >> 5) & 0x03;
                    if (psg_reg_addr & PSG_ATT_BIT) {
                        if (psg_data == 0x0f) {
                            psg_iont |= 1 << idx;
                            player_write_register(0, 0x07, psg_iont);
                        } else {
                            psg_iont &= ~(1 << idx);
                            player_write_register(0, 0x07, psg_iont);
                            player_write_register(0, 0x08 + idx, psg_to_ssg_att[psg_data]);
                        }
                    } else {
                        psg_freq_lo[idx] = psg_data;
                        psg_idx = idx;
                        psg_llwt = 1;
                        update_psg_freq();
                    }
                }
            } else {
                if (psg_llwt) {
                    psg_freq_hi[psg_idx] = reg_data & 0x3f;
                    update_psg_freq();
                }
            }
        } else if (cmd == 0x56 || cmd == 0x57) {
            if (fread(&reg_id, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the YM2608 register id\n");
                return -1;
            }
            if (fread(&reg_data, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the YM2608 register data\n");
                return -1;
            }
            player_write_register(cmd & 1, reg_id, reg_data);
        } else if (cmd == 0x52 || cmd == 0x53) {
            if (fread(&reg_id, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the YM2612 register id\n");
                return -1;
            }
            if (fread(&reg_data, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the YM2612 register data\n");
                return -1;
            }
            player_write_register(cmd & 1, reg_id, reg_data);
        } else if (cmd == 0xe0) {
            u32_t seek_off;
            if (fread(&seek_off, 1, sizeof(u32_t), in_fp) != sizeof(u32_t)) {
                fprintf(stderr, "Unexpected EOF while reading the YM2612 seek offset\n");
                return -1;
            }
        } else if ((cmd >> 4) == 0x08) {
            player_wait(cmd & 0x0f);
        } else if (cmd == 0x55) {
            if (fread(&reg_id, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the YM2203 register id\n");
                return -1;
            }
            if (fread(&reg_data, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the YM2203 register data\n");
                return -1;
            }
            player_write_register(0, reg_id, reg_data);
        } else if (cmd == 0x62 || cmd == 0x63) {
            player_wait(735 + ((cmd & 1) * 147));
        } else if ((cmd >> 4) == 0x07) {
            player_wait(1 + (cmd & 0x0f));
        } else if (cmd == 0x61) {
            if (fread(&ws, 1, sizeof(u16_t), in_fp) != sizeof(u16_t)) {
                fprintf(stderr, "Unexpected EOF while reading the wait sample count\n");
                return -1;
            }
            player_wait(ws);
        } else if (cmd == 0x66) {
            puts("End reached");
            if (!done_loop && vgm_loop_off >= vgm_data_off && vgm_loop_off < hdr.eof_offset) {
                fseek(in_fp, vgm_loop_off, SEEK_SET);
                //done_loop = 1;
            } else {
                break;
            }
        } else if (cmd == 0x90) {
            u8_t stream_id, chip_type, port, cmd_reg;
            if (fread(&stream_id, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the stream id\n");
                return -1;
            }
            if (fread(&chip_type, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the chip type\n");
                return -1;
            }
            if (fread(&port, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the port\n");
                return -1;
            }
            if (fread(&cmd_reg, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the cmd/reg\n");
                return -1;
            }
        } else if (cmd == 0x91) {
            u8_t stream_id, data_bank_id, step_size, step_base;
            if (fread(&stream_id, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the stream id\n");
                return -1;
            }
            if (fread(&data_bank_id, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the data bank id\n");
                return -1;
            }
            if (fread(&step_size, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the step size\n");
                return -1;
            }
            if (fread(&step_base, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the step base\n");
                return -1;
            }
        } else if (cmd == 0x92) {
            u8_t stream_id;
            u32_t freq;
            if (fread(&stream_id, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the stream id\n");
                return -1;
            }
            if (fread(&freq, 1, sizeof(u32_t), in_fp) != sizeof(u32_t)) {
                fprintf(stderr, "Unexpected EOF while reading the frequency\n");
                return -1;
            }
        } else if (cmd == 0x93) {
            u8_t stream_id, length_mode;
            u32_t data_offset, data_length;

            if (fread(&stream_id, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the stream id\n");
                return -1;
            }
            if (fread(&data_offset, 1, sizeof(u32_t), in_fp) != sizeof(u32_t)) {
                fprintf(stderr, "Unexpected EOF while reading the data offset\n");
                return -1;
            }
            if (fread(&length_mode, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the length mode\n");
                return -1;
            }
            if (fread(&data_length, 1, sizeof(u32_t), in_fp) != sizeof(u32_t)) {
                fprintf(stderr, "Unexpected EOF while reading the data length\n");
                return -1;
            }
        } else if (cmd == 0x94) {
            u8_t stream_id;
            if (fread(&stream_id, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the stream id\n");
                return -1;
            }
        } else if (cmd == 0x95) {
            u8_t stream_id, flags;
            u16_t block_id;
            if (fread(&stream_id, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the stream id\n");
                return -1;
            }
            if (fread(&block_id, 1, sizeof(u16_t), in_fp) != sizeof(u16_t)) {
                fprintf(stderr, "Unexpected EOF while reading the block id\n");
                return -1;
            }
            if (fread(&flags, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                fprintf(stderr, "Unexpected EOF while reading the flags\n");
                return -1;
            }
        } else if (cmd == 0x67) {
            u32_t rom_size, start_addr, j, i, end_addr;
            u8_t rom_byte;
            long tmp_pos;
            if (fread(&data_pad, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                end_playback();
                fprintf(stderr, "Unexpected EOF while reading the data pad\n");
                return -1;
            }

            if (fread(&data_type, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                end_playback();
                fprintf(stderr, "Unexpected EOF while reading the data block type\n");
                return -1;
            }
            if (fread(&data_size, 1, sizeof(u32_t), in_fp) != sizeof(u32_t)) {
                end_playback();
                fprintf(stderr, "Unexpected EOF while reading the data block size\n");
                return -1;
            }

            tmp_pos = ftell(in_fp);

#ifdef PC98
            if (data_type == 0x81) {
                if (fread(&rom_size, 1, sizeof(u32_t), in_fp) != sizeof(u32_t)) {
                    fprintf(stderr, "Unexpected EOF while reading the rom size\n");
                    return -1;
                }
                if (fread(&start_addr, 1, sizeof(u32_t), in_fp) != sizeof(u32_t)) {
                    fprintf(stderr, "Unexpected EOF while reading the start address\n");
                    return -1;
                }

                printf("ds:%lu dt:%u\n", data_size, data_type & 0xff);
                printf("sa:%lu rs:%lu\n", start_addr, rom_size);

                end_addr = (start_addr + (data_size - 8)) - 1;

                outp(0x18c, 0x10); outp(0x18e, 0x13);
                outp(0x18c, 0x10); outp(0x18e, 0x80);
                outp(0x18c, 0x00); outp(0x18e, 0x60);
                outp(0x18c, 0x01); outp(0x18e, 0x02);

                outp(0x18c, 0x02); outp(0x18e, (start_addr >> 5) & 0xff);
                outp(0x18c, 0x03); outp(0x18e, (start_addr >> 13) & 0xff);

                outp(0x18c, 0x04); outp(0x18e, (end_addr >> 5) & 0xff);
                outp(0x18c, 0x05); outp(0x18e, (end_addr >> 13) & 0xff);
                
                outp(0x18c, 0x0c); outp(0x18e, (end_addr >> 5) & 0xff);
                outp(0x18c, 0x0d); outp(0x18e, (end_addr >> 13) & 0xff);

                for (j = 0; j < (data_size - 8); ++j) {
                    if (fread(&rom_byte, 1, sizeof(u8_t), in_fp) != sizeof(u8_t)) {
                        fprintf(stderr, "Unexpected EOF while reading rom byte\n");
                        return -1;
                    }
                    outp(0x18c, 0x08); outp(0x18e, rom_byte);
                    outp(0x18c, 0x10); outp(0x18e, 0x1b);
                    outp(0x18c, 0x10); outp(0x18e, 0x13);
                    for (;;) {
                        u8_t status = 1; //inp(0x18c);
                        if ((status >> 2) & 1) {
                            goto upload_done;
                        }
                        if ((status >> 3) & 1) {
                            break;
                        }
                    }
                }
                upload_done:
                outp(0x18c, 0x00); outp(0x18e, 0x00);
                outp(0x18c, 0x10); outp(0x18e, 0x80);
            }
#endif
            fseek(in_fp, tmp_pos + data_size, SEEK_SET);
        } else {
            end_playback();
            fprintf(stderr, "Unknown vgm command 0x%.2X\n", cmd);
            return -1;
        }
    }
    end_playback();
    return 0;
}

void print_info() {
    printf("VGM version %X.", hdr.version >> 8);
    printf("%.2X\n", hdr.version & 0xff);
    printf("VGM data offset %lu\n", vgm_data_off);
    printf("VGM gd3 offset %lu\n", vgm_gd3_off);
    printf("VGM EOF offset %lu\n", hdr.eof_offset);
    printf("VGM extra header offset %lu\n", vgm_extra_hdr_off);
    printf("VGM loop offset %lu\n", vgm_loop_off);
    printf("VGM loop samples %lu\n", hdr.loop_samples);
    printf("VGM total samples %lu\n", hdr.total_samples);
    if (hdr.ym2608_clock && hdr.ym2608_clock <= 8600000) {
        printf("YM2608 clock %lu\n", hdr.ym2608_clock);
    }
    if (hdr.ym2203_clock && hdr.ym2203_clock <= 8600000) {
        printf("YM2203 clock %lu\n", hdr.ym2203_clock);
    }
    if (hdr.ym2612_clock && hdr.ym2612_clock <= 8600000) {
        printf("YM2612 clock %lu\n", hdr.ym2612_clock);
    }
}

int read_vgm(char *path) {
    for (;;) {
        in_fp = fopen(path, "rb");
        if (!in_fp) {
            fprintf(stderr, "File not found\n");
            return -1;
        }
        if (fread(&hdr, 1, sizeof(vgm_hdr_t), in_fp) != sizeof(vgm_hdr_t)) {
            fprintf(stderr, "Unexpected EOF while reading the vgm header\n");
            fclose(in_fp);
            return -1;
        }

        if (hdr.ident[0] == '\x1f' && hdr.ident[1] == '\x8b') {
            char *text = malloc(strlen(path) + 29);
            fclose(in_fp);
            if (text) {
                sprintf(text, "gzip\\gzip.exe -dc %s > tmp.vgm", path);
                printf("%s\n", text);
                system(text);
                free(text);
                path = "tmp.vgm";
                continue;
            } else {
                fprintf(stderr, "Failed to allocate memory\n");
                fclose(in_fp);
                return -1;
            }
        }
        break;
    }
    if (hdr.ident[0] != '\x56' || hdr.ident[1] != '\x67' || hdr.ident[2] != '\x6D' || hdr.ident[3] != '\x20') {
        fprintf(stderr, "Invalid VGM header\n");
        fclose(in_fp);
        return -1;
    }
    if (hdr.version < 0x150) {
        vgm_data_off = 0x40;
    } else {
        vgm_data_off = 0x34 + hdr.vgm_data_offset;
    }
    vgm_gd3_off = 0x14 + hdr.gd3_offset;
    vgm_loop_off = 0x1c + hdr.loop_offset;
    vgm_extra_hdr_off = 0xbc + hdr.extra_hdr_ofs;
    print_info();
    return parse_cmds();
}
