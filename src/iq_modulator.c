/*
 * PiFmRds-Enchanted - SDR I/Q Modulator Engine (2026 Edition)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "iq_modulator.h"
#include "fm_mpx.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int iq_modulator_init(iq_modulator_t *mod, const iq_config_t *config) {
    if (!mod || !config) return -1;
    memset(mod, 0, sizeof(iq_modulator_t));
    mod->config = *config;

    if (mod->config.out_sample_rate == 0) {
        mod->config.out_sample_rate = 2000000; /* Default 2.0 MSPS for HackRF */
    }
    if (mod->config.deviation_hz <= 0.0) {
        mod->config.deviation_hz = 75000.0; /* 75 kHz broadcast standard */
    }

    /* Ratio of input rate (228 kHz) to output rate */
    mod->resample_ratio = (double)MPX_SAMPLE_RATE / (double)mod->config.out_sample_rate;
    mod->phase_scale = (2.0 * M_PI * mod->config.deviation_hz) / (double)mod->config.out_sample_rate;
    mod->current_phase = 0.0;
    mod->resample_phase = 0.0;
    mod->last_mpx = 0.0f;

    return 0;
}

size_t iq_modulator_process(iq_modulator_t *mod, const float *mpx_samples, size_t mpx_count) {
    if (!mod || !mod->config.out_fp || !mpx_samples || mpx_count == 0) return 0;

    size_t mpx_idx = 0;
    size_t out_pairs = 0;

    /* Fast local buffers */
    uint8_t u8_buf[2048];
    int8_t s8_buf[2048];
    int16_t s16_buf[2048];
    float f32_buf[2048];
    size_t buf_idx = 0;

    while (mpx_idx < mpx_count) {
        /* Linearly interpolate baseband MPX sample at current output time step */
        float m0 = (mpx_idx > 0) ? mpx_samples[mpx_idx - 1] : mod->last_mpx;
        float m1 = mpx_samples[mpx_idx];
        float m = m0 + (float)mod->resample_phase * (m1 - m0);

        /* Advance FM phase accumulator: delta_phi = 2*pi * delta_f * m / Fs_out */
        mod->current_phase += mod->phase_scale * m;
        if (mod->current_phase >= 2.0 * M_PI) mod->current_phase -= 2.0 * M_PI;
        else if (mod->current_phase < 0.0) mod->current_phase += 2.0 * M_PI;

        double i_val = cos(mod->current_phase);
        double q_val = sin(mod->current_phase);

        switch (mod->config.format) {
            case IQ_FORMAT_S8:
                s8_buf[buf_idx++] = (int8_t)(i_val * 127.0);
                s8_buf[buf_idx++] = (int8_t)(q_val * 127.0);
                if (buf_idx >= 2048) {
                    fwrite(s8_buf, sizeof(int8_t), buf_idx, mod->config.out_fp);
                    buf_idx = 0;
                }
                break;
            case IQ_FORMAT_U8:
                u8_buf[buf_idx++] = (uint8_t)((i_val + 1.0) * 127.5);
                u8_buf[buf_idx++] = (uint8_t)((q_val + 1.0) * 127.5);
                if (buf_idx >= 2048) {
                    fwrite(u8_buf, sizeof(uint8_t), buf_idx, mod->config.out_fp);
                    buf_idx = 0;
                }
                break;
            case IQ_FORMAT_S16_LE:
                s16_buf[buf_idx++] = (int16_t)(i_val * 32767.0);
                s16_buf[buf_idx++] = (int16_t)(q_val * 32767.0);
                if (buf_idx >= 2048) {
                    fwrite(s16_buf, sizeof(int16_t), buf_idx, mod->config.out_fp);
                    buf_idx = 0;
                }
                break;
            case IQ_FORMAT_FLOAT32:
                f32_buf[buf_idx++] = (float)i_val;
                f32_buf[buf_idx++] = (float)q_val;
                if (buf_idx >= 2048) {
                    fwrite(f32_buf, sizeof(float), buf_idx, mod->config.out_fp);
                    buf_idx = 0;
                }
                break;
        }

        out_pairs++;
        mod->resample_phase += mod->resample_ratio;
        while (mod->resample_phase >= 1.0) {
            mod->last_mpx = mpx_samples[mpx_idx];
            mpx_idx++;
            mod->resample_phase -= 1.0;
            if (mpx_idx >= mpx_count) break;
        }
    }

    /* Flush remaining samples */
    if (buf_idx > 0) {
        switch (mod->config.format) {
            case IQ_FORMAT_S8:
                fwrite(s8_buf, sizeof(int8_t), buf_idx, mod->config.out_fp);
                break;
            case IQ_FORMAT_U8:
                fwrite(u8_buf, sizeof(uint8_t), buf_idx, mod->config.out_fp);
                break;
            case IQ_FORMAT_S16_LE:
                fwrite(s16_buf, sizeof(int16_t), buf_idx, mod->config.out_fp);
                break;
            case IQ_FORMAT_FLOAT32:
                fwrite(f32_buf, sizeof(float), buf_idx, mod->config.out_fp);
                break;
        }
    }

    return out_pairs;
}

void iq_modulator_close(iq_modulator_t *mod) {
    if (!mod) return;
    if (mod->config.out_fp && !mod->config.is_stdout) {
        fclose(mod->config.out_fp);
        mod->config.out_fp = NULL;
    }
}
