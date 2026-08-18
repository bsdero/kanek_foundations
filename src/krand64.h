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
 *
 * KERNEL-SPACE CAVEAT: in non-USER_SPACE builds the internal mutex
 * does not exist, so this write to the global seed is unsynchronized.
 * Concurrent callers will race. See krand64() below.
 */
void set_kseed64( uint64_t seed);

/*
 * krand64() - thread-safe 64-bit pseudo-random number generator.
 * Protected by an internal pthread_mutex_t in user-space builds.
 *
 * KERNEL-SPACE CAVEAT: in non-USER_SPACE builds the global seed is
 * NOT protected by any lock — pthread is unavailable there and no
 * kernel-space equivalent (e.g. a spinlock) has been substituted yet.
 * Concurrent kernel-space callers must serialize calls externally
 * (e.g. hold a spinlock around set_kseed64()/krand64()), or use the
 * reentrant krand64_r() with per-caller state instead. Proper
 * kernel-space locking is planned for a future version.
 *
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
