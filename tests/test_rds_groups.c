#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "rds.h"

int main(void) {
    printf("[TEST] Running test_rds_groups...\n");

    rds_init();
    set_rds_pi(0x3201);
    set_rds_ps("TEST-FM ");
    set_rds_rt("Test RadioText message 2026");
    set_rds_rt_plus("Radioactive", "Imagine Dragons");
    set_rds_ta(true);

    const rds_config_t *cfg = rds_get_config();
    assert(cfg->pi == 0x3201);
    assert(strncmp(cfg->ps, "TEST-FM ", 8) == 0);
    assert(cfg->ta == true);
    assert(cfg->rt_plus_enabled == true);

    /* Generate multiple groups to test group scheduling */
    uint16_t blocks[4];
    for (int i = 0; i < 16; i++) {
        rds_build_group(blocks);
        /* Block 1 must always contain the PI code */
        assert(blocks[0] == 0x3201);
        
        uint8_t group_type = (blocks[1] >> 12) & 0x0F;
        char group_version = ((blocks[1] >> 11) & 1) ? 'B' : 'A';
        printf("  Group generated: %d%c (Block 2: 0x%04X, Block 3: 0x%04X, Block 4: 0x%04X)\n",
               group_type, group_version, blocks[1], blocks[2], blocks[3]);
    }

    /* Test baseband sample output */
    float samples[192 * 4];
    get_rds_samples(samples, 192 * 4);
    for (size_t i = 0; i < 192 * 4; i++) {
        /* Samples should be well-behaved floats */
        assert(samples[i] >= -2.0f && samples[i] <= 2.0f);
    }

    rds_cleanup();
    printf("[TEST] test_rds_groups PASSED!\n");
    return 0;
}
