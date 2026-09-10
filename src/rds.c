/*
 * PiFmRds-ng - Next-Generation FM/RDS transmitter
 * Modernized for 2026 standards
 *
 * RDS Engine Implementation (IEC 62106 / RBDS)
 * Supports Groups 0A (PS/AF), 2A (RT), 3A (ODA/RT+), 11A (RT+ data), 4A (Clock-Time)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "rds.h"
#include "rds_strings.h"
#include "waveforms.h"

#define POLY 0x1B9
#define POLY_DEG 10
#define MSB_BIT 0x8000
#define BLOCK_SIZE 16

#define SAMPLES_PER_BIT 192  /* 228000 Hz / 1187.5 bps = 192 samples per bit */
#define FILTER_SIZE 384     /* Length of waveform_biphase */
#define SAMPLE_BUFFER_SIZE (SAMPLES_PER_BIT + FILTER_SIZE)

static const uint16_t offset_words[4] = {0x0FC, 0x198, 0x168, 0x1B4};

static rds_config_t g_cfg;
static rds_ps_paginator_t g_ps_paginator;

/* Internal bitstream buffer */
static uint8_t g_group_bits[104];
static size_t g_bit_index = 104;

/* Differential biphase state */
static int g_last_diff_bit = 0;
static float g_sample_buffer[SAMPLE_BUFFER_SIZE] = {0};
static size_t g_sample_buf_idx = 0;
static int g_subcarrier_phase = 0; /* 0, 1, 2, 3 for 57 kHz @ 228 kHz sample rate */

/* Group scheduler state */
static uint32_t g_group_counter = 0;
static uint8_t g_ps_segment = 0;
static uint8_t g_rt_segment = 0;
static int g_last_ct_minute = -1;

/* RT+ Tagging state */
static uint8_t g_rt_plus_toggle = 0;

uint16_t rds_calc_crc(uint16_t block) {
    uint16_t crc = 0;
    for (int j = 0; j < BLOCK_SIZE; j++) {
        int bit = (block & MSB_BIT) != 0;
        block <<= 1;
        int msb = (crc >> (POLY_DEG - 1)) & 1;
        crc <<= 1;
        if ((msb ^ bit) != 0) {
            crc ^= POLY;
        }
    }
    return crc & 0x3FF;
}

void rds_init(void) {
    memset(&g_cfg, 0, sizeof(rds_config_t));
    g_cfg.pi = 0x1234;
    snprintf(g_cfg.ps, sizeof(g_cfg.ps), "PIFMRDS ");
    snprintf(g_cfg.rt, sizeof(g_cfg.rt), "PiFmRds-ng 2026: Live FM-RDS transmission");
    g_cfg.music = true;
    g_cfg.enable_ct = true;
    g_cfg.pty = 10; /* Pop Music */
    g_cfg.pty_std = RDS_PTY_STANDARD_RDS;
    
    rds_ps_paginator_init(&g_ps_paginator);
    rds_ps_paginator_set_text(&g_ps_paginator, g_cfg.ps, RDS_PS_MODE_STATIC, 2000);

    g_sample_buf_idx = SAMPLES_PER_BIT;
    g_bit_index = 104;
    g_group_counter = 0;
    g_ps_segment = 0;
    g_rt_segment = 0;
    g_last_ct_minute = -1;
}

void rds_cleanup(void) {
    /* Nothing to dynamically free */
}

void set_rds_pi(uint16_t pi) {
    g_cfg.pi = pi;
}

void set_rds_ps(const char *ps) {
    if (!ps) return;
    rds_sanitize_string(ps, g_cfg.ps, sizeof(g_cfg.ps));
    while (strlen(g_cfg.ps) < 8) {
        strcat(g_cfg.ps, " ");
    }
    rds_ps_paginator_set_text(&g_ps_paginator, g_cfg.ps, RDS_PS_MODE_STATIC, 2000);
}

void set_rds_dynamic_ps(const char *text, rds_ps_mode_t mode, uint32_t interval_ms) {
    if (!text) return;
    rds_sanitize_string(text, g_cfg.dynamic_ps_text, sizeof(g_cfg.dynamic_ps_text));
    g_cfg.ps_mode = mode;
    g_cfg.ps_interval_ms = interval_ms;
    rds_ps_paginator_set_text(&g_ps_paginator, g_cfg.dynamic_ps_text, (int)mode, interval_ms);
}

void set_rds_rt(const char *rt) {
    if (!rt) return;
    char clean_rt[RDS_RT_LEN + 1];
    rds_sanitize_string(rt, clean_rt, sizeof(clean_rt));
    if (strncmp(clean_rt, g_cfg.rt, RDS_RT_LEN) != 0) {
        strncpy(g_cfg.rt, clean_rt, RDS_RT_LEN);
        g_cfg.rt[RDS_RT_LEN] = '\0';
        g_cfg.rt_ab_flag = !g_cfg.rt_ab_flag; /* Toggle A/B on new text */
        g_rt_segment = 0;
    }
}

void set_rds_rt_plus(const char *title, const char *artist) {
    if (!title && !artist) {
        g_cfg.rt_plus_enabled = false;
        return;
    }

    g_cfg.rt_plus_enabled = true;
    if (title) {
        rds_sanitize_string(title, g_cfg.rt_plus_title, sizeof(g_cfg.rt_plus_title));
    }
    if (artist) {
        rds_sanitize_string(artist, g_cfg.rt_plus_artist, sizeof(g_cfg.rt_plus_artist));
    }

    /* Build combined RadioText: "Artist - Title" */
    char combined_rt[RDS_RT_LEN + 1];
    if (strlen(g_cfg.rt_plus_artist) > 0 && strlen(g_cfg.rt_plus_title) > 0) {
        snprintf(combined_rt, sizeof(combined_rt), "%s - %s", g_cfg.rt_plus_artist, g_cfg.rt_plus_title);
    } else if (strlen(g_cfg.rt_plus_title) > 0) {
        snprintf(combined_rt, sizeof(combined_rt), "%s", g_cfg.rt_plus_title);
    } else {
        snprintf(combined_rt, sizeof(combined_rt), "%s", g_cfg.rt_plus_artist);
    }

    set_rds_rt(combined_rt);
    g_rt_plus_toggle = !g_rt_plus_toggle;
}

void set_rds_ta(bool ta) { g_cfg.ta = ta; }
void set_rds_tp(bool tp) { g_cfg.tp = tp; }
void set_rds_pty(uint8_t pty, rds_pty_standard_t standard) {
    g_cfg.pty = pty & 0x1F;
    g_cfg.pty_std = standard;
}
void set_rds_music(bool music) { g_cfg.music = music; }
void set_rds_ct(bool enable) { g_cfg.enable_ct = enable; }

void set_rds_af(const uint32_t *af_khz, size_t count) {
    g_cfg.af_count = 0;
    if (!af_khz) return;
    for (size_t i = 0; i < count && i < RDS_MAX_AF; i++) {
        if (af_khz[i] >= 87500 && af_khz[i] <= 108000) {
            g_cfg.af_list[g_cfg.af_count++] = af_khz[i];
        }
    }
}

const rds_config_t *rds_get_config(void) {
    return &g_cfg;
}

/* Helper to convert kHz frequency to RDS AF code (87.5 MHz = 1, 108.0 MHz = 204) */
static uint8_t freq_to_af_code(uint32_t khz) {
    if (khz < 87500 || khz > 108000) return 0; /* No AF */
    return (uint8_t)((khz - 87500) / 100 + 1);
}

/* Build Group 0A (Basic Tuning and Switching: PS, AF, TA, TP, PTY) */
static void build_group_0a(uint16_t *blocks) {
    blocks[0] = g_cfg.pi;
    
    uint16_t b = (0 << 12) | (0 << 11); /* Group 0A */
    b |= (g_cfg.tp ? 1 : 0) << 10;
    b |= (g_cfg.pty & 0x1F) << 5;
    b |= (g_cfg.ta ? 1 : 0) << 4;
    b |= (g_cfg.music ? 1 : 0) << 3;
    b |= 1 << 2; /* DI stereo */
    b |= (g_ps_segment & 0x03);
    blocks[1] = b;

    /* Block 3: AF (Alternative Frequencies) */
    if (g_cfg.af_count > 0) {
        size_t af_pair_idx = (g_ps_segment % ((g_cfg.af_count + 1) / 2)) * 2;
        uint8_t af1 = (af_pair_idx == 0) ? (224 + (uint8_t)g_cfg.af_count) : freq_to_af_code(g_cfg.af_list[af_pair_idx - 1]);
        uint8_t af2 = (af_pair_idx < g_cfg.af_count) ? freq_to_af_code(g_cfg.af_list[af_pair_idx]) : 205; /* 205 = filler */
        blocks[2] = ((uint16_t)af1 << 8) | af2;
    } else {
        blocks[2] = 0xE000; /* No AF carrier */
    }

    /* Block 4: 2 characters of PS (guaranteed non-null space-padded) */
    size_t ps_char_idx = g_ps_segment * 2;
    size_t ps_len = strlen(g_cfg.ps);
    uint8_t c1 = (ps_char_idx < ps_len && g_cfg.ps[ps_char_idx] != '\0') ? (uint8_t)g_cfg.ps[ps_char_idx] : 0x20;
    uint8_t c2 = (ps_char_idx + 1 < ps_len && g_cfg.ps[ps_char_idx + 1] != '\0') ? (uint8_t)g_cfg.ps[ps_char_idx + 1] : 0x20;
    blocks[3] = ((uint16_t)c1 << 8) | c2;

    g_ps_segment = (g_ps_segment + 1) & 0x03;
}

/* Build Group 2A (RadioText: 64 characters across 16 segments) */
static void build_group_2a(uint16_t *blocks) {
    blocks[0] = g_cfg.pi;

    uint16_t b = (2 << 12) | (0 << 11); /* Group 2A */
    b |= (g_cfg.tp ? 1 : 0) << 10;
    b |= (g_cfg.pty & 0x1F) << 5;
    b |= (g_cfg.rt_ab_flag ? 1 : 0) << 4;
    b |= (g_rt_segment & 0x0F);
    blocks[1] = b;

    size_t idx = g_rt_segment * 4;
    size_t rt_len = strlen(g_cfg.rt);

    char c1 = (idx < rt_len) ? g_cfg.rt[idx] : ' ';
    char c2 = (idx + 1 < rt_len) ? g_cfg.rt[idx + 1] : ' ';
    char c3 = (idx + 2 < rt_len) ? g_cfg.rt[idx + 2] : ' ';
    char c4 = (idx + 3 < rt_len) ? g_cfg.rt[idx + 3] : ' ';

    blocks[2] = ((uint16_t)(uint8_t)c1 << 8) | (uint8_t)c2;
    blocks[3] = ((uint16_t)(uint8_t)c3 << 8) | (uint8_t)c4;

    g_rt_segment = (g_rt_segment + 1) & 0x0F;
}

/* Build Group 3A (Open Data Application registration for RT+, AID = 0x4BD7) */
static void build_group_3a_rt_plus(uint16_t *blocks) {
    blocks[0] = g_cfg.pi;
    uint16_t b = (3 << 12) | (0 << 11); /* Group 3A */
    b |= (g_cfg.tp ? 1 : 0) << 10;
    b |= (g_cfg.pty & 0x1F) << 5;
    b |= 0x0B; /* Application Group Type: 11A */
    blocks[1] = b;

    blocks[2] = 0x0000; /* Server-specific message bits */
    blocks[3] = 0x4BD7; /* RT+ Registered Application ID */
}

/* Build Group 11A (RT+ Data Group with Title and Artist markers) */
static void build_group_11a_rt_plus(uint16_t *blocks) {
    blocks[0] = g_cfg.pi;
    uint16_t b = (11 << 12) | (0 << 11); /* Group 11A */
    b |= (g_cfg.tp ? 1 : 0) << 10;
    b |= (g_cfg.pty & 0x1F) << 5;
    b |= (g_rt_plus_toggle ? 1 : 0) << 4; /* Item toggle */
    b |= 1 << 3;                          /* Item running */
    blocks[1] = b;

    /* Locate artist and title positions in g_cfg.rt */
    size_t artist_len = strlen(g_cfg.rt_plus_artist);
    size_t title_len = strlen(g_cfg.rt_plus_title);
    uint8_t artist_start = 0;
    uint8_t title_start = 0;

    const char *p_artist = strstr(g_cfg.rt, g_cfg.rt_plus_artist);
    if (p_artist) artist_start = (uint8_t)(p_artist - g_cfg.rt);

    const char *p_title = strstr(g_cfg.rt, g_cfg.rt_plus_title);
    if (p_title) title_start = (uint8_t)(p_title - g_cfg.rt);

    /* Block 3: First tag (e.g. Title, type 1) */
    uint8_t tag1_type = RT_PLUS_ITEM_TITLE;
    uint8_t tag1_start = title_start & 0x3F;
    uint8_t tag1_len = (title_len > 0) ? (uint8_t)(title_len - 1) & 0x3F : 0;

    /* Block 4: Second tag (e.g. Artist, type 4) */
    uint8_t tag2_type = RT_PLUS_ITEM_ARTIST;
    uint8_t tag2_start = artist_start & 0x3F;
    uint8_t tag2_len = (artist_len > 0) ? (uint8_t)(artist_len - 1) & 0x3F : 0;

    blocks[2] = ((uint16_t)tag1_type << 10) | ((uint16_t)tag1_start << 4) | (tag1_len >> 2);
    blocks[3] = ((uint16_t)(tag1_len & 0x03) << 14) | ((uint16_t)tag2_type << 8) | ((uint16_t)tag2_start << 2) | (tag2_len >> 4);
}

/* Build Group 4A (Clock-Time and Date) */
static bool build_group_4a_ct(uint16_t *blocks) {
    time_t now = time(NULL);
    struct tm utc_tm;
    gmtime_r(&now, &utc_tm);

    if (utc_tm.tm_min == g_last_ct_minute) {
        return false;
    }
    g_last_ct_minute = utc_tm.tm_min;

    blocks[0] = g_cfg.pi;

    /* Modified Julian Day calculation according to RDS standard Annex G */
    int y = utc_tm.tm_year + 1900;
    int m = utc_tm.tm_mon + 1;
    int d = utc_tm.tm_mday;
    int l = (m <= 2) ? 1 : 0;
    int mjd = 14956 + d + (int)((y - 1900 - l) * 365.25) + (int)((m + 1 + l * 12) * 30.6001);

    uint16_t b = (4 << 12) | (0 << 11); /* Group 4A */
    b |= (g_cfg.tp ? 1 : 0) << 10;
    b |= (g_cfg.pty & 0x1F) << 5;
    b |= (mjd >> 15) & 0x03;
    blocks[1] = b;

    blocks[2] = ((mjd & 0x7FFF) << 1) | (utc_tm.tm_hour >> 4);

    struct tm loc_tm;
    localtime_r(&now, &loc_tm);
    int offset_half_hours = (int)(loc_tm.tm_gmtoff / 1800);
    uint8_t sign = (offset_half_hours < 0) ? 0x20 : 0;
    int abs_h = abs(offset_half_hours);
    if (abs_h > 31) abs_h = 31;
    uint8_t abs_offset = (uint8_t)abs_h;

    blocks[3] = ((utc_tm.tm_hour & 0x0F) << 12) | (utc_tm.tm_min << 6) | sign | abs_offset;
    return true;
}

void rds_build_group(uint16_t *blocks) {
    /* Update dynamic PS if needed */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t now_ms = (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    char new_ps[9];
    if (rds_ps_paginator_tick(&g_ps_paginator, now_ms, new_ps)) {
        memcpy(g_cfg.ps, new_ps, 8);
    }

    /* Check for minute change for CT */
    if (g_cfg.enable_ct && build_group_4a_ct(blocks)) {
        return;
    }

    /* RDS Sequence: 0A, 0A, 2A, 0A, (RT+ or 0A) */
    uint32_t step = g_group_counter % 8;
    g_group_counter++;

    if (g_cfg.rt_plus_enabled && step == 3) {
        build_group_3a_rt_plus(blocks);
    } else if (g_cfg.rt_plus_enabled && step == 7) {
        build_group_11a_rt_plus(blocks);
    } else if (step == 2 || step == 6) {
        build_group_2a(blocks);
    } else {
        build_group_0a(blocks);
    }
}

/* Assemble a full 104-bit RDS group with checkwords and offset words */
static void rds_generate_next_group_bits(void) {
    uint16_t blocks[4];
    rds_build_group(blocks);

    size_t bit_pos = 0;
    for (int b = 0; b < 4; b++) {
        uint16_t info = blocks[b];
        uint16_t check = rds_calc_crc(info) ^ offset_words[b];

        for (int i = 15; i >= 0; i--) {
            g_group_bits[bit_pos++] = (info >> i) & 1;
        }
        for (int i = 9; i >= 0; i--) {
            g_group_bits[bit_pos++] = (check >> i) & 1;
        }
    }
    g_bit_index = 0;
}

void get_rds_samples(float *buffer, size_t count) {
    static const float subcarrier_lut[4] = {0.0f, 1.0f, 0.0f, -1.0f};

    for (size_t i = 0; i < count; i++) {
        /* Check if we need to load a new bit into the filter buffer */
        if (g_sample_buf_idx >= SAMPLES_PER_BIT) {
            if (g_bit_index >= 104) {
                rds_generate_next_group_bits();
            }

            int data_bit = g_group_bits[g_bit_index++];
            /* Differential encoding */
            g_last_diff_bit ^= data_bit;
            float symbol = g_last_diff_bit ? 1.0f : -1.0f;

            /* Shift the filter buffer and add the new shaped pulse */
            memmove(g_sample_buffer, g_sample_buffer + SAMPLES_PER_BIT, 
                    (SAMPLE_BUFFER_SIZE - SAMPLES_PER_BIT) * sizeof(float));
            memset(g_sample_buffer + (SAMPLE_BUFFER_SIZE - SAMPLES_PER_BIT), 0, 
                   SAMPLES_PER_BIT * sizeof(float));

            for (size_t k = 0; k < waveform_biphase_size && k < SAMPLE_BUFFER_SIZE; k++) {
                g_sample_buffer[k] += symbol * waveform_biphase[k];
            }

            g_sample_buf_idx = 0;
        }

        /* Baseband biphase shaped pulse */
        float biphase_sample = g_sample_buffer[g_sample_buf_idx++];

        /* Modulate with 57 kHz subcarrier (samples at 228 kHz: 57k/228k = 1/4 rate) */
        buffer[i] = biphase_sample * subcarrier_lut[g_subcarrier_phase];
        g_subcarrier_phase = (g_subcarrier_phase + 1) & 0x03;
    }
}
