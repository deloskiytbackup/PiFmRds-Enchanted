/*
 * PiFmRds-ng - Next-Generation FM/RDS transmitter
 * Modernized for 2026 standards
 *
 * FM Multiplex (MPX) Stereo and RDS Baseband Generator
 */

#ifndef FM_MPX_H
#define FM_MPX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPX_SAMPLE_RATE 228000 /* 228 kHz sampling rate */

typedef enum {
    PRE_EMPHASIS_NONE = 0,
    PRE_EMPHASIS_50US = 50, /* Europe, Asia, Australia, South America */
    PRE_EMPHASIS_75US = 75  /* North America, Japan FM */
} mpx_pre_emphasis_t;

typedef struct {
    const char *audio_source;      /* Filename or "-" for stdin */
    bool is_stereo;                /* Enable 19 kHz pilot & 38 kHz L-R subcarrier */
    mpx_pre_emphasis_t pre_emph;  /* 50us or 75us */
    float audio_gain;              /* Volume scaling (default 1.0) */
    float pilot_level;             /* Nominal 0.09 (9% modulation) */
    float rds_level;               /* Nominal 0.05 (5% modulation) */
    bool rds_enabled;              /* Include 57 kHz RDS subcarrier */
    bool loop_audio;               /* Loop audio file when EOF reached */
} mpx_config_t;

/* Initialize MPX generator */
int fm_mpx_init(const mpx_config_t *config, size_t block_size);

/* Close and free MPX resources */
void fm_mpx_close(void);

/* Generates `count` samples of multiplexed baseband signal at 228 kHz.
   Returns number of samples written, or negative on EOF/error. */
int fm_mpx_get_samples(float *buffer, size_t count);

/* Check if audio stream has ended (for file playback) */
bool fm_mpx_is_eof(void);

#ifdef __cplusplus
}
#endif

#endif /* FM_MPX_H */
