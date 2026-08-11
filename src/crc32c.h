#ifndef _CRC32C_H_
#define _CRC32C_H_

#ifdef USER_SPACE
#include <stdint.h>
#include <stddef.h>
#else
#include <linux/types.h>
#include <linux/string.h>
#endif

/*
 * kfl_crc32c() - compute CRC-32C of a byte buffer.
 *
 * Uses hardware acceleration when available:
 *   x86/x86-64: SSE4.2 _mm_crc32_u* intrinsics
 *   ARM/AArch64: __crc32cb/__crc32cw/__crc32cd intrinsics
 * Falls back to a software lookup table otherwise.
 *
 * crc: initial value. Pass 0 for a fresh computation.
 *      Pass the result of a prior call to chain buffers:
 *        uint32_t c = kfl_crc32c(0,   buf1, len1);
 *        c           = kfl_crc32c(c,   buf2, len2);
 * Returns the CRC-32C of the input.
 */
uint32_t kfl_crc32c(
    uint32_t    crc,
    const void *buf,
    size_t      len
);

/*
 * kfl_crc32c_verify() - verify a struct protected by CRC-32C.
 *
 * Convention used throughout the KANEK stack:
 *   the last 4 bytes of every on-disk struct are the
 *   little-endian CRC-32C of all preceding bytes.
 *
 * buf: pointer to the struct.
 * len: total size of the struct including the 4-byte CRC.
 *
 * Returns 1 if the CRC matches, 0 if the data is corrupted.
 */
int kfl_crc32c_verify( const void *buf, size_t len);

#endif
