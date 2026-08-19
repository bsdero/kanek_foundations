#ifndef _DICT_H_
#define _DICT_H_

#include <stddef.h>
#include "ta.h"
#include "list.h"
#include "var.h"

#define DICT_NBUCKETS   64      /* default bucket count for dict_new() */

typedef struct {
    list_t  node;
    var_t  *val;
    char    key[];   /* flexible array member — MUST stay last field */
} dict_entry_t;

typedef struct {
    list_t     *buckets;   /* pointer to bucket array (TA-tracked) */
    uint32_t    nbuckets;  /* number of buckets; must be a power of 2 */
    size_t      count;
    ta_list_t  *gc;
} dict_t;

/* lifecycle */
dict_t *dict_new  (ta_list_t *gc);

/*
 * dict_new_sized() - create a dictionary with a specific bucket count.
 *
 * bucket_count must be a power of 2 and >= 4.
 * Use dict_new() for the default 64-bucket table.
 * Use dict_new_sized() when the expected entry count is known: a load
 * factor below 0.7 gives good performance, so
 *   bucket_count >= ceil(expected_entries / 0.7).
 *
 * Returns the new dict_t, or NULL on allocation failure or invalid
 * bucket_count.
 */
dict_t *dict_new_sized( ta_list_t *gc, uint32_t bucket_count);

/*
 * dict_rehash() - resize d's bucket table to new_nbuckets and
 * re-distribute all existing entries into it.
 *
 * new_nbuckets must be a power of 2 and >= 4 (same constraint as
 * dict_new_sized()). On success, d->buckets and d->nbuckets are
 * updated in place and the old bucket array is freed. On failure
 * (invalid new_nbuckets, or allocation failure), d is left completely
 * unmodified and -1 is returned — it is always safe to keep using d
 * with its old table after a failed rehash.
 *
 * You normally don't need to call this directly — dict_set() and
 * dict_set_owned() call it automatically when the load factor crosses
 * ~0.75. It's exposed for callers who want to pre-size a dict at a
 * specific growth point.
 */
int dict_rehash( dict_t *d, uint32_t new_nbuckets);

/* CRUD */

/*
 * dict_set() - set d[key] = val, without taking ownership of val.
 *
 * If key already exists, its value pointer is replaced with val — the
 * previous value is NOT freed; it stays alive (tracked by d->gc) until
 * the whole gc context is destroyed, or until you free it yourself.
 * This is the safe default: it never frees a pointer you might still
 * be holding a reference to elsewhere (another container, a local
 * variable, a dict_each() callback, etc).
 *
 * Returns 0 on success, -1 on error (d or key is NULL, or allocation
 * failure for a new entry).
 */
int    dict_set  (dict_t *d, const char *key, var_t *val);

/*
 * dict_set_owned() - like dict_set(), but if key already exists, its
 * previous value IS freed via ta_free() before being replaced.
 *
 * Only use this at a call site where you can be certain the value
 * being replaced is not referenced anywhere else (not stored under
 * another key, not pushed into an array, not held by a caller that
 * expects to keep using it). If that's not certain, use dict_set()
 * instead and let the old value live until the gc context is torn
 * down — a bounded, harmless cost — rather than risk a
 * use-after-free.
 *
 * Returns 0 on success, -1 on error (same conditions as dict_set()).
 */
int    dict_set_owned( dict_t *d, const char *key, var_t *val);

var_t *dict_get  (dict_t *d, const char *key);
int    dict_del  (dict_t *d, const char *key);
int    dict_has  (dict_t *d, const char *key);
size_t dict_count( dict_t *d);

/* iteration */
typedef void (*dict_iter_fn)(const char *key, var_t *val, void *userdata);
void dict_each( dict_t *d, dict_iter_fn fn, void *userdata);

/* display */
void dict_print( dict_t *d);

/* internal: shared with var.c's var_print_depth() recursion so
 * dict-printing has exactly one implementation. Not part of the
 * stable per-entry API — use dict_print() or var_print() instead. */
void dict_print_depth( dict_t *d, int depth);

/* creates a VAR_DICT var_t wrapping a new dict_t */
var_t *var_dict_new( ta_list_t *gc);

#endif
