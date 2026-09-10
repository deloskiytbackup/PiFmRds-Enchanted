/*
 * PiFmRds-Enchanted - FM/RDS Transmitter (2026 Edition)
 *
 * FM Multiplex (MPX) Stereo and RDS Composite Baseband Generator
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fm_mpx.h"
#include "wav_io.h"
#include "rds.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define AUDIO_IN_CHUNK 4096

static mpx_config_t g_cfg;
static wav_file_t *g_wav_in = NULL;
static bool g_eof_reached = false;

/* Audio input buffer */
static float *g_in_audio = NULL;
static size_t g_in_samples_loaded = 0;
static size_t g_in_read_pos = 0;
static double g_resample_phase = 0.0;
static double g_resample_ratio = 1.0;

/* Subcarrier oscillators */
static double g_pilot_phase = 0.0;
static double g_pilot_inc = 0.0;
static double g_sub38_phase = 0.0;
static double g_sub38_inc = 0.0;

/* Pre-emphasis filter state */
static float g_pre_emph_alpha = 0.0f;
static float g_prev_left = 0.0f;
static float g_prev_right = 0.0f;

/* Temporary RDS buffer */
static float *g_rds_buf = NULL;
static size_t g_block_size = 0;

static void setup_pre_emphasis(mpx_pre_emphasis_t type, double in_rate) {
    if (type == PRE_EMPHASIS_NONE || in_rate <= 0.0) {
        g_pre_emph_alpha = 0.0f;
        return;
    }
    double tau = (type == PRE_EMPHASIS_75US) ? 75e-6 : 50e-6;
    /* High-shelf pre-emphasis: y[n] = x[n] - alpha * x[n-1] */
    g_pre_emph_alpha = (float)exp(-1.0 / (in_rate * tau));
}

int fm_mpx_init(const mpx_config_t *config, size_t block_size) {
    if (!config) return -1;
    fm_mpx_close();

    g_cfg = *config;
    g_block_size = block_size;
    g_eof_reached = false;

    if (g_cfg.audio_source != NULL) {
        g_wav_in = wav_open_read(g_cfg.audio_source);
        if (!g_wav_in) {
            fprintf(stderr, "[MPX] Warning: Could not open audio source '%s'. Transmitting carrier & RDS only.\n",
                    g_cfg.audio_source);
        } else {
            printf("[MPX] Audio opened: %u Hz, %u channels, %u-bit\n",
                   g_wav_in->sample_rate, g_wav_in->channels, g_wav_in->bits_per_sample);
            g_resample_ratio = (double)g_wav_in->sample_rate / (double)MPX_SAMPLE_RATE;
            setup_pre_emphasis(g_cfg.pre_emph, g_wav_in->sample_rate);
        }
    }

    g_in_audio = (float *)malloc(AUDIO_IN_CHUNK * 2 * sizeof(float));
    g_rds_buf = (float *)malloc(block_size * sizeof(float));
    if (!g_in_audio || !g_rds_buf) {
        fm_mpx_close();
        return -1;
    }

    g_pilot_inc = (2.0 * M_PI * 19000.0) / MPX_SAMPLE_RATE;
    g_sub38_inc = (2.0 * M_PI * 38000.0) / MPX_SAMPLE_RATE;
    g_pilot_phase = 0.0;
    g_sub38_phase = 0.0;

    return 0;
}

void fm_mpx_close(void) {
    if (g_wav_in) {
        wav_close(g_wav_in);
        g_wav_in = NULL;
    }
    if (g_in_audio) {
        free(g_in_audio);
        g_in_audio = NULL;
    }
    if (g_rds_buf) {
        free(g_rds_buf);
        g_rds_buf = NULL;
    }
    g_in_samples_loaded = 0;
    g_in_read_pos = 0;
    g_resample_phase = 0.0;
    g_prev_left = 0.0f;
    g_prev_right = 0.0f;
}

bool fm_mpx_is_eof(void) {
    return g_eof_reached;
}

static bool load_more_audio(void) {
    if (!g_wav_in) return false;

    size_t read_frames = wav_read_float_frames(g_wav_in, g_in_audio, AUDIO_IN_CHUNK);
    if (read_frames == 0) {
        if (g_cfg.loop_audio && !g_wav_in->is_stdin) {
            wav_rewind(g_wav_in);
            read_frames = wav_read_float_frames(g_wav_in, g_in_audio, AUDIO_IN_CHUNK);
        }
    }

    if (read_frames == 0) {
        g_eof_reached = true;
        g_in_samples_loaded = 0;
        return false;
    }

    g_in_samples_loaded = read_frames;
    g_in_read_pos = 0;
    return true;
}

int fm_mpx_get_samples(float *buffer, size_t count) {
    if (!buffer || count == 0) return 0;

    if (g_cfg.rds_enabled) {
        get_rds_samples(g_rds_buf, count);
    } else {
        memset(g_rds_buf, 0, count * sizeof(float));
    }

    for (size_t i = 0; i < count; i++) {
        float left = 0.0f;
        float right = 0.0f;

        if (g_wav_in && !g_eof_reached) {
            while (g_resample_phase >= 1.0) {
                g_in_read_pos++;
                g_resample_phase -= 1.0;
                if (g_in_read_pos >= g_in_samples_loaded) {
                    if (!load_more_audio()) {
                        break;
                    }
                }
            }

            if (!g_eof_reached && g_in_samples_loaded > 0) {
                size_t p0 = g_in_read_pos;
                size_t p1 = (p0 + 1 < g_in_samples_loaded) ? p0 + 1 : p0;
                float frac = (float)g_resample_phase;

                if (g_wav_in->channels >= 2) {
                    float l0 = g_in_audio[p0 * 2];
                    float r0 = g_in_audio[p0 * 2 + 1];
                    float l1 = g_in_audio[p1 * 2];
                    float r1 = g_in_audio[p1 * 2 + 1];

                    left = l0 + frac * (l1 - l0);
                    right = r0 + frac * (r1 - r0);
                } else {
                    float m0 = g_in_audio[p0];
                    float m1 = g_in_audio[p1];
                    left = right = m0 + frac * (m1 - m0);
                }

                if (g_pre_emph_alpha > 0.0f) {
                    float cur_l = left;
                    float cur_r = right;
                    left = left - g_pre_emph_alpha * g_prev_left;
                    right = right - g_pre_emph_alpha * g_prev_right;
                    g_prev_left = cur_l;
                    g_prev_right = cur_r;
                }

                left *= g_cfg.audio_gain;
                right *= g_cfg.audio_gain;
                g_resample_phase += g_resample_ratio;
            }
        }

        /* Mono sum M = (L + R) / 2 */
        float mono = 0.5f * (left + right);
        float mpx = mono;

        /* Stereo pilot tone (19 kHz) and difference (38 kHz) */
        if (g_cfg.is_stereo) {
            float pilot = (float)(sin(g_pilot_phase) * (g_cfg.pilot_level > 0 ? g_cfg.pilot_level : 0.09));
            float diff = 0.5f * (left - right);
            float sub38 = (float)(diff * sin(g_sub38_phase));

            mpx += pilot + sub38;

            g_pilot_phase += g_pilot_inc;
            if (g_pilot_phase >= 2.0 * M_PI) g_pilot_phase -= 2.0 * M_PI;

            g_sub38_phase += g_sub38_inc;
            if (g_sub38_phase >= 2.0 * M_PI) g_sub38_phase -= 2.0 * M_PI;
        }

        /* Add 57 kHz RDS subcarrier */
        if (g_cfg.rds_enabled) {
            float rds_amp = (g_cfg.rds_level > 0.0f) ? g_cfg.rds_level : 0.05f;
            mpx += g_rds_buf[i] * rds_amp;
        }

        if (mpx > 1.2f) mpx = 1.2f;
        else if (mpx < -1.2f) mpx = -1.2f;

        buffer[i] = mpx;
    }

    return (int)count;
}
