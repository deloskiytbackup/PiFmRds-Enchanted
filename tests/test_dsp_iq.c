#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "dsp_processor.h"
#include "iq_modulator.h"

int main(void) {
    printf("[TEST] Running test_dsp_iq...\n");

    /* Test 1: DSP Processor 15 kHz low-pass attenuation test */
    dsp_processor_t dsp;
    dsp_config_t conf;
    conf.enable_15khz_filter = true;
    conf.enable_agc = false; /* Disable AGC to test pure filter frequency response */
    conf.enable_limiter = false;
    dsp_init(&dsp, 44100.0, &conf);

    /* Generate 1 kHz tone (in-band) and 19 kHz tone (near stereo pilot) */
    float buf_1k[200];
    float buf_19k[200];
    for (int i = 0; i < 100; i++) {
        float s1 = sinf(2.0f * 3.14159f * 1000.0f * i / 44100.0f);
        float s19 = sinf(2.0f * 3.14159f * 19000.0f * i / 44100.0f);
        buf_1k[2 * i] = s1;
        buf_1k[2 * i + 1] = s1;
        buf_19k[2 * i] = s19;
        buf_19k[2 * i + 1] = s19;
    }

    dsp_process_stereo(&dsp, buf_1k, 100);
    dsp_process_stereo(&dsp, buf_19k, 100);

    /* Compute RMS of filtered signals */
    float rms_1k = 0.0f, rms_19k = 0.0f;
    for (int i = 50; i < 100; i++) { /* steady state */
        rms_1k += buf_1k[2 * i] * buf_1k[2 * i];
        rms_19k += buf_19k[2 * i] * buf_19k[2 * i];
    }
    rms_1k = sqrtf(rms_1k / 50.0f);
    rms_19k = sqrtf(rms_19k / 50.0f);

    printf("  1 kHz tone RMS after 15kHz filter:  %.4f (Expected ~0.707)\n", rms_1k);
    printf("  19 kHz tone RMS after 15kHz filter: %.4f (Expected heavily attenuated <0.15)\n", rms_19k);
    assert(rms_1k > 0.6f);
    assert(rms_19k < 0.2f);

    /* Test 2: MPX Limiter */
    float limited = dsp_limit_mpx_sample(2.5f);
    printf("  MPX soft limiter on 2.5 input: %.4f (Expected <= 1.05)\n", limited);
    assert(limited <= 1.05f);

    /* Test 3: SDR I/Q Modulator */
    iq_modulator_t mod;
    iq_config_t iq_conf;
    iq_conf.out_sample_rate = 2000000;
    iq_conf.deviation_hz = 75000.0;
    iq_conf.format = IQ_FORMAT_FLOAT32;
    iq_conf.is_stdout = false;

    FILE *tmp_iq = tmpfile();
    iq_conf.out_fp = tmp_iq;

    assert(iq_modulator_init(&mod, &iq_conf) == 0);

    float mpx_test[228];
    for (int i = 0; i < 228; i++) {
        mpx_test[i] = 0.5f * sinf(2.0f * 3.14159f * 1000.0f * i / 228000.0f);
    }

    size_t out_pairs = iq_modulator_process(&mod, mpx_test, 228);
    printf("  Modulated 228 MPX samples into %zu SDR I/Q sample pairs.\n", out_pairs);
    assert(out_pairs > 1000);

    /* Check I/Q values in file */
    fseek(tmp_iq, 0, SEEK_SET);
    float iq_data[64];
    size_t n = fread(iq_data, sizeof(float), 64, tmp_iq);
    assert(n == 64);
    for (size_t i = 0; i < 32; i++) {
        float i_val = iq_data[2 * i];
        float q_val = iq_data[2 * i + 1];
        float mag = sqrtf(i_val * i_val + q_val * q_val);
        assert(fabsf(mag - 1.0f) < 0.05f); /* FM constant envelope constraint: |I^2+Q^2| == 1 */
    }

    fclose(tmp_iq);
    printf("[TEST] test_dsp_iq PASSED!\n");
    return 0;
}
