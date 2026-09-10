/*
 * PiFmRds-Enchanted - Zero-dependency Robust WAV file reader/writer
 * Modernized for 2026 standards
 * Correctly parses arbitrary RIFF chunks (fmt, data, LIST, JUNK, ID3)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wav_io.h"

#pragma pack(push, 1)
typedef struct {
    char id[4];
    uint32_t size;
} riff_chunk_header_t;

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} fmt_chunk_payload_t;
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

    /* Read RIFF header */
    char riff_tag[4];
    uint32_t file_size;
    char wave_tag[4];

    if (fread(riff_tag, 1, 4, wf->fp) == 4 &&
        fread(&file_size, 4, 1, wf->fp) == 1 &&
        fread(wave_tag, 1, 4, wf->fp) == 4 &&
        memcmp(riff_tag, "RIFF", 4) == 0 &&
        memcmp(wave_tag, "WAVE", 4) == 0) {

        bool found_fmt = false;
        bool found_data = false;

        /* Walk arbitrary chunks until 'data' is reached */
        riff_chunk_header_t chunk;
        while (fread(&chunk, sizeof(riff_chunk_header_t), 1, wf->fp) == 1) {
            if (memcmp(chunk.id, "fmt ", 4) == 0) {
                fmt_chunk_payload_t fmt;
                if (fread(&fmt, sizeof(fmt_chunk_payload_t), 1, wf->fp) == 1) {
                    wf->audio_format = fmt.audio_format;
                    wf->channels = fmt.num_channels;
                    wf->sample_rate = fmt.sample_rate;
                    wf->bits_per_sample = fmt.bits_per_sample;
                    found_fmt = true;

                    /* Skip any extra fmt bytes */
                    if (chunk.size > sizeof(fmt_chunk_payload_t)) {
                        fseek(wf->fp, (long)(chunk.size - sizeof(fmt_chunk_payload_t)), SEEK_CUR);
                    }
                }
            } else if (memcmp(chunk.id, "data", 4) == 0) {
                wf->data_chunk_pos = ftell(wf->fp);
                found_data = true;
                break;
            } else {
                /* Skip unneeded chunks (LIST, JUNK, BEXT, ID3, etc.) */
                fseek(wf->fp, (long)chunk.size, SEEK_CUR);
            }
        }

        if (found_fmt && found_data) {
            return wf;
        }
    }

    /* Fallback for raw PCM or unrecognized header */
    if (!wf->is_stdin) {
        fseek(wf->fp, 0, SEEK_SET);
    }
    wf->sample_rate = 44100;
    wf->channels = 2;
    wf->bits_per_sample = 16;
    wf->audio_format = 1;
    wf->data_chunk_pos = 0;
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
    fwrite("RIFF", 1, 4, wf->fp);
    uint32_t zero32 = 36;
    fwrite(&zero32, 4, 1, wf->fp);
    fwrite("WAVEfmt ", 1, 8, wf->fp);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, wf->fp);

    fmt_chunk_payload_t fmt;
    fmt.audio_format = 1;
    fmt.num_channels = channels;
    fmt.sample_rate = sample_rate;
    fmt.byte_rate = sample_rate * channels * (bits_per_sample / 8);
    fmt.block_align = channels * (bits_per_sample / 8);
    fmt.bits_per_sample = bits_per_sample;
    fwrite(&fmt, sizeof(fmt), 1, wf->fp);

    fwrite("data", 1, 4, wf->fp);
    zero32 = 0;
    fwrite(&zero32, 4, 1, wf->fp);

    wf->data_chunk_pos = ftell(wf->fp);
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
    } else if (wf->bits_per_sample == 32 && wf->audio_format == 3) {
        /* IEEE 32-bit float support */
        samples_read = fread(out_frames, sizeof(float), total_samples, wf->fp);
        if (samples_read < total_samples) wf->eof = true;
    } else {
        wf->eof = true;
    }

    return (wf->channels > 0) ? (samples_read / wf->channels) : 0;
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

    return (wf->channels > 0) ? (written_samples / wf->channels) : 0;
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
