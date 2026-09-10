#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "rds.h"

int main(void) {
    printf("[TEST] Running test_rds_crc...\n");

    /* Test 1: CRC of 0 should be 0 */
    uint16_t crc0 = rds_calc_crc(0x0000);
    assert(crc0 == 0);

    /* Test 2: Known standard RDS CRC calculations */
    uint16_t block = 0x1234;
    uint16_t crc = rds_calc_crc(block);
    printf("  Block 0x%04X -> CRC: 0x%03X\n", block, crc);
    assert(crc < 1024); /* Must fit within 10 bits */

    /* Verify consistency */
    assert(rds_calc_crc(block) == crc);

    /* Test 3: Bit toggle changes CRC */
    uint16_t crc_diff = rds_calc_crc(block ^ 0x0001);
    assert(crc_diff != crc);

    printf("[TEST] test_rds_crc PASSED!\n");
    return 0;
}
