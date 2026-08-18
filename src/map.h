#ifndef _MAP_H_
#define _MAP_H_

#ifdef USER_SPACE
 #include <stdint.h>
 #include <string.h>
#else
 #include <linux/types.h>
 #include <linux/string.h>
#endif


/*
 * Concurrency note: none of the bm_* / byte_* functions below do any
 * locking, and their read-modify-write on bm[] bytes is not atomic.
 * This is intentional — bitmap synchronization (e.g. serializing a
 * bm_find() + bm_set_extent()/bm_set_bit() claim pair, or guarding
 * concurrent writers to the same bm buffer) is the responsibility of
 * the upper layer (KFS block/inode allocation code), not this library.
 */

#define CLEARBIT                                   0
#define SETBIT                                     1

#ifndef min
#define min(a,b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b;       \
})
#endif


/* ── byte-level primitives ───────────────────────────────────────────── */

/* Set or clear numbits bits in byte starting at bit position start.
 * Returns the updated byte. */
unsigned char byte_set_bits( int start,
                             int numbits,
                             unsigned char byte,
                             int clear_or_set);

/* Count contiguous 0s or 1s in byte starting at start, up to numbits.
 * clear_or_set selects which value to count.
 * Returns the count of contiguous matching bits. */
int byte_count_bits( int start,
                     int numbits,
                     unsigned char byte,
                     int clear_or_set);

/* Find a gap of numbits zero-bits in byte starting at start.
 * *count is updated with the number of contiguous zero bits found.
 * Returns:
 *  >= 0  bit offset of the gap (gap fully fits in the byte)
 *    -1  no gap found; *count == 0
 *    -2  end of byte reached before finding numbits zeros;
 *        *count holds the partial count at the end of the byte */
int byte_find_gap( int start, int numbits, unsigned char byte, int *count);

/* Get the value (0 or 1) of a single bit in a byte. */
int byte_get_bit( unsigned char byte, int bit);


/* ── bitmap operations ───────────────────────────────────────────────── */

/* Set or clear a single bit in bitmap bm (total_bits wide).
 * Returns 0 on success, -1 if bit_address is out of range. */
int bm_set_bit( unsigned char *bm,
                uint64_t total_bits,
                uint64_t bit_address,
                int clear_or_set);

/* Get the value (0 or 1) of a single bit.
 * Returns 0 or 1, or -1 if bit_address is out of range. */
int bm_get_bit( unsigned char *bm,
                uint64_t total_bits,
                uint64_t bit_address);

/* Set or clear num_bits_to_set contiguous bits starting at bit_address.
 * Returns 0 on success, -1 if the range is out of bounds. */
int bm_set_extent( unsigned char *bm,
                   uint64_t total_bits,
                   uint64_t bit_address,
                   uint64_t num_bits_to_set,
                   int clear_or_set);

/* Count contiguous 0s or 1s (selected by clear_or_set) starting at
 * bit_address, up to num_bits_to_count bits.
 * *count_result receives the count.
 * Returns 0 on success, -1 if the range is out of bounds. */
int bm_count( unsigned char *bm,
              uint64_t total_bits,
              uint64_t bit_address,
              uint64_t num_bits_to_count,
              int clear_or_set,
              uint64_t *count_result);

/* Find the first run of gap_size contiguous zero bits in bm, searching
 * up to count bits starting at bit_address.
 * On success returns 0 and sets *found_address to the bit offset of
 * the gap.  Returns -1 if no gap was found. */
int bm_find( unsigned char *bm,
             uint64_t total_bits,
             uint64_t bit_address,
             uint64_t count,
             uint64_t gap_size,
             uint64_t *found_address);

/* Check whether a region of count bits starting at bit_address can grow
 * (i.e. the trailing bits after any leading 1s are all 0).
 *
 * Returns:
 *   0    the region can grow (trailing bits are all 0)
 *  -1    invalid arguments
 *  -2    region starts with 0s but not all 0s (mixed leading 0s/1s)
 *  -3    region is entirely 1s, no room to grow
 *  -4    internal: should not happen
 *  -5    region has leading 1s followed by 0s then more 1s (fragmented)
 * -100   bm_count internal error (first pass)
 * -101   bm_count internal error (second pass)
 * -102   bm_count internal error (third pass) */
int bm_extent_can_grow( unsigned char *bm,
                        uint64_t total_bits,
                        uint64_t bit_address,
                        uint64_t count);

/* ── whole-bitmap utilities ──────────────────────────────────────────── */

/* Clear all bits (mark all as free). */
void bm_zero( unsigned char *bm, uint64_t total_bits);

/* Set all bits (mark all as used). */
void bm_fill( unsigned char *bm, uint64_t total_bits);

/* Return the total number of set (1) bits in the bitmap. */
uint64_t bm_popcount( unsigned char *bm, uint64_t total_bits);

#endif
