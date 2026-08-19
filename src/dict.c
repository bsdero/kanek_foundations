#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "dict.h"
#include "hash.h"
#include "trace.h"


/* ── internal helpers ──────────────────────────────────────────────────── */

/* NOTE: seed is fixed at 0. If this dict is ever used with
 * attacker-influenced keys, this is vulnerable to hash-flooding
 * (crafted keys colliding into one bucket). Not fixed here — see
 * plan.md's "Explicitly out of scope" section for why. */
/* bucket_for_n() - compute bucket index for key in a table of n buckets.
 * n must be a power of 2. */
static size_t bucket_for_n( const char *key, uint32_t n){
    uint32_t h = xxh32( key, strlen( key), 0);
    return( (size_t)(h & (n - 1)));
}

/* is_power_of_two() - return 1 if n is a power of 2, 0 otherwise. */
static int is_power_of_two( uint32_t n){
    return( (n > 0) && ((n & (n - 1)) == 0));
}


/* ── lifecycle ─────────────────────────────────────────────────────────── */

/*
 * dict_new_sized() - create a dictionary with a specific bucket count.
 * bucket_count must be a power of 2 and >= 4.
 * Returns NULL on invalid bucket_count or allocation failure.
 */
dict_t *dict_new_sized( ta_list_t *gc, uint32_t bucket_count){
    uint32_t i;
    dict_t  *d;

    if ( bucket_count < 4 || !is_power_of_two( bucket_count)) {
        TRACE_ERR( "invalid bucket_count %u", bucket_count);
        return( NULL);
    }

    d = ta_malloc( gc, sizeof( dict_t));
    if ( d == NULL) {
        TRACE_ERR( "ta_malloc failed for dict_t");
        return( NULL);
    }

    d->buckets = ta_calloc( gc, bucket_count, sizeof( list_t));
    if ( d->buckets == NULL) {
        TRACE_ERR( "ta_calloc failed for %u buckets", bucket_count);
        ta_free( d);
        return( NULL);
    }

    d->gc       = gc;
    d->nbuckets = bucket_count;
    d->count    = 0;

    for ( i = 0; i < bucket_count; i++) {
        INIT_LIST_HEAD( &d->buckets[i]);
    }
    return( d);
}

/*
 * dict_new() - create a dictionary with the default 64-bucket table.
 */
dict_t *dict_new( ta_list_t *gc){
    return( dict_new_sized( gc, DICT_NBUCKETS));
}

int dict_rehash( dict_t *d, uint32_t new_nbuckets){
    uint32_t  i;
    list_t   *old_buckets;
    uint32_t  old_nbuckets;
    list_t   *new_buckets;
    list_t   *pos, *tmp;

    if ( d == NULL) {
        TRACE_ERR( "d is NULL");
        return( -1);
    }
    if ( new_nbuckets < 4 || !is_power_of_two( new_nbuckets)) {
        TRACE_ERR( "invalid new_nbuckets %u", new_nbuckets);
        return( -1);
    }

    new_buckets = ta_calloc( d->gc, new_nbuckets, sizeof( list_t));
    if ( new_buckets == NULL) {
        TRACE_ERR( "ta_calloc failed for %u buckets", new_nbuckets);
        return( -1);
    }
    for ( i = 0; i < new_nbuckets; i++) {
        INIT_LIST_HEAD( &new_buckets[i]);
    }

    old_buckets  = d->buckets;
    old_nbuckets = d->nbuckets;

    /* Move every entry's list node into the new table. Never realloc
     * or memcpy the bucket array itself — see the comment above this
     * function's declaration in dict.h. */
    for ( i = 0; i < old_nbuckets; i++) {
        list_for_each_safe( pos, tmp, &old_buckets[i]) {
            dict_entry_t *e = container_of( pos, dict_entry_t, node);
            size_t new_b = bucket_for_n( e->key, new_nbuckets);

            list_del( pos);
            list_add_tail( pos, &new_buckets[new_b]);
        }
    }

    d->buckets  = new_buckets;
    d->nbuckets = new_nbuckets;
    ta_free( old_buckets);

    return( 0);
}


/* ── CRUD ──────────────────────────────────────────────────────────────── */

static int dict_set_impl( dict_t *d, const char *key, var_t *val,
                          int free_old){
    size_t        b;
    list_t       *pos;
    dict_entry_t *e;
    size_t        keylen;

    if ( d == NULL || key == NULL) {
        TRACE_ERR( "d or key is NULL");
        return( -1);
    }
    b = bucket_for_n( key, d->nbuckets);

    /* update in place if key already exists */
    list_for_each( pos, &d->buckets[b]) {
        e = container_of( pos, dict_entry_t, node);
        if ( strcmp( e->key, key) == 0) {
            if ( free_old && e->val != val) {
                ta_free( e->val);
            }
            e->val = val;
            return( 0);
        }
    }

    /* new entry — key is stored inline (flexible array member) so
     * this is a single allocation instead of two. */
    keylen = strlen( key);
    e = ta_malloc( d->gc, sizeof( dict_entry_t) + keylen + 1);
    if ( e == NULL) {
        TRACE_ERR( "ta_malloc failed for dict_entry_t");
        return( -1);
    }
    memcpy( e->key, key, keylen + 1);
    e->val = val;
    list_add_tail( &e->node, &d->buckets[b]);
    d->count++;

    /* grow at ~0.75 load factor; if it fails, keep going with the
     * table we already have — the insert above already succeeded, so
     * a rehash failure here must not turn into a dict_set_impl()
     * failure. Applies to both dict_set() and dict_set_owned(), since
     * both call this shared function. */
    if ( d->count * 4 >= (size_t)d->nbuckets * 3 &&
         d->nbuckets <= (UINT32_MAX / 2)) {
        dict_rehash( d, d->nbuckets * 2);
    }

    return( 0);
}

int dict_set( dict_t *d, const char *key, var_t *val){
    return( dict_set_impl( d, key, val, 0));
}

int dict_set_owned( dict_t *d, const char *key, var_t *val){
    return( dict_set_impl( d, key, val, 1));
}

var_t *dict_get( dict_t *d, const char *key){
    size_t b;
    list_t *pos;

    if ( d == NULL || key == NULL) {
        TRACE_ERR( "d or key is NULL");
        return( NULL);
    }
    b = bucket_for_n( key, d->nbuckets);

    list_for_each( pos, &d->buckets[b]) {
        dict_entry_t *e = container_of( pos, dict_entry_t, node);
        if ( strcmp( e->key, key) == 0) {
            return( e->val);
        }
    }
    return( NULL);
}

int dict_del( dict_t *d, const char *key){
    size_t b;
    list_t *pos, *tmp;

    if ( d == NULL || key == NULL) {
        TRACE_ERR( "d or key is NULL");
        return( -1);
    }
    b = bucket_for_n( key, d->nbuckets);

    list_for_each_safe( pos, tmp, &d->buckets[b]) {
        dict_entry_t *e = container_of( pos, dict_entry_t, node);
        if ( strcmp( e->key, key) == 0) {
            list_del( pos);
            ta_free( e);
            d->count--;
            return( 0);
        }
    }
    return( -1);   /* key not found */
}

/* dict_has: searches for key existence, independent of stored value.
 * dict_get returning NULL could mean key-not-found OR key maps to NULL
 * val. */
int dict_has( dict_t *d, const char *key){
    size_t b;
    list_t *pos;

    if ( d == NULL || key == NULL) {
        TRACE_ERR( "d or key is NULL");
        return( 0);
    }
    b = bucket_for_n( key, d->nbuckets);

    list_for_each( pos, &d->buckets[b]) {
        dict_entry_t *e = container_of( pos, dict_entry_t, node);
        if ( strcmp( e->key, key) == 0) {
            return( 1);
        }
    }
    return( 0);
}

size_t dict_count( dict_t *d){
    if ( d == NULL) {
        TRACE_ERR( "d is NULL");
        return( 0);
    }
    return( d->count);
}


/* ── iteration ─────────────────────────────────────────────────────────── */

/* list_for_each_safe allows the callback to call dict_del on the current
 * key */
void dict_each( dict_t *d, dict_iter_fn fn, void *userdata){
    int i;
    list_t *pos, *tmp;

    if ( d == NULL) {
        TRACE_ERR( "d is NULL");
        return;
    }
    for ( i = 0; i < (int)d->nbuckets; i++) {
        list_for_each_safe( pos, tmp, &d->buckets[i]) {
            dict_entry_t *e = container_of( pos, dict_entry_t, node);
            fn( e->key, e->val, userdata);
        }
    }
}


/* ── display ───────────────────────────────────────────────────────────── */

void dict_print( dict_t *d){
    dict_print_depth( d, 0);
}


/* ── var_t wrapper ─────────────────────────────────────────────────────── */

var_t *var_dict_new( ta_list_t *gc){
    var_t *v = ta_malloc( gc, sizeof( var_t));
    if ( v == NULL) {
        TRACE_ERR( "ta_malloc failed for var_t");
        return( NULL);
    }
    v->type = VAR_DICT;
    v->dict = dict_new( gc);
    if ( v->dict == NULL) {
        TRACE_ERR( "dict_new failed");
        ta_free( v);
        return( NULL);
    }
    return( v);
}
