/*
 * PiFmRds-Enchanted - FM/RDS Transmitter (2026 Edition)
 *
 * FM Multiplex (MPX) Stereo, Broadcast Audio DSP, and RDS Composite Baseband Generator
 * Highly optimized with table-driven phase-locked subcarrier oscillators
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fm_mpx.h"
#include "wav_io.h"
#include "rds.h"
#include "dsp_processor.h"

#define AUDIO_IN_CHUNK 4096

/* Phase-locked subcarrier tables at 228 kHz sampling rate */
static const float g_pilot_lut[12] = {
    0.0f, 0.5f, 0.8660254038f, 1.0f, 0.8660254038f, 0.5f,
    0.0f, -0.5f, -0.8660254038f, -1.0f, -0.8660254038f, -0.5f
};

static const float g_sub38_lut[6] = {
    0.0f, 0.8660254038f, 0.8660254038f, 0.0f, -0.8660254038f, -0.8660254038f
};

static mpx_config_t g_cfg;
static wav_file_t *g_wav_in = NULL;
static bool g_eof_reached = false;
static dsp_processor_t g_dsp;

/* Audio input buffer */
static float *g_in_audio = NULL;
static size_t g_in_samples_loaded = 0;
static size_t g_in_read_pos = 0;
static double g_resample_phase = 0.0;
static double g_resample_ratio = 1.0;

/* Subcarrier oscillator phase counters */
static unsigned int g_pilot_idx = 0;
static unsigned int g_sub38_idx = 0;

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

    double in_rate = 44100.0;
    if (g_cfg.audio_source != NULL) {
        g_wav_in = wav_open_read(g_cfg.audio_source);
        if (!g_wav_in) {
            fprintf(stderr, "[MPX] Warning: Could not open audio source '%s'. Transmitting carrier & RDS only.\n",
                    g_cfg.audio_source);
        } else {
            printf("[MPX] Audio opened: %u Hz, %u channels, %u-bit\n",
                   g_wav_in->sample_rate, g_wav_in->channels, g_wav_in->bits_per_sample);
            in_rate = (double)g_wav_in->sample_rate;
            g_resample_ratio = in_rate / (double)MPX_SAMPLE_RATE;
            setup_pre_emphasis(g_cfg.pre_emph, in_rate);
        }
    }

    /* Initialize broadcast DSP processor (15 kHz brickwall LPF + AGC limiter) */
    dsp_config_t dsp_conf;
    dsp_conf.enable_15khz_filter = true;
    dsp_conf.enable_agc = true;
    dsp_conf.enable_limiter = true;
    dsp_conf.target_level = 0.90f;
    dsp_conf.agc_attack = 0.005f;
    dsp_conf.agc_release = 0.0002f;
    dsp_conf.max_gain = 2.0f;
    dsp_init(&g_dsp, in_rate, &dsp_conf);

    g_in_audio = (float *)malloc(AUDIO_IN_CHUNK * 2 * sizeof(float));
    g_rds_buf = (float *)malloc(block_size * sizeof(float));
    if (!g_in_audio || !g_rds_buf) {
        fm_mpx_close();
        return -1;
    }

    g_pilot_idx = 0;
    g_sub38_idx = 0;

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

    /* Process audio through Broadcast DSP: 15 kHz brickwall LPF + AGC */
    if (g_wav_in->channels >= 2) {
        dsp_process_stereo(&g_dsp, g_in_audio, read_frames);
    } else {
        dsp_process_mono(&g_dsp, g_in_audio, read_frames);
    }

    g_in_samples_loaded = read_frames;
    g_in_read_pos = 0;
    return true;
}

/* Audiophile 4-point Catmull-Rom cubic Hermite spline interpolation */
static inline float catmull_rom(float ym1, float y0, float y1, float y2, float t) {
    float a = -0.5f * ym1 + 1.5f * y0 - 1.5f * y1 + 0.5f * y2;
    float b = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    float c = -0.5f * ym1 + 0.5f * y1;
    float d = y0;
    return ((a * t + b) * t + c) * t + d;
}

int fm_mpx_get_samples(float *buffer, size_t count) {
    if (!buffer || count == 0) return 0;

    if (g_cfg.rds_enabled) {
        get_rds_samples(g_rds_buf, count);
    } else {
        memset(g_rds_buf, 0, count * sizeof(float));
    }

    float pilot_gain = (g_cfg.pilot_level > 0.0f) ? g_cfg.pilot_level : 0.09f;
    float rds_amp = (g_cfg.rds_level > 0.0f) ? g_cfg.rds_level : 0.05f;

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
                size_t pm1 = (p0 > 0) ? p0 - 1 : p0;
                size_t p1 = (p0 + 1 < g_in_samples_loaded) ? p0 + 1 : p0;
                size_t p2 = (p0 + 2 < g_in_samples_loaded) ? p0 + 2 : p1;
                float frac = (float)g_resample_phase;

                if (g_wav_in->channels >= 2) {
                    left = catmull_rom(g_in_audio[pm1 * 2],     g_in_audio[p0 * 2],     g_in_audio[p1 * 2],     g_in_audio[p2 * 2],     frac);
                    right = catmull_rom(g_in_audio[pm1 * 2 + 1], g_in_audio[p0 * 2 + 1], g_in_audio[p1 * 2 + 1], g_in_audio[p2 * 2 + 1], frac);
                } else {
                    left = right = catmull_rom(g_in_audio[pm1], g_in_audio[p0], g_in_audio[p1], g_in_audio[p2], frac);
                }

                /* Pre-emphasis filter */
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

        /* Stereo pilot tone (19 kHz) and difference (38 kHz) via ultra-fast tables */
        if (g_cfg.is_stereo) {
            float pilot = g_pilot_lut[g_pilot_idx] * pilot_gain;
            float diff = 0.5f * (left - right);
            float sub38 = diff * g_sub38_lut[g_sub38_idx];

            mpx += pilot + sub38;

            g_pilot_idx = (g_pilot_idx + 1) % 12;
            g_sub38_idx = (g_sub38_idx + 1) % 6;
        }

        /* Add 57 kHz RDS subcarrier */
        if (g_cfg.rds_enabled) {
            mpx += g_rds_buf[i] * rds_amp;
        }

        /* Broadcast soft peak limiter (guarantees safe 75 kHz deviation compliance) */
        buffer[i] = dsp_limit_mpx_sample(mpx);
    }

    return (int)count;
}
