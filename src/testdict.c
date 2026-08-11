#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ta.h"
#include "var.h"
#include "dict.h"

static int passed = 0;
static int failed = 0;

#define CHECK(cond, desc) do {                              \
    if ( cond) {                                             \
        printf( "  PASS: %s\n", (desc));                     \
        passed++;                                           \
    } else {                                                \
        printf( "  FAIL: %s\n", (desc));                     \
        failed++;                                           \
    }                                                       \
} while ( 0)

int main( void){
    ta_list_t ta;
    ta_list_init( &ta);

    printf( "=== dict_new_sized: valid sizes ===\n");

    /* bucket_count = 4 (minimum valid) */
    dict_t *d4 = dict_new_sized( &ta, 4);
    CHECK( d4 != NULL, "dict_new_sized(4) returns non-NULL");
    if ( d4 != NULL) {
        CHECK( d4->nbuckets == 4, "nbuckets == 4");
    }

    /* bucket_count = 256 (large table) */
    dict_t *d256 = dict_new_sized( &ta, 256);
    CHECK( d256 != NULL, "dict_new_sized(256) returns non-NULL");
    if ( d256 != NULL) {
        CHECK( d256->nbuckets == 256, "nbuckets == 256");
    }

    printf( "\n=== dict_new_sized: invalid sizes ===\n");

    /* bucket_count = 3 (not a power of 2) */
    dict_t *d3 = dict_new_sized( &ta, 3);
    CHECK( d3 == NULL, "dict_new_sized(3) returns NULL (not power of 2)");

    /* bucket_count = 0 (invalid) */
    dict_t *d0 = dict_new_sized( &ta, 0);
    CHECK( d0 == NULL, "dict_new_sized(0) returns NULL");

    printf( "\n=== 200 entries in 256-bucket table ===\n");

    if ( d256 != NULL) {
        char key[32];
        int  i;

        /* Insert 200 entries */
        for ( i = 0; i < 200; i++) {
            snprintf( key, sizeof( key), "key_%d", i);
            dict_set( d256, key, var_int( &ta, (int64_t)i));
        }

        CHECK( dict_count( d256) == 200,
              "dict_count == 200 after 200 inserts");

        /* Verify all 200 are retrievable */
        int all_ok = 1;
        for ( i = 0; i < 200; i++) {
            snprintf( key, sizeof( key), "key_%d", i);
            var_t *v = dict_get( d256, key);
            if ( v == NULL || var_to_int( v) != (int64_t)i) {
                all_ok = 0;
                break;
            }
        }
        CHECK( all_ok, "all 200 entries retrievable from 256-bucket table");
    }

    ta_list_destroy( &ta);

    printf( "\n--- RESULTS ---\n");
    printf( "PASSED: %d  FAILED: %d\n", passed, failed);
    return( (failed > 0) ? 1 : 0);
}
