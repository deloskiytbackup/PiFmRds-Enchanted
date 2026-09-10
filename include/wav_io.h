/*
 * PiFmRds-ng - Zero-dependency WAV file reader/writer
 * Modernized for 2026 standards
 * Handles 16-bit PCM, 24-bit PCM, 32-bit Float WAV files without external dependencies.
 */

#ifndef WAV_IO_H
#define WAV_IO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    FILE *fp;
    bool is_writing;
    bool is_stdin;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint16_t audio_format; /* 1 = PCM, 3 = IEEE Float */
    uint32_t data_bytes_written;
    long data_chunk_pos;
    bool eof;
} wav_file_t;

/* Open WAV for reading (or stdin if filename is "-" or "stdin") */
wav_file_t *wav_open_read(const char *filename);

/* Open WAV for writing */
wav_file_t *wav_open_write(const char *filename, uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample);

/* Read interleaved float samples in range [-1.0, 1.0]. Returns number of FRAMES read. */
size_t wav_read_float_frames(wav_file_t *wf, float *out_frames, size_t frame_count);

/* Write interleaved float samples. Returns number of FRAMES written. */
size_t wav_write_float_frames(wav_file_t *wf, const float *in_frames, size_t frame_count);

/* Rewind to beginning of audio data (for loop mode, files only) */
bool wav_rewind(wav_file_t *wf);

/* Close WAV file and update header sizes if writing */
void wav_close(wav_file_t *wf);

#ifdef __cplusplus
}
#endif

#endif /* WAV_IO_H */
