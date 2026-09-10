/*
 * PiFmRds-Enchanted - Broadcast Audio DSP Engine (2026 Edition)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dsp_processor.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void calc_biquad_lpf(double fs, double fc, double q, double *b0, double *b1, double *b2, double *a1, double *a2) {
    if (fc >= fs * 0.48) fc = fs * 0.45;
    double w0 = 2.0 * M_PI * fc / fs;
    double alpha = sin(w0) / (2.0 * q);
    double cos_w0 = cos(w0);

    double a0 = 1.0 + alpha;
    *b0 = ((1.0 - cos_w0) / 2.0) / a0;
    *b1 = (1.0 - cos_w0) / a0;
    *b2 = ((1.0 - cos_w0) / 2.0) / a0;
    *a1 = (-2.0 * cos_w0) / a0;
    *a2 = (1.0 - alpha) / a0;
}

void dsp_init(dsp_processor_t *dsp, double sample_rate, const dsp_config_t *config) {
    if (!dsp) return;
    memset(dsp, 0, sizeof(dsp_processor_t));
    dsp->sample_rate = sample_rate;

    if (config) {
        dsp->config = *config;
    } else {
        dsp->config.enable_15khz_filter = true;
        dsp->config.enable_agc = true;
        dsp->config.enable_limiter = true;
        dsp->config.target_level = 0.92f;
        dsp->config.agc_attack = 0.005f;
        dsp->config.agc_release = 0.0002f;
        dsp->config.max_gain = 2.0f;
    }

    dsp->current_gain = 1.0f;
    dsp->envelope = 0.0f;

    /* Design 4th-order Butterworth 15 kHz Low-Pass Filter (two cascaded 2nd-order stages) */
    dsp->num_sections = 2;
    double q1 = 0.54119610; /* Butterworth 4th-order Q values */
    double q2 = 1.30656296;
    calc_biquad_lpf(sample_rate, 15000.0, q1, &dsp->b0[0], &dsp->b1[0], &dsp->b2[0], &dsp->a1[0], &dsp->a2[0]);
    calc_biquad_lpf(sample_rate, 15000.0, q2, &dsp->b0[1], &dsp->b1[1], &dsp->b2[1], &dsp->a1[1], &dsp->a2[1]);
}

static inline double process_biquad(dsp_processor_t *dsp, int ch, int sec, double in) {
    double out = dsp->b0[sec] * in + dsp->b1[sec] * dsp->bq_x1[ch][sec] + dsp->b2[sec] * dsp->bq_x2[ch][sec]
                 - dsp->a1[sec] * dsp->bq_y1[ch][sec] - dsp->a2[sec] * dsp->bq_y2[ch][sec];

    dsp->bq_x2[ch][sec] = dsp->bq_x1[ch][sec];
    dsp->bq_x1[ch][sec] = in;
    dsp->bq_y2[ch][sec] = dsp->bq_y1[ch][sec];
    dsp->bq_y1[ch][sec] = out;
    return out;
}

void dsp_process_stereo(dsp_processor_t *dsp, float *buffer, size_t frame_count) {
    if (!dsp || !buffer || frame_count == 0) return;

    for (size_t i = 0; i < frame_count; i++) {
        double left = buffer[2 * i];
        double right = buffer[2 * i + 1];

        /* 15 kHz Brickwall Low-Pass Filtering */
        if (dsp->config.enable_15khz_filter) {
            for (int s = 0; s < dsp->num_sections; s++) {
                left = process_biquad(dsp, 0, s, left);
                right = process_biquad(dsp, 1, s, right);
            }
        }

        /* Peak envelope follower for AGC */
        float peak = (float)fmax(fabs(left), fabs(right));
        if (peak > dsp->envelope) {
            dsp->envelope += dsp->config.agc_attack * (peak - dsp->envelope);
        } else {
            dsp->envelope += dsp->config.agc_release * (peak - dsp->envelope);
        }

        /* Calculate AGC gain */
        if (dsp->config.enable_agc) {
            float desired_gain = 1.0f;
            if (dsp->envelope > 0.001f) {
                desired_gain = dsp->config.target_level / dsp->envelope;
                if (desired_gain > dsp->config.max_gain) desired_gain = dsp->config.max_gain;
                if (desired_gain < 0.2f) desired_gain = 0.2f;
            }
            dsp->current_gain += 0.001f * (desired_gain - dsp->current_gain);
            left *= dsp->current_gain;
            right *= dsp->current_gain;
        }

        /* Fast soft limiter */
        if (dsp->config.enable_limiter) {
            if (left > 1.0) left = 1.0 + 0.1 * tanh((left - 1.0) / 0.1);
            else if (left < -1.0) left = -1.0 + 0.1 * tanh((left + 1.0) / 0.1);

            if (right > 1.0) right = 1.0 + 0.1 * tanh((right - 1.0) / 0.1);
            else if (right < -1.0) right = -1.0 + 0.1 * tanh((right + 1.0) / 0.1);
        }

        buffer[2 * i] = (float)left;
        buffer[2 * i + 1] = (float)right;
    }
}

void dsp_process_mono(dsp_processor_t *dsp, float *buffer, size_t count) {
    if (!dsp || !buffer || count == 0) return;

    for (size_t i = 0; i < count; i++) {
        double sample = buffer[i];

        if (dsp->config.enable_15khz_filter) {
            for (int s = 0; s < dsp->num_sections; s++) {
                sample = process_biquad(dsp, 0, s, sample);
            }
        }

        float peak = (float)fabs(sample);
        if (peak > dsp->envelope) {
            dsp->envelope += dsp->config.agc_attack * (peak - dsp->envelope);
        } else {
            dsp->envelope += dsp->config.agc_release * (peak - dsp->envelope);
        }

        if (dsp->config.enable_agc) {
            float desired_gain = 1.0f;
            if (dsp->envelope > 0.001f) {
                desired_gain = dsp->config.target_level / dsp->envelope;
                if (desired_gain > dsp->config.max_gain) desired_gain = dsp->config.max_gain;
                if (desired_gain < 0.2f) desired_gain = 0.2f;
            }
            dsp->current_gain += 0.001f * (desired_gain - dsp->current_gain);
            sample *= dsp->current_gain;
        }

        if (dsp->config.enable_limiter) {
            if (sample > 1.0) sample = 1.0 + 0.1 * tanh((sample - 1.0) / 0.1);
            else if (sample < -1.0) sample = -1.0 + 0.1 * tanh((sample + 1.0) / 0.1);
        }

        buffer[i] = (float)sample;
    }
}

float dsp_limit_mpx_sample(float sample) {
    /* Hard clipping threshold beyond which transmitter overdeviates past 75 kHz */
    if (sample > 1.0f) {
        return 1.0f + 0.05f * (float)tanh((sample - 1.0f) / 0.05f);
    } else if (sample < -1.0f) {
        return -1.0f + 0.05f * (float)tanh((sample + 1.0f) / 0.05f);
    }
    return sample;
}
