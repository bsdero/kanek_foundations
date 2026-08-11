#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dumphex.h"
#include "ta.h"

static int passed = 0;
static int failed = 0;

#define CHECK(cond, desc) do {                          \
    if ( cond) {                                         \
        printf( "  PASS: %s\n", (desc));                 \
        passed++;                                       \
    } else {                                            \
        printf( "  FAIL: %s\n", (desc));                 \
        failed++;                                       \
    }                                                   \
} while ( 0)

int main( void){
    ta_list_t ta;
    char *p, *q;
    char str2[128];

    printf( "=== Basic allocation and tracking ===\n");

    ta_list_init( &ta);
    p = ta_malloc( &ta, 32);

    memset( p, 'a', 32);
    q = ta_strndup( &ta, "hola", 4);
    ta_strdup( &ta, "anita lava la tina");

    ta_mark( q);
    sprintf( str2, "%s", "debug message");
    ta_node_set_trace( p, str2);

    printf( "total=%ld\n", ta_list_total_mem( &ta));
    ta_dump_list( &ta);
    ta_free( p);

    ta_dump_list( &ta);
    ta_free( q);

    ta_dump_list( &ta);
    ta_list_destroy( &ta);

    printf( "\n=== ta_list_reset ===\n");

    /* Sub-test a: allocate N items, verify total_mem. */
    ta_list_init( &ta);
    ta_malloc( &ta, 16);
    ta_malloc( &ta, 32);
    ta_malloc( &ta, 64);
    CHECK( ta_list_total_mem( &ta) == 112,
          "total_mem == 112 after 3 allocs (16+32+64)");

    /* Sub-test b: reset frees all allocations. */
    ta_list_reset( &ta);
    CHECK( ta_list_total_mem( &ta) == 0,
          "ta_list_total_mem() == 0 after ta_list_reset()");

    /* Sub-test c: list is reusable after reset. */
    ta_malloc( &ta, 20);
    ta_malloc( &ta, 20);
    ta_malloc( &ta, 20);
    CHECK( ta_list_total_mem( &ta) == 60,
          "ta_list_total_mem() == 60 after 3 * 20-byte allocs");

    ta_list_destroy( &ta);

    printf( "\n--- RESULTS ---\n");
    printf( "PASSED: %d  FAILED: %d\n", passed, failed);
    return( (failed > 0) ? 1 : 0);
}
