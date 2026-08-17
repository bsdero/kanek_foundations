#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "krand64.h"

#define DICE_SIDES     100
#define SAMPLES_FAST   2000000
#define SAMPLES_FULL   100000000
#define THREAD_ITERS   1000000
#define EDGE_ITERS     1000
#define SEQ_LEN        1000
#define CONTENTION_THREADS  8

/* Chi-square critical value for df = DICE_SIDES - 1 = 99, alpha = 0.01
 * (Wilson-Hilferty approximation, matches published chi-square tables).
 */
#define CHI2_CRITICAL  134.642

typedef struct {
    uint64_t state;
    uint64_t result;
} rarg_t;

typedef struct {
    uint64_t *results;
    long n;
} cont_arg_t;

static void *thread_global( void *arg){
    int i;
    uint64_t *last = (uint64_t *)arg;
    for ( i = 0; i < THREAD_ITERS; i++) {
        *last = krand64( DICE_SIDES);
    }
    return( NULL);
}

static void *thread_reentrant( void *arg){
    int i;
    rarg_t *a = (rarg_t *)arg;
    for ( i = 0; i < THREAD_ITERS; i++) {
        a->result = krand64_r( &a->state, DICE_SIDES);
    }
    return( NULL);
}

static void *thread_contention( void *arg){
    cont_arg_t *a = (cont_arg_t *)arg;
    long i;
    for ( i = 0; i < a->n; i++) {
        a->results[i] = krand64( DICE_SIDES);
    }
    return( NULL);
}

static double chi_square_stat( int *counts, int nbuckets, long samples){
    double expected, diff, chi2;
    int i;

    expected = (double)samples / (double)nbuckets;
    chi2 = 0.0;
    for ( i = 0; i < nbuckets; i++) {
        diff = (double)counts[i] - expected;
        chi2 += ( diff * diff) / expected;
    }
    return( chi2);
}

static void test_distribution( long samples){
    int test_vals[DICE_SIDES];
    double chi2;
    long i;
    int sample;

    printf( "\n--- distribution test (samples=%ld) ---\n", samples);
    memset( &test_vals, 0, sizeof( test_vals));
    set_kseed64( 1);
    for ( i = 0; i < samples; i++) {
        sample = krand64( DICE_SIDES);
        test_vals[sample]++;
    }

    printf( "Expected probability: %6.6f\n", 1.0 / (float)DICE_SIDES);
    for ( i = 0; i < DICE_SIDES; i++) {
        printf( "%ld : [ %d, %6.6f]\n", i,
                test_vals[i],
                ((float)test_vals[i]) / ((float)samples));
    }

    chi2 = chi_square_stat( test_vals, DICE_SIDES, samples);
    printf( "chi-square statistic: %6.6f (critical value: %6.6f)\n",
            chi2, CHI2_CRITICAL);
    assert( chi2 < CHI2_CRITICAL);
    printf( "distribution test passed\n");
}

static void test_max_zero( void){
    uint64_t first, val;
    int i, distinct;

    printf( "\n--- max == 0 (raw passthrough) test ---\n");
    set_kseed64( 7);
    first = krand64( 0);
    distinct = 0;
    for ( i = 0; i < EDGE_ITERS; i++) {
        val = krand64( 0);
        if ( val != first) {
            distinct = 1;
        }
    }
    assert( distinct == 1);
    printf( "max == 0 test passed\n");
}

static void test_max_one( void){
    uint64_t val;
    int i;

    printf( "\n--- max == 1 test ---\n");
    set_kseed64( 11);
    for ( i = 0; i < EDGE_ITERS; i++) {
        val = krand64( 1);
        assert( val == 0);
    }
    printf( "max == 1 test passed\n");
}

static void test_max_near_uint64_max( void){
    uint64_t val, max;
    int i;

    printf( "\n--- max near UINT64_MAX test ---\n");
    set_kseed64( 13);
    max = UINT64_MAX - 1;
    for ( i = 0; i < EDGE_ITERS; i++) {
        val = krand64( max);
        assert( val < max);
    }
    printf( "max near UINT64_MAX test passed\n");
}

static void test_determinism( void){
    uint64_t seq1[SEQ_LEN], seq2[SEQ_LEN];
    int i;

    printf( "\n--- determinism test ---\n");
    set_kseed64( 12345);
    for ( i = 0; i < SEQ_LEN; i++) {
        seq1[i] = krand64( 0);
    }
    set_kseed64( 12345);
    for ( i = 0; i < SEQ_LEN; i++) {
        seq2[i] = krand64( 0);
    }
    for ( i = 0; i < SEQ_LEN; i++) {
        assert( seq1[i] == seq2[i]);
    }
    printf( "determinism test passed\n");
}

static void test_reentrant_purity( void){
    uint64_t state1, state2, v1, v2;
    int i;

    printf( "\n--- krand64_r state purity test ---\n");
    state1 = 999;
    state2 = 999;
    for ( i = 0; i < SEQ_LEN; i++) {
        v1 = krand64_r( &state1, 0);
        v2 = krand64_r( &state2, 0);
        assert( v1 == v2);
    }
    assert( state1 == state2);
    printf( "krand64_r state purity test passed\n");
}

static void test_thread_safety( void){
    pthread_t t1, t2;
    uint64_t last_global = 0;
    rarg_t rarg;

    printf( "\n--- thread safety test ---\n");
    set_kseed64( 42);
    rarg.state  = 0xdeadbeefULL;
    rarg.result = 0;

    pthread_create( &t1, NULL, thread_global,    &last_global);
    pthread_create( &t2, NULL, thread_reentrant, &rarg);
    pthread_join( t1, NULL);
    pthread_join( t2, NULL);

    printf( "global path last value:    %llu\n",
           (unsigned long long) last_global);
    printf( "reentrant path last value: %llu\n",
           (unsigned long long) rarg.result);
    printf( "thread safety test passed\n");
}

static void test_contention( long total_samples){
    pthread_t threads[CONTENTION_THREADS];
    cont_arg_t args[CONTENTION_THREADS];
    uint64_t *results[CONTENTION_THREADS];
    int counts[DICE_SIDES];
    long per_thread, total, sum, i;
    int t;
    double chi2;

    printf( "\n--- contention test (%d threads, samples=%ld) ---\n",
            CONTENTION_THREADS, total_samples);

    per_thread = total_samples / CONTENTION_THREADS;
    total = per_thread * CONTENTION_THREADS;

    for ( t = 0; t < CONTENTION_THREADS; t++) {
        results[t] = malloc( per_thread * sizeof( uint64_t));
        assert( results[t] != NULL);
        args[t].results = results[t];
        args[t].n = per_thread;
    }

    set_kseed64( 99);
    for ( t = 0; t < CONTENTION_THREADS; t++) {
        pthread_create( &threads[t], NULL, thread_contention, &args[t]);
    }
    for ( t = 0; t < CONTENTION_THREADS; t++) {
        pthread_join( threads[t], NULL);
    }

    memset( counts, 0, sizeof( counts));
    for ( t = 0; t < CONTENTION_THREADS; t++) {
        for ( i = 0; i < per_thread; i++) {
            counts[results[t][i]]++;
        }
        free( results[t]);
    }

    sum = 0;
    for ( i = 0; i < DICE_SIDES; i++) {
        sum += counts[i];
    }
    assert( sum == total);

    chi2 = chi_square_stat( counts, DICE_SIDES, total);
    printf( "aggregated chi-square statistic: %6.6f (critical value: "
            "%6.6f)\n", chi2, CHI2_CRITICAL);
    assert( chi2 < CHI2_CRITICAL);
    printf( "contention test passed (%ld samples across %d threads)\n",
            total, CONTENTION_THREADS);
}

int main(){
    long samples;

    samples = ( getenv( "KRAND_FULL") != NULL) ?
              SAMPLES_FULL : SAMPLES_FAST;

    test_distribution( samples);
    test_max_zero();
    test_max_one();
    test_max_near_uint64_max();
    test_determinism();
    test_reentrant_purity();
    test_thread_safety();
    test_contention( samples);

    return( 0);
}
