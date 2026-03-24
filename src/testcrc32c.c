/*
 * testcrc32c.c
 * Tests for the CRC-32C checksum module (crc32c).
 *
 * Part of the Kanek Foundation Library (KFL).
 * KANEK Storage Project.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "crc32c.h"

static int passed = 0;
static int failed = 0;

#define CHECK(cond, desc) do {                          \
    if (cond) {                                         \
        printf("  PASS: %s\n", (desc));                 \
        passed++;                                       \
    } else {                                            \
        printf("  FAIL: %s\n", (desc));                 \
        failed++;                                       \
    }                                                   \
} while (0)

int main(void)
{
    uint32_t crc;

    printf("=== CRC-32C standard test vectors ===\n");

    /* Test 1: empty buffer → 0 */
    crc = kfl_crc32c(0, "", 0);
    CHECK(crc == 0x00000000U, "kfl_crc32c(0, \"\", 0) == 0x00000000");

    /* Test 2: single zero byte → 0x527D5351
     * (CRC-32C with standard XOR init/final convention) */
    crc = kfl_crc32c(0, "\x00", 1);
    CHECK(crc == 0x527D5351U,
          "kfl_crc32c(0, \"\\x00\", 1) == 0x527D5351");

    /* Test 3: "123456789" → 0xE3069283 */
    crc = kfl_crc32c(0, "123456789", 9);
    CHECK(crc == 0xE3069283U,
          "kfl_crc32c(0, \"123456789\", 9) == 0xE3069283");

    printf("\n=== Chaining ===\n");

    /* Test 4: chaining two halves of an 8-byte buffer */
    {
        const uint8_t buf[8] = { 0x01, 0x02, 0x03, 0x04,
                                  0x05, 0x06, 0x07, 0x08 };
        uint32_t full  = kfl_crc32c(0, buf, 8);
        uint32_t chain = kfl_crc32c(
                            kfl_crc32c(0, buf,     4),
                            buf + 4, 4);
        CHECK(full == chain,
              "crc32c(crc32c(0,buf,4), buf+4, 4) == crc32c(0,buf,8)");
    }

    printf("\n=== kfl_crc32c_verify ===\n");

    /* Test 5: correctly assembled 20-byte struct → verify returns 1 */
    {
        uint8_t buf[20];
        uint32_t computed;
        int i;

        for (i = 0; i < 16; i++)
            buf[i] = (uint8_t)(i + 1);

        computed = kfl_crc32c(0, buf, 16);

        /* Write CRC little-endian into bytes 16-19. */
        buf[16] = (uint8_t)(computed & 0xFFU);
        buf[17] = (uint8_t)((computed >>  8) & 0xFFU);
        buf[18] = (uint8_t)((computed >> 16) & 0xFFU);
        buf[19] = (uint8_t)((computed >> 24) & 0xFFU);

        CHECK(kfl_crc32c_verify(buf, 20) == 1,
              "kfl_crc32c_verify on correct struct returns 1");

        /* Test 6: flip one bit → verify returns 0 */
        buf[5] ^= 0x01;
        CHECK(kfl_crc32c_verify(buf, 20) == 0,
              "kfl_crc32c_verify on corrupted struct returns 0");
    }

    printf("\n--- RESULTS ---\n");
    printf("PASSED: %d  FAILED: %d\n", passed, failed);
    return (failed > 0) ? 1 : 0;
}
