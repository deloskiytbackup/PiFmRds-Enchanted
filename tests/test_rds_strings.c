#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "rds_strings.h"

int main(void) {
    printf("[TEST] Running test_rds_strings...\n");

    /* Test 1: UTF-8 Polish diacritic normalization */
    const char *pl_input = "Zażółć gęślą jaźń";
    char pl_output[64] = {0};
    rds_sanitize_string(pl_input, pl_output, sizeof(pl_output));
    printf("  Input:  '%s'\n", pl_input);
    printf("  Output: '%s'\n", pl_output);
    assert(strcmp(pl_output, "Zazolc gesla jazn") == 0);

    /* Test 2: German / Western European characters */
    const char *de_input = "Über Mäßig";
    char de_output[64] = {0};
    rds_sanitize_string(de_input, de_output, sizeof(de_output));
    printf("  Input:  '%s'\n", de_input);
    printf("  Output: '%s'\n", de_output);
    assert(strcmp(de_output, "Uber Massig") == 0 || strcmp(de_output, "Uber Masig") == 0);

    /* Test 3: Dynamic PS word boundary paging */
    rds_ps_paginator_t paginator;
    rds_ps_paginator_init(&paginator);
    rds_ps_paginator_set_text(&paginator, "RADIO POLSKIE FM 2026", 1, 1000); /* Paged mode */

    printf("  Generated %zu pages for Dynamic PS:\n", paginator.page_count);
    for (size_t i = 0; i < paginator.page_count; i++) {
        printf("    Page %zu: '[%s]' (len=%zu)\n", i, paginator.pages[i], strlen(paginator.pages[i]));
        assert(strlen(paginator.pages[i]) == 8);
    }
    assert(paginator.page_count >= 3);

    /* Check ticking */
    char out_ps[9] = {0};
    bool ticked = rds_ps_paginator_tick(&paginator, 1500, out_ps);
    assert(ticked == true);
    assert(strlen(out_ps) == 8);

    /* Test 4: Extended European diacritics (Spanish & Czech) */
    const char *intl_input = "Español Příliš Žluť";
    char intl_output[64] = {0};
    rds_sanitize_string(intl_input, intl_output, sizeof(intl_output));
    printf("  Intl Input:  '%s'\n", intl_input);
    printf("  Intl Output: '%s'\n", intl_output);
    assert(strcmp(intl_output, "Espanol Prilis Zlut") == 0);

    /* Test 5: Dynamic PS smooth scrolling mode */
    rds_ps_paginator_t scroll_pag;
    rds_ps_paginator_init(&scroll_pag);
    rds_ps_paginator_set_text(&scroll_pag, "NOW PLAYING", 2, 500); /* Mode 2: Scrolling */
    assert(scroll_pag.scroll_len == strlen("NOW PLAYING") + 8);

    char scroll_ps[9] = {0};
    bool sc_ticked = rds_ps_paginator_tick(&scroll_pag, 600, scroll_ps);
    assert(sc_ticked == true);
    assert(strlen(scroll_ps) == 8);
    assert(strncmp(scroll_ps, "NOW PLAY", 8) == 0);

    /* Tick again */
    sc_ticked = rds_ps_paginator_tick(&scroll_pag, 1200, scroll_ps);
    assert(sc_ticked == true);
    assert(strncmp(scroll_ps, "OW PLAYI", 8) == 0);

    printf("[TEST] test_rds_strings PASSED!\n");
    return 0;
}
