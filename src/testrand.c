/*
 * testrand.c
 * Tests for the krand64 PRNG: distribution and thread safety.
 *
 * Part of the Kanek Foundation Library (KFL).
 * KANEK Storage Project.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "krand64.h"

#define DICE_SIDES    100
#define SAMPLES       100000000
#define THREAD_ITERS  1000000

typedef struct {
    uint64_t state;
    uint64_t result;
} rarg_t;

static void *thread_global(void *arg)
{
    int i;
    uint64_t *last = (uint64_t *)arg;
    for (i = 0; i < THREAD_ITERS; i++)
        *last = krand64(DICE_SIDES);
    return NULL;
}

static void *thread_reentrant(void *arg)
{
    int i;
    rarg_t *a = (rarg_t *)arg;
    for (i = 0; i < THREAD_ITERS; i++)
        a->result = krand64_r(&a->state, DICE_SIDES);
    return NULL;
}

int main(){
    int i;
    int sample;
    int test_vals[DICE_SIDES];
    pthread_t t1, t2;
    uint64_t last_global = 0;
    rarg_t rarg;

    memset( &test_vals, 0, sizeof( test_vals));
    set_kseed64(1);
    for( i = 0; i < SAMPLES; i++){
        sample = krand64( DICE_SIDES);
        test_vals[sample]++;
    }

    printf("Expected probability: %6.6f\n", 1.0 / (float) DICE_SIDES);
    for( i = 0; i < DICE_SIDES; i++){
        printf("%d : [ %d, %6.6f]\n", i,
                test_vals[i],
                ((float) test_vals[i]) / ((float) SAMPLES));
    }

    /* --- thread safety test --- */
    printf("\n--- thread safety test ---\n");
    set_kseed64(42);
    rarg.state  = 0xdeadbeefULL;
    rarg.result = 0;

    pthread_create(&t1, NULL, thread_global,    &last_global);
    pthread_create(&t2, NULL, thread_reentrant, &rarg);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("global path last value:    %llu\n",
           (unsigned long long) last_global);
    printf("reentrant path last value: %llu\n",
           (unsigned long long) rarg.result);
    printf("thread safety test passed\n");

    return(0);
}
