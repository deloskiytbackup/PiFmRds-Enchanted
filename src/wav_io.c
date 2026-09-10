/*
 * PiFmRds-ng - Zero-dependency WAV file reader/writer
 * Modernized for 2026 standards
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wav_io.h"

#pragma pack(push, 1)
typedef struct {
    char riff_id[4];        /* "RIFF" */
    uint32_t riff_size;     /* Overall file size - 8 */
    char wave_id[4];        /* "WAVE" */
    char fmt_id[4];         /* "fmt " */
    uint32_t fmt_size;      /* 16 for PCM */
    uint16_t audio_format;  /* 1 for PCM, 3 for Float */
    uint16_t num_channels;  /* 1 = Mono, 2 = Stereo */
    uint32_t sample_rate;   /* e.g. 228000 */
    uint32_t byte_rate;     /* sample_rate * num_channels * (bits / 8) */
    uint16_t block_align;   /* num_channels * (bits / 8) */
    uint16_t bits_per_samp; /* 16 */
    char data_id[4];        /* "data" */
    uint32_t data_size;     /* Size of data payload in bytes */
} wav_header_t;
#pragma pack(pop)

wav_file_t *wav_open_read(const char *filename) {
    if (!filename) return NULL;

    wav_file_t *wf = (wav_file_t *)calloc(1, sizeof(wav_file_t));
    if (!wf) return NULL;

    if (strcmp(filename, "-") == 0 || strcmp(filename, "stdin") == 0) {
        wf->fp = stdin;
        wf->is_stdin = true;
    } else {
        wf->fp = fopen(filename, "rb");
    }

    if (!wf->fp) {
        free(wf);
        return NULL;
    }

    /* Try to read RIFF header */
    wav_header_t header;
    if (fread(&header, sizeof(wav_header_t), 1, wf->fp) == 1 &&
        memcmp(header.riff_id, "RIFF", 4) == 0 &&
        memcmp(header.wave_id, "WAVE", 4) == 0) {
        wf->sample_rate = header.sample_rate;
        wf->channels = header.num_channels;
        wf->bits_per_sample = header.bits_per_samp;
        wf->audio_format = header.audio_format;
        wf->data_chunk_pos = ftell(wf->fp);
    } else {
        /* Fallback for raw PCM or non-standard header */
        if (!wf->is_stdin) {
            fseek(wf->fp, 0, SEEK_SET);
        }
        wf->sample_rate = 44100;
        wf->channels = 2;
        wf->bits_per_sample = 16;
        wf->audio_format = 1;
        wf->data_chunk_pos = 0;
    }

    return wf;
}

wav_file_t *wav_open_write(const char *filename, uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample) {
    if (!filename) return NULL;

    wav_file_t *wf = (wav_file_t *)calloc(1, sizeof(wav_file_t));
    if (!wf) return NULL;

    wf->fp = fopen(filename, "wb");
    if (!wf->fp) {
        free(wf);
        return NULL;
    }

    wf->is_writing = true;
    wf->sample_rate = sample_rate;
    wf->channels = channels;
    wf->bits_per_sample = bits_per_sample;
    wf->audio_format = 1; /* PCM */

    /* Write preliminary header placeholder */
    wav_header_t header;
    memcpy(header.riff_id, "RIFF", 4);
    header.riff_size = 36; /* Will update in wav_close */
    memcpy(header.wave_id, "WAVE", 4);
    memcpy(header.fmt_id, "fmt ", 4);
    header.fmt_size = 16;
    header.audio_format = 1;
    header.num_channels = channels;
    header.sample_rate = sample_rate;
    header.byte_rate = sample_rate * channels * (bits_per_sample / 8);
    header.block_align = channels * (bits_per_sample / 8);
    header.bits_per_samp = bits_per_sample;
    memcpy(header.data_id, "data", 4);
    header.data_size = 0;

    fwrite(&header, sizeof(wav_header_t), 1, wf->fp);
    wf->data_chunk_pos = sizeof(wav_header_t);
    return wf;
}

size_t wav_read_float_frames(wav_file_t *wf, float *out_frames, size_t frame_count) {
    if (!wf || !wf->fp || !out_frames || wf->eof) return 0;

    size_t total_samples = frame_count * wf->channels;
    size_t samples_read = 0;

    if (wf->bits_per_sample == 16) {
        int16_t raw[1024];
        while (samples_read < total_samples) {
            size_t to_read = total_samples - samples_read;
            if (to_read > 1024) to_read = 1024;
            size_t n = fread(raw, sizeof(int16_t), to_read, wf->fp);
            if (n == 0) {
                wf->eof = true;
                break;
            }
            for (size_t i = 0; i < n; i++) {
                out_frames[samples_read + i] = (float)raw[i] / 32768.0f;
            }
            samples_read += n;
        }
    } else {
        /* Default 16-bit fallback */
        wf->eof = true;
    }

    return samples_read / wf->channels;
}

size_t wav_write_float_frames(wav_file_t *wf, const float *in_frames, size_t frame_count) {
    if (!wf || !wf->fp || !wf->is_writing || !in_frames) return 0;

    size_t total_samples = frame_count * wf->channels;
    int16_t raw[2048];
    size_t written_samples = 0;

    while (written_samples < total_samples) {
        size_t chunk = total_samples - written_samples;
        if (chunk > 2048) chunk = 2048;

        for (size_t i = 0; i < chunk; i++) {
            float s = in_frames[written_samples + i];
            if (s > 1.0f) s = 1.0f;
            else if (s < -1.0f) s = -1.0f;
            raw[i] = (int16_t)(s * 32767.0f);
        }

        size_t n = fwrite(raw, sizeof(int16_t), chunk, wf->fp);
        written_samples += n;
        wf->data_bytes_written += (uint32_t)(n * sizeof(int16_t));
        if (n < chunk) break;
    }

    return written_samples / wf->channels;
}

bool wav_rewind(wav_file_t *wf) {
    if (!wf || !wf->fp || wf->is_stdin || wf->is_writing) return false;
    fseek(wf->fp, wf->data_chunk_pos, SEEK_SET);
    wf->eof = false;
    return true;
}

void wav_close(wav_file_t *wf) {
    if (!wf) return;

    if (wf->fp) {
        if (wf->is_writing) {
            /* Update RIFF and data chunk sizes */
            fseek(wf->fp, 4, SEEK_SET);
            uint32_t riff_size = wf->data_bytes_written + 36;
            fwrite(&riff_size, sizeof(uint32_t), 1, wf->fp);

            fseek(wf->fp, 40, SEEK_SET);
            fwrite(&wf->data_bytes_written, sizeof(uint32_t), 1, wf->fp);
        }

        if (!wf->is_stdin) {
            fclose(wf->fp);
        }
        wf->fp = NULL;
    }
    free(wf);
}
