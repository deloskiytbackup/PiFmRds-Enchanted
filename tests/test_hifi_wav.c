#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include "wav_io.h"

/* Helper to write a synthetic 24-bit 96 kHz WAV file */
static void create_synthetic_24bit_wav(const char *filename) {
    FILE *fp = fopen(filename, "wb");
    assert(fp != NULL);

    uint32_t sample_rate = 96000;
    uint16_t channels = 2;
    uint16_t bits = 24;
    uint32_t num_frames = 1000;
    uint32_t data_bytes = num_frames * channels * 3;

    /* Write RIFF header */
    fwrite("RIFF", 1, 4, fp);
    uint32_t riff_size = 36 + data_bytes;
    fwrite(&riff_size, 4, 1, fp);
    fwrite("WAVEfmt ", 1, 8, fp);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, fp);

    uint16_t audio_fmt = 1; /* PCM */
    fwrite(&audio_fmt, 2, 1, fp);
    fwrite(&channels, 2, 1, fp);
    fwrite(&sample_rate, 4, 1, fp);
    uint32_t byte_rate = sample_rate * channels * 3;
    fwrite(&byte_rate, 4, 1, fp);
    uint16_t block_align = channels * 3;
    fwrite(&block_align, 2, 1, fp);
    fwrite(&bits, 2, 1, fp);

    /* Write data chunk */
    fwrite("data", 1, 4, fp);
    fwrite(&data_bytes, 4, 1, fp);

    /* Write 1000 stereo frames of 24-bit audio (sine wave) */
    for (uint32_t i = 0; i < num_frames; i++) {
        float f_left = sinf(2.0f * 3.14159265f * 1000.0f * (float)i / (float)sample_rate);
        float f_right = cosf(2.0f * 3.14159265f * 1000.0f * (float)i / (float)sample_rate);

        int32_t i_left = (int32_t)(f_left * 8388607.0f);
        int32_t i_right = (int32_t)(f_right * 8388607.0f);

        uint8_t l_bytes[3] = {
            (uint8_t)(i_left & 0xFF),
            (uint8_t)((i_left >> 8) & 0xFF),
            (uint8_t)((i_left >> 16) & 0xFF)
        };
        uint8_t r_bytes[3] = {
            (uint8_t)(i_right & 0xFF),
            (uint8_t)((i_right >> 8) & 0xFF),
            (uint8_t)((i_right >> 16) & 0xFF)
        };

        fwrite(l_bytes, 1, 3, fp);
        fwrite(r_bytes, 1, 3, fp);
    }

    fclose(fp);
}

int main(void) {
    printf("[TEST] Running test_hifi_wav...\n");

    const char *test_path = "/tmp/test_hifi_96k_24b.wav";
    create_synthetic_24bit_wav(test_path);

    /* Open the synthetic 24-bit 96 kHz file */
    wav_file_t *wf = wav_open_read(test_path);
    assert(wf != NULL);
    printf("  Opened: %u Hz, %u channels, %u-bit PCM\n", wf->sample_rate, wf->channels, wf->bits_per_sample);
    assert(wf->sample_rate == 96000);
    assert(wf->channels == 2);
    assert(wf->bits_per_sample == 24);
    assert(wf->audio_format == 1);

    /* Read frames */
    float buffer[100 * 2];
    size_t read_frames = wav_read_float_frames(wf, buffer, 100);
    assert(read_frames == 100);

    /* Verify sine sample range */
    for (size_t i = 0; i < read_frames * 2; i++) {
        assert(buffer[i] >= -1.05f && buffer[i] <= 1.05f);
    }
    printf("  Verified first 100 frames correctly scaled to float [-1.0, 1.0]. Sample[0]=%f, Sample[1]=%f\n",
           buffer[0], buffer[1]);

    wav_close(wf);
    remove(test_path);

    printf("[TEST] test_hifi_wav PASSED!\n");
    return 0;
}
