/*
 * PiFmRds-ng - Next-Generation FM/RDS transmitter
 * Modernized for 2026 standards
 *
 * RDS String Encoding, Diacritic Normalization, and PS Paginator
 */

#ifndef RDS_STRINGS_H
#define RDS_STRINGS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Convert UTF-8 string to RDS 8-bit charset / normalized ASCII safe for receivers */
void rds_sanitize_string(const char *input, char *output, size_t max_len);

/* Dynamic PS generator: manages word-boundary paging or smooth scrolling */
typedef struct {
    char full_text[256];
    char pages[32][9];       /* Up to 32 8-character pages */
    size_t page_count;
    size_t current_page;
    size_t scroll_offset;
    uint32_t last_update_ms;
    uint32_t interval_ms;
    int mode;                /* 0 = static, 1 = paged, 2 = scrolling */
} rds_ps_paginator_t;

void rds_ps_paginator_init(rds_ps_paginator_t *p);
void rds_ps_paginator_set_text(rds_ps_paginator_t *p, const char *text, int mode, uint32_t interval_ms);
/* Returns true if PS has updated, filling out_ps (8 chars + null) */
bool rds_ps_paginator_tick(rds_ps_paginator_t *p, uint32_t now_ms, char *out_ps);

#ifdef __cplusplus
}
#endif

#endif /* RDS_STRINGS_H */
