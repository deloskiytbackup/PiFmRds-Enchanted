/*
 * PiFmRds-Enchanted - Broadcast Audio DSP Engine (2026 Edition)
 *
 * Features:
 * - 15 kHz steep brickwall low-pass filter (prevents 19 kHz pilot intermodulation)
 * - Broadcast Automatic Gain Control (AGC) and soft peak limiter
 * - Pre-emphasis overshoot limiter
 */

#ifndef DSP_PROCESSOR_H
#define DSP_PROCESSOR_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool enable_15khz_filter;
    bool enable_agc;
    bool enable_limiter;
    float target_level;      /* Nominal output level (0.0 to 1.0, default 0.9) */
    float agc_attack;        /* Attack rate */
    float agc_release;       /* Release rate */
    float max_gain;          /* Maximum AGC boost in linear scale (e.g. 2.0 = +6dB) */
} dsp_config_t;

typedef struct {
    dsp_config_t config;
    double sample_rate;
    /* Biquad cascade states for Left and Right channels (15 kHz low-pass) */
    double bq_x1[2][4], bq_x2[2][4];
    double bq_y1[2][4], bq_y2[2][4];
    /* Biquad coefficients (2 stages of 2nd order = 4th order IIR) */
    double b0[4], b1[4], b2[4], a1[4], a2[4];
    int num_sections;
    /* AGC envelope follower state */
    float current_gain;
    float envelope;
} dsp_processor_t;

/* Initialize DSP processor */
void dsp_init(dsp_processor_t *dsp, double sample_rate, const dsp_config_t *config);

/* Process stereo frames in-place (L = buffer[2*i], R = buffer[2*i+1]) */
void dsp_process_stereo(dsp_processor_t *dsp, float *buffer, size_t frame_count);

/* Process mono samples in-place */
void dsp_process_mono(dsp_processor_t *dsp, float *buffer, size_t count);

/* Composite MPX peak limiter (prevents overdeviation beyond 75 kHz) */
float dsp_limit_mpx_sample(float sample);

#ifdef __cplusplus
}
#endif

#endif /* DSP_PROCESSOR_H */
