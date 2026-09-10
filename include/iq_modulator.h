/*
 * PiFmRds-Enchanted - SDR I/Q Modulator Engine (2026 Edition)
 * Converts MPX composite baseband into complex I/Q samples for HackRF, LimeSDR, FL2k, etc.
 */

#ifndef IQ_MODULATOR_H
#define IQ_MODULATOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IQ_FORMAT_S8 = 0,       /* 8-bit signed (HackRF default) */
    IQ_FORMAT_U8 = 1,       /* 8-bit unsigned (RTL-SDR / FL2k) */
    IQ_FORMAT_S16_LE = 2,   /* 16-bit signed LE (LimeSDR / BladeRF) */
    IQ_FORMAT_FLOAT32 = 3   /* 32-bit float (GNU Radio / SDR++) */
} iq_format_t;

typedef struct {
    uint32_t out_sample_rate; /* e.g. 2000000 (2.0 MSPS) */
    double deviation_hz;      /* Nominal 75000.0 Hz */
    iq_format_t format;
    FILE *out_fp;             /* Destination file or stdout */
    bool is_stdout;
} iq_config_t;

typedef struct {
    iq_config_t config;
    double current_phase;
    double phase_scale;
    double resample_ratio;
    double resample_phase;
    float last_mpx;
} iq_modulator_t;

/* Initialize I/Q modulator */
int iq_modulator_init(iq_modulator_t *mod, const iq_config_t *config);

/* Feed MPX samples (sampled at 228 kHz) and write modulated I/Q samples to output stream */
size_t iq_modulator_process(iq_modulator_t *mod, const float *mpx_samples, size_t mpx_count);

/* Close I/Q modulator */
void iq_modulator_close(iq_modulator_t *mod);

#ifdef __cplusplus
}
#endif

#endif /* IQ_MODULATOR_H */
