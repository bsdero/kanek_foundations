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

static int     each_visit_count = 0;
static int64_t each_sum         = 0;

static void count_and_sum_cb( const char *key, var_t *val, void *userdata){
    (void)key;
    (void)userdata;
    each_visit_count++;
    each_sum += var_to_int( val);
}

static void delete_current_cb( const char *key, var_t *val, void *userdata){
    dict_t *d = (dict_t *)userdata;

    (void)val;
    dict_del( d, key);
}

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

    printf( "\n=== dict_new_sized: bucket_count below minimum ===\n");

    dict_t *d2 = dict_new_sized( &ta, 2);
    CHECK( d2 == NULL,
          "dict_new_sized(2) returns NULL (power of 2, below minimum 4)");

    printf( "\n=== dict_new: default bucket count ===\n");

    dict_t *ddef = dict_new( &ta);
    CHECK( ddef != NULL, "dict_new() returns non-NULL");
    if ( ddef != NULL) {
        CHECK( ddef->nbuckets == DICT_NBUCKETS,
              "dict_new() uses DICT_NBUCKETS (64) buckets");
        CHECK( dict_count( ddef) == 0, "freshly created dict has count == 0");
    }

    printf( "\n=== NULL-argument safety ===\n");

    CHECK( dict_set( NULL, "k", NULL) == -1, "dict_set(NULL, k, v) == -1");
    CHECK( dict_get( NULL, "k") == NULL, "dict_get(NULL, k) == NULL");
    CHECK( dict_del( NULL, "k") == -1, "dict_del(NULL, k) == -1");
    CHECK( dict_has( NULL, "k") == 0, "dict_has(NULL, k) == 0");
    CHECK( dict_count( NULL) == 0, "dict_count(NULL) == 0");
    dict_each( NULL, count_and_sum_cb, NULL);
    CHECK( 1, "dict_each(NULL, ...) does not crash");
    dict_print( NULL);
    CHECK( 1, "dict_print(NULL) does not crash");

    if ( ddef != NULL) {
        CHECK( dict_set( ddef, NULL, NULL) == -1,
              "dict_set(d, NULL, v) == -1");
        CHECK( dict_get( ddef, NULL) == NULL, "dict_get(d, NULL) == NULL");
        CHECK( dict_del( ddef, NULL) == -1, "dict_del(d, NULL) == -1");
        CHECK( dict_has( ddef, NULL) == 0, "dict_has(d, NULL) == 0");
    }

    printf( "\n=== empty dict behavior ===\n");

    dict_t *dempty = dict_new_sized( &ta, 4);
    CHECK( dempty != NULL, "dict_new_sized(4) for empty-dict test != NULL");
    if ( dempty != NULL) {
        CHECK( dict_count( dempty) == 0, "empty dict: count == 0");
        CHECK( dict_get( dempty, "missing") == NULL,
              "empty dict: dict_get(missing) == NULL");
        CHECK( dict_has( dempty, "missing") == 0,
              "empty dict: dict_has(missing) == 0");
        CHECK( dict_del( dempty, "missing") == -1,
              "empty dict: dict_del(missing) == -1");
    }

    printf( "\n=== basic set/get/has/del roundtrip ===\n");

    dict_t *dbasic = dict_new_sized( &ta, 4);
    CHECK( dbasic != NULL, "dict_new_sized(4) for basic test != NULL");
    if ( dbasic != NULL) {
        var_t *v1 = var_int( &ta, 42);
        var_t *v2 = var_int( &ta, 99);
        var_t *got;

        CHECK( dict_set( dbasic, "a", v1) == 0, "dict_set(a, 42) == 0");
        CHECK( dict_count( dbasic) == 1, "count == 1 after first insert");
        CHECK( dict_has( dbasic, "a") == 1, "dict_has(a) == 1");
        CHECK( dict_get( dbasic, "a") == v1,
              "dict_get(a) returns the exact stored pointer");

        /* overwrite: count must not increase, old value must survive
         * (dict_set is documented as non-owning) */
        CHECK( dict_set( dbasic, "a", v2) == 0,
              "dict_set(a, 99) (overwrite) == 0");
        CHECK( dict_count( dbasic) == 1,
              "count still 1 after overwrite (not incremented)");
        CHECK( dict_get( dbasic, "a") == v2,
              "dict_get(a) returns the new pointer after overwrite");
        CHECK( var_to_int( v1) == 42,
              "old value (v1) still valid/unchanged after being "
              "overwritten in the dict");

        /* explicit NULL value: a legitimate, distinct case from
         * key-not-found (see dict_has's purpose) */
        CHECK( dict_set( dbasic, "b", NULL) == 0,
              "dict_set(b, NULL) == 0");
        CHECK( dict_has( dbasic, "b") == 1,
              "dict_has(b) == 1 (key exists, value is NULL)");
        got = dict_get( dbasic, "b");
        CHECK( got == NULL, "dict_get(b) == NULL (the stored value)");
        CHECK( dict_has( dbasic, "nonexistent") == 0,
              "dict_has(nonexistent) == 0 (distinct from stored-NULL "
              "case above)");

        /* delete */
        CHECK( dict_del( dbasic, "a") == 0, "dict_del(a) == 0");
        CHECK( dict_count( dbasic) == 1,
              "count == 1 after deleting one of two entries");
        CHECK( dict_has( dbasic, "a") == 0, "dict_has(a) == 0 after delete");
        CHECK( dict_get( dbasic, "a") == NULL,
              "dict_get(a) == NULL after delete");
        CHECK( dict_del( dbasic, "a") == -1,
              "deleting an already-deleted key returns -1");
    }

    printf( "\n=== chain (bucket collision) correctness ===\n");

    dict_t *dchain = dict_new_sized( &ta, 4);
    CHECK( dchain != NULL, "dict_new_sized(4) for chain test != NULL");
    if ( dchain != NULL) {
        char key[32];
        int  i, n = 20;
        int  all_ok = 1;

        for ( i = 0; i < n; i++) {
            snprintf( key, sizeof( key), "chain_%d", i);
            dict_set( dchain, key, var_int( &ta, (int64_t)i));
        }
        CHECK( dict_count( dchain) == (size_t)n,
              "count == 20 after inserting 20 keys into a 4-bucket table");

        /* Delete every key one at a time, checking after each delete
         * that the deleted key is gone and every *remaining* key is
         * still correctly retrievable. See the forward-compatibility
         * note above this task's steps regarding Task 7. */
        for ( i = 0; i < n && all_ok; i++) {
            int j;

            snprintf( key, sizeof( key), "chain_%d", i);
            if ( dict_del( dchain, key) != 0 || dict_has( dchain, key)) {
                all_ok = 0;
                break;
            }
            for ( j = i + 1; j < n; j++) {
                char   other[32];
                var_t *v;

                snprintf( other, sizeof( other), "chain_%d", j);
                v = dict_get( dchain, other);
                if ( v == NULL || var_to_int( v) != (int64_t)j) {
                    all_ok = 0;
                    break;
                }
            }
        }
        CHECK( all_ok, "deleting all 20 chained keys one at a time "
                       "leaves every remaining key intact");
        CHECK( dict_count( dchain) == 0,
              "count == 0 after deleting all keys");
    }

    printf( "\n=== dict_each: iteration ===\n");

    dict_t *deach = dict_new_sized( &ta, 4);
    CHECK( deach != NULL, "dict_new_sized(4) for dict_each test != NULL");
    if ( deach != NULL) {
        char    key[32];
        int     i, n = 10;
        int64_t expected_sum = 0;

        for ( i = 0; i < n; i++) {
            snprintf( key, sizeof( key), "e_%d", i);
            dict_set( deach, key, var_int( &ta, (int64_t)i));
            expected_sum += i;
        }

        each_visit_count = 0;
        each_sum = 0;
        dict_each( deach, count_and_sum_cb, NULL);
        CHECK( each_visit_count == n, "dict_each visits exactly n entries");
        CHECK( each_sum == expected_sum,
              "dict_each callback sees the correct key/val pairs "
              "(sum check)");
    }

    printf( "\n=== dict_each: callback may dict_del the current key ===\n");

    dict_t *deachdel = dict_new_sized( &ta, 4);
    CHECK( deachdel != NULL,
          "dict_new_sized(4) for dict_each-delete test != NULL");
    if ( deachdel != NULL) {
        char key[32];
        int  i, n = 10;

        for ( i = 0; i < n; i++) {
            snprintf( key, sizeof( key), "d_%d", i);
            dict_set( deachdel, key, var_int( &ta, (int64_t)i));
        }

        dict_each( deachdel, delete_current_cb, deachdel);
        CHECK( dict_count( deachdel) == 0,
              "dict_each callback deleting every current key empties "
              "the dict without crashing or skipping entries");
    }

    printf( "\n=== dict_print: smoke test ===\n");

    dict_t *dprint = dict_new_sized( &ta, 4);
    if ( dprint != NULL) {
        dict_set( dprint, "x", var_int( &ta, 1));
        dict_set( dprint, "y", var_str( &ta, "hello"));
        printf( "  ");
        dict_print( dprint);
        printf( "\n");
    }
    CHECK( 1, "dict_print did not crash on a populated dict");

    printf( "\n=== var_dict_new: nested dict ===\n");

    var_t *vouter = var_dict_new( &ta);
    CHECK( vouter != NULL, "var_dict_new() returns non-NULL");
    if ( vouter != NULL) {
        CHECK( vouter->type == VAR_DICT,
              "var_dict_new() wrapper has type VAR_DICT");

        dict_t *inner = (dict_t *)vouter->dict;
        CHECK( inner != NULL, "wrapped dict_t is non-NULL");
        if ( inner != NULL) {
            var_t *got;

            dict_set( inner, "nested_key", var_int( &ta, 7));
            got = dict_get( inner, "nested_key");
            CHECK( got != NULL && var_to_int( got) == 7,
                  "set/get through a var_t-wrapped dict works");
        }
    }

    printf( "\n=== dict_set: stays non-owning ===\n");

    dict_t *dnonown = dict_new_sized( &ta, 4);
    CHECK( dnonown != NULL, "dict_new_sized(4) for dict_set test != NULL");
    if ( dnonown != NULL) {
        var_t *old = var_int( &ta, 1);
        var_t *newv = var_int( &ta, 2);

        CHECK( dict_set( dnonown, "k", old) == 0,
              "dict_set(k, 1) == 0");
        CHECK( dict_set( dnonown, "k", newv) == 0,
              "dict_set(k, 2) (overwrite) == 0");
        CHECK( var_to_int( old) == 1,
              "old value survives dict_set overwrite");
        CHECK( var_to_int( dict_get( dnonown, "k")) == 2,
              "dict_get(k) returns the new value after dict_set "
              "overwrite");
    }

    printf( "\n=== dict_set_owned: frees old value and works ===\n");

    dict_t *downed = dict_new_sized( &ta, 4);
    CHECK( downed != NULL,
          "dict_new_sized(4) for dict_set_owned test != NULL");
    if ( downed != NULL) {
        var_t *first = var_int( &ta, 10);
        var_t *second = var_int( &ta, 20);

        CHECK( dict_set_owned( downed, "k", first) == 0,
              "dict_set_owned(k, 10) == 0");
        CHECK( dict_set_owned( downed, "k", second) == 0,
              "dict_set_owned(k, 20) (frees old) == 0");
        CHECK( var_to_int( dict_get( downed, "k")) == 20,
              "dict_get(k) == 20 after dict_set_owned overwrite");
    }

    printf( "\n=== dict_set_owned: NULL old value ===\n");

    dict_t *downednull = dict_new_sized( &ta, 4);
    CHECK( downednull != NULL,
          "dict_new_sized(4) for dict_set_owned NULL test != NULL");
    if ( downednull != NULL) {
        CHECK( dict_set_owned( downednull, "k", NULL) == 0,
              "dict_set_owned(k, NULL) == 0");
        CHECK( dict_set_owned( downednull, "k", var_int( &ta, 5)) == 0,
              "dict_set_owned(k, 5) over a NULL old value does not "
              "crash and returns 0");
        CHECK( var_to_int( dict_get( downednull, "k")) == 5,
              "dict_get(k) == 5 after dict_set_owned over NULL old "
              "value");
    }

    printf( "\n=== dict_rehash: automatic growth on insert ===\n");

    dict_t *dgrow = dict_new_sized( &ta, 4);
    CHECK( dgrow != NULL, "dict_new_sized(4) for growth test != NULL");
    if ( dgrow != NULL) {
        char key[32];
        int  i, n = 12;
        int  all_ok = 1;

        for ( i = 0; i < n; i++) {
            snprintf( key, sizeof( key), "grow_%d", i);
            dict_set( dgrow, key, var_int( &ta, (int64_t)i));
        }
        CHECK( dict_count( dgrow) == (size_t)n,
              "dict_count == 12 after 12 inserts into a 4-bucket table "
              "that auto-grows");

        for ( i = 0; i < n; i++) {
            var_t *v;

            snprintf( key, sizeof( key), "grow_%d", i);
            v = dict_get( dgrow, key);
            if ( v == NULL || var_to_int( v) != (int64_t)i) {
                all_ok = 0;
                break;
            }
        }
        CHECK( all_ok, "every key is still correctly retrievable after "
                       "automatic rehashing");
        CHECK( dgrow->nbuckets > 4,
              "nbuckets grew past the initial 4 (automatic growth "
              "actually happened)");

        printf( "\n=== dict_rehash: failure path (non-power-of-2) "
               "leaves d intact ===\n");

        CHECK( dict_rehash( dgrow, 5) == -1,
              "dict_rehash(d, 5) returns -1 (5 is not a power of 2)");
        CHECK( var_to_int( dict_get( dgrow, "grow_0")) == 0,
              "grow_0 still retrievable after failed rehash");
        CHECK( var_to_int( dict_get( dgrow, "grow_11")) == 11,
              "grow_11 still retrievable after failed rehash");
    }

    ta_list_destroy( &ta);

    printf( "\n--- RESULTS ---\n");
    printf( "PASSED: %d  FAILED: %d\n", passed, failed);
    return( (failed > 0) ? 1 : 0);
}
