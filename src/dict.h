#ifndef _DICT_H_
#define _DICT_H_

#include <stddef.h>
#include "ta.h"
#include "list.h"
#include "var.h"

#define DICT_NBUCKETS   64      /* default bucket count for dict_new() */

typedef struct {
    list_t  node;
    char   *key;
    var_t  *val;
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

/* CRUD */
int    dict_set  (dict_t *d, const char *key, var_t *val);
var_t *dict_get  (dict_t *d, const char *key);
int    dict_del  (dict_t *d, const char *key);
int    dict_has  (dict_t *d, const char *key);
size_t dict_count( dict_t *d);

/* iteration */
typedef void (*dict_iter_fn)(const char *key, var_t *val, void *userdata);
void dict_each( dict_t *d, dict_iter_fn fn, void *userdata);

/* display */
void dict_print( dict_t *d);

/* creates a VAR_DICT var_t wrapping a new dict_t */
var_t *var_dict_new( ta_list_t *gc);

#endif
