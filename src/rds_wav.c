/*
 * PiFmRds-Enchanted - FM/RDS Transmitter (2026 Edition)
 *
 * Standalone MPX / RDS WAV Baseband Generator (Runs universally on PC/Pi/Mac/SDR)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rds.h"
#include "fm_mpx.h"
#include "wav_io.h"

#define CHUNK_SIZE 11400 /* 50 ms at 228 kHz */

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("PiFmRds-Enchanted MPX WAV Generator (2026 Edition)\n");
        printf("Syntax: %s <in_audio.wav|NONE> <out_mpx.wav> <station_text> [seconds]\n", argv[0]);
        printf("Example:\n  %s sound.wav out_mpx.wav \"ENCHANTED\" 10\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *in_file = (strcmp(argv[1], "NONE") == 0) ? NULL : argv[1];
    const char *out_file = argv[2];
    const char *text = argv[3];
    double duration_sec = (argc >= 5) ? atof(argv[4]) : 10.0;
    if (duration_sec <= 0.0) duration_sec = 10.0;

    rds_init();
    set_rds_pi(0x1234);
    set_rds_ps(text);
    set_rds_rt(text);

    mpx_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.audio_source = in_file;
    cfg.is_stereo = true;
    cfg.pre_emph = PRE_EMPHASIS_50US;
    cfg.audio_gain = 1.0f;
    cfg.pilot_level = 0.09f;
    cfg.rds_level = 0.05f;
    cfg.rds_enabled = true;
    cfg.loop_audio = true;

    if (fm_mpx_init(&cfg, CHUNK_SIZE) != 0) {
        fprintf(stderr, "Error: Could not initialize MPX generator.\n");
        return EXIT_FAILURE;
    }

    wav_file_t *outf = wav_open_write(out_file, MPX_SAMPLE_RATE, 1, 16);
    if (!outf) {
        fprintf(stderr, "Error: Could not open output file %s.\n", out_file);
        fm_mpx_close();
        return EXIT_FAILURE;
    }

    size_t total_samples = (size_t)(duration_sec * MPX_SAMPLE_RATE);
    size_t generated = 0;
    float mpx_buffer[CHUNK_SIZE];

    printf("Generating %g seconds of 228 kHz MPX signal into '%s'...\n", duration_sec, out_file);

    while (generated < total_samples) {
        size_t to_gen = CHUNK_SIZE;
        if (generated + to_gen > total_samples) {
            to_gen = total_samples - generated;
        }

        int n = fm_mpx_get_samples(mpx_buffer, to_gen);
        if (n <= 0) break;

        /* Scale sample values to safe modulation range */
        for (int i = 0; i < n; i++) {
            mpx_buffer[i] *= 0.7f;
        }

        size_t written = wav_write_float_frames(outf, mpx_buffer, n);
        if (written == 0) {
            fprintf(stderr, "Error writing samples to %s.\n", out_file);
            break;
        }

        generated += n;
    }

    wav_close(outf);
    fm_mpx_close();
    rds_cleanup();

    printf("Successfully generated %zu samples to '%s'.\n", generated, out_file);
    return EXIT_SUCCESS;
}
