#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef USER_SPACE
#include <pthread.h>
#endif
#include "krand64.h"


static uint64_t _kseed64 = 1;

#ifdef USER_SPACE
static pthread_mutex_t _krand64_mtx = PTHREAD_MUTEX_INITIALIZER;
#endif

void set_kseed64( uint64_t seed){
#ifdef USER_SPACE
    pthread_mutex_lock( &_krand64_mtx);
#endif
    _kseed64 = seed;
#ifdef USER_SPACE
    pthread_mutex_unlock( &_krand64_mtx);
#endif
}

uint64_t krand64( uint64_t max){
    uint64_t seed, a, b, c, ac, seed0, seed1, seed2, rc;

#ifdef USER_SPACE
    pthread_mutex_lock( &_krand64_mtx);
#endif

    a = 0xbadbabe;
    b = 0xc007c0ffee;
    c = 0xdeadbeef;

    ac = 0xb00;
    seed = _kseed64++;

    seed0 = seed - 0xbadd0d0;
    seed1 = seed;
    seed2 = seed + 0xc007dad;

    seed0 = ( (( seed1 * c) >> 32) +  (( seed2 * b) >> 32) + (seed0 * c) +
               (seed1 * b) + (seed2 * a)  );
    seed1 = ( (( seed2 * c) >> 32) + ( seed1 * c) + ( seed2 * b));
    seed2 = ( seed2 * c) + ac;

    seed1 += seed2 >> 32;
    seed0 += seed1 >> 32;

    seed0 &= 0xffffffff;
    seed1 &= 0xffffffff;

    rc = (seed0 << 32) | seed1;

    if ( max > 0){
       rc = rc % max;
    }

#ifdef USER_SPACE
    pthread_mutex_unlock( &_krand64_mtx);
#endif

    return( rc);
}

uint64_t krand64_r( uint64_t *state, uint64_t max){
    uint64_t seed, a, b, c, ac, seed0, seed1, seed2, rc;

    a = 0xbadbabe;
    b = 0xc007c0ffee;
    c = 0xdeadbeef;

    ac = 0xb00;
    seed = (*state)++;

    seed0 = seed - 0xbadd0d0;
    seed1 = seed;
    seed2 = seed + 0xc007dad;

    seed0 = ( (( seed1 * c) >> 32) +  (( seed2 * b) >> 32) + (seed0 * c) +
               (seed1 * b) + (seed2 * a)  );
    seed1 = ( (( seed2 * c) >> 32) + ( seed1 * c) + ( seed2 * b));
    seed2 = ( seed2 * c) + ac;

    seed1 += seed2 >> 32;
    seed0 += seed1 >> 32;

    seed0 &= 0xffffffff;
    seed1 &= 0xffffffff;

    rc = (seed0 << 32) | seed1;

    if ( max > 0){
       rc = rc % max;
    }

    return( rc);
}
