#ifndef _KRAND64_H_
#define _KRAND64_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USER_SPACE
#include <pthread.h>
#endif

/*
 * set_kseed64() - initialise the global PRNG seed.
 * Also initialises the internal mutex used by krand64().
 * Must be called once at startup before using krand64().
 */
void set_kseed64( uint64_t seed);

/*
 * krand64() - thread-safe 64-bit pseudo-random number generator.
 * Protected by an internal pthread_mutex_t (user-space builds).
 * Returns a value in [0, max) if max > 0, else raw 64-bit output.
 */
uint64_t krand64( uint64_t max);

/*
 * krand64_r() - reentrant 64-bit PRNG.
 * Caller supplies its own state. No locking needed.
 * state must be initialised to a non-zero seed before first call.
 * Returns a value in [0, max) if max > 0, else raw 64-bit output.
 */
uint64_t krand64_r( uint64_t *state, uint64_t max);

#endif
