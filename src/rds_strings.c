/*
 * PiFmRds-ng - Next-Generation FM/RDS transmitter
 * Modernized for 2026 standards
 *
 * RDS String Encoding, Diacritic Normalization, and PS Paginator
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "rds_strings.h"

/* Convert UTF-8 character to safe ASCII / RDS representation */
static size_t convert_utf8_char(const unsigned char *in, char *out) {
    if (in[0] < 0x80) {
        /* Standard ASCII (0x20 to 0x7E printable) */
        if (in[0] >= 32 && in[0] <= 126) {
            *out = (char)in[0];
        } else {
            *out = ' ';
        }
        return 1;
    }

    /* 2-byte UTF-8 */
    if ((in[0] & 0xE0) == 0xC0) {
        uint16_t codepoint = ((in[0] & 0x1F) << 6) | (in[1] & 0x3F);
        switch (codepoint) {
            /* Polish */
            case 0x0104: *out = 'A'; return 2; /* Ą */
            case 0x0105: *out = 'a'; return 2; /* ą */
            case 0x0106: *out = 'C'; return 2; /* Ć */
            case 0x0107: *out = 'c'; return 2; /* ć */
            case 0x0118: *out = 'E'; return 2; /* Ę */
            case 0x0119: *out = 'e'; return 2; /* ę */
            case 0x0141: *out = 'L'; return 2; /* Ł */
            case 0x0142: *out = 'l'; return 2; /* ł */
            case 0x0143: *out = 'N'; return 2; /* Ń */
            case 0x0144: *out = 'n'; return 2; /* ń */
            case 0x00D3: *out = 'O'; return 2; /* Ó */
            case 0x00F3: *out = 'o'; return 2; /* ó */
            case 0x015A: *out = 'S'; return 2; /* Ś */
            case 0x015B: *out = 's'; return 2; /* ś */
            case 0x0179: case 0x017B: *out = 'Z'; return 2; /* Ź, Ż */
            case 0x017A: case 0x017C: *out = 'z'; return 2; /* ź, ż */

            /* German / Nordic / Western Europe */
            case 0x00C4: *out = 'A'; return 2; /* Ä */
            case 0x00E4: *out = 'a'; return 2; /* ä */
            case 0x00D6: *out = 'O'; return 2; /* Ö */
            case 0x00F6: *out = 'o'; return 2; /* ö */
            case 0x00DC: *out = 'U'; return 2; /* Ü */
            case 0x00FC: *out = 'u'; return 2; /* ü */
            case 0x00DF: *out = 's'; return 2; /* ß */
            case 0x00C9: case 0x00C8: case 0x00CA: case 0x00CB: *out = 'E'; return 2; /* É, È, Ê, Ë */
            case 0x00E9: case 0x00E8: case 0x00EA: case 0x00EB: *out = 'e'; return 2; /* é, è, ê, ë */
            case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3: case 0x00C5: *out = 'A'; return 2;
            case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3: case 0x00E5: *out = 'a'; return 2;
            case 0x00C7: *out = 'C'; return 2; /* Ç */
            case 0x00E7: *out = 'c'; return 2; /* ç */
            case 0x00CD: case 0x00CC: case 0x00CE: case 0x00CF: *out = 'I'; return 2;
            case 0x00ED: case 0x00EC: case 0x00EE: case 0x00EF: *out = 'i'; return 2;
            case 0x00D1: *out = 'N'; return 2; /* Ñ */
            case 0x00F1: *out = 'n'; return 2; /* ñ */
            case 0x00DA: case 0x00D9: case 0x00DB: *out = 'U'; return 2; /* Ú, Ù, Û */
            case 0x00FA: case 0x00F9: case 0x00FB: *out = 'u'; return 2; /* ú, ù, û */
            case 0x00DD: *out = 'Y'; return 2; /* Ý */
            case 0x00FD: case 0x00FF: *out = 'y'; return 2; /* ý, ÿ */

            /* Czech / Slovak */
            case 0x010C: *out = 'C'; return 2; /* Č */
            case 0x010D: *out = 'c'; return 2; /* č */
            case 0x010E: *out = 'D'; return 2; /* Ď */
            case 0x010F: *out = 'd'; return 2; /* ď */
            case 0x011A: *out = 'E'; return 2; /* Ě */
            case 0x011B: *out = 'e'; return 2; /* ě */
            case 0x0147: *out = 'N'; return 2; /* Ň */
            case 0x0148: *out = 'n'; return 2; /* ň */
            case 0x0158: *out = 'R'; return 2; /* Ř */
            case 0x0159: *out = 'r'; return 2; /* ř */
            case 0x0160: *out = 'S'; return 2; /* Š */
            case 0x0161: *out = 's'; return 2; /* š */
            case 0x0164: *out = 'T'; return 2; /* Ť */
            case 0x0165: *out = 't'; return 2; /* ť */
            case 0x016E: *out = 'U'; return 2; /* Ů */
            case 0x016F: *out = 'u'; return 2; /* ů */
            case 0x017D: *out = 'Z'; return 2; /* Ž */
            case 0x017E: *out = 'z'; return 2; /* ž */
            default:
                *out = '?';
                return 2;
        }
    }

    /* 3-byte UTF-8 */
    if ((in[0] & 0xF0) == 0xE0) {
        *out = '?';
        return 3;
    }

    /* 4-byte UTF-8 */
    if ((in[0] & 0xF8) == 0xF0) {
        *out = '?';
        return 4;
    }

    *out = ' ';
    return 1;
}

void rds_sanitize_string(const char *input, char *output, size_t max_len) {
    if (!input || !output || max_len == 0) return;

    size_t in_idx = 0;
    size_t out_idx = 0;
    size_t in_len = strlen(input);

    while (in_idx < in_len && out_idx < max_len - 1) {
        char converted = ' ';
        size_t bytes = convert_utf8_char((const unsigned char *)&input[in_idx], &converted);
        output[out_idx++] = converted;
        in_idx += bytes;
    }

    output[out_idx] = '\0';
}

void rds_ps_paginator_init(rds_ps_paginator_t *p) {
    if (!p) return;
    memset(p, 0, sizeof(rds_ps_paginator_t));
    p->interval_ms = 2000;
}

void rds_ps_paginator_set_text(rds_ps_paginator_t *p, const char *text, int mode, uint32_t interval_ms) {
    if (!p) return;
    memset(p, 0, sizeof(rds_ps_paginator_t));
    p->mode = mode;
    p->interval_ms = (interval_ms > 0) ? interval_ms : 2000;

    if (!text) {
        strncpy(p->full_text, "PIFMRDS", sizeof(p->full_text) - 1);
    } else {
        rds_sanitize_string(text, p->full_text, sizeof(p->full_text));
    }

    size_t len = strlen(p->full_text);

    if (p->mode == 0 || len <= 8) {
        /* Static mode */
        p->mode = 0;
        p->page_count = 1;
        snprintf(p->pages[0], sizeof(p->pages[0]), "%-8.8s", p->full_text);
        return;
    }

    if (p->mode == 1) {
        /* Paged mode with word-boundary splitting */
        char words[64][32];
        size_t word_count = 0;
        char temp[256];
        strncpy(temp, p->full_text, sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';

        char *token = strtok(temp, " ");
        while (token && word_count < 64) {
            strncpy(words[word_count++], token, 31);
            token = strtok(NULL, " ");
        }

        char current_page[16] = {0};
        p->page_count = 0;

        for (size_t i = 0; i < word_count && p->page_count < 32; i++) {
            size_t cur_len = strlen(current_page);
            size_t w_len = strlen(words[i]);

            if (cur_len == 0) {
                if (w_len <= 8) {
                    strncpy(current_page, words[i], sizeof(current_page) - 1);
                } else {
                    /* Long word, split it */
                    for (size_t k = 0; k < w_len && p->page_count < 32; k += 8) {
                        snprintf(p->pages[p->page_count++], 9, "%-8.8s", &words[i][k]);
                    }
                    current_page[0] = '\0';
                }
            } else if (cur_len + 1 + w_len <= 8) {
                strcat(current_page, " ");
                strcat(current_page, words[i]);
            } else {
                /* Flush current page */
                snprintf(p->pages[p->page_count++], 9, "%-8.8s", current_page);
                current_page[0] = '\0';
                i--; /* Re-evaluate this word for the new page */
            }
        }

        if (strlen(current_page) > 0 && p->page_count < 32) {
            snprintf(p->pages[p->page_count++], 9, "%-8.8s", current_page);
        }

        if (p->page_count == 0) {
            snprintf(p->pages[0], 9, "%-8.8s", p->full_text);
            p->page_count = 1;
        }
    } else if (p->mode == 2) {
        /* Scrolling mode: precompute padded wrap-around string once */
        p->scroll_offset = 0;
        p->scroll_len = len + 8;
        snprintf(p->scroll_buf, sizeof(p->scroll_buf), "%s        %s", p->full_text, p->full_text);
    }
}

bool rds_ps_paginator_tick(rds_ps_paginator_t *p, uint32_t now_ms, char *out_ps) {
    if (!p || !out_ps) return false;

    if (p->mode == 0) {
        /* Static mode */
        memcpy(out_ps, p->pages[0], 8);
        out_ps[8] = '\0';
        return false;
    }

    if (now_ms - p->last_update_ms < p->interval_ms && p->last_update_ms != 0) {
        return false;
    }

    p->last_update_ms = now_ms;

    if (p->mode == 1) {
        /* Paged mode */
        if (p->page_count > 0) {
            memcpy(out_ps, p->pages[p->current_page], 8);
            out_ps[8] = '\0';
            p->current_page = (p->current_page + 1) % p->page_count;
            return true;
        }
    } else if (p->mode == 2) {
        /* Scrolling mode: zero-copy lookup from precomputed buffer */
        if (p->scroll_len > 0 && p->scroll_offset + 8 <= sizeof(p->scroll_buf)) {
            memcpy(out_ps, &p->scroll_buf[p->scroll_offset], 8);
            out_ps[8] = '\0';
            p->scroll_offset = (p->scroll_offset + 1) % p->scroll_len;
            return true;
        }
    }

    return false;
}
