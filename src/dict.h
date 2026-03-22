#ifndef _DICT_H_
#define _DICT_H_

#include <stddef.h>
#include "gc.h"
#include "list.h"
#include "var.h"

#define DICT_NBUCKETS   64      /* must be a power of 2 */

typedef struct {
    list_t  node;
    char   *key;
    var_t  *val;
} dict_entry_t;

typedef struct {
    list_t      buckets[DICT_NBUCKETS];
    size_t      count;
    gc_list_t  *gc;
} dict_t;

/* lifecycle */
dict_t *dict_new  (gc_list_t *gc);

/* CRUD */
int    dict_set  (dict_t *d, const char *key, var_t *val);
var_t *dict_get  (dict_t *d, const char *key);
int    dict_del  (dict_t *d, const char *key);
int    dict_has  (dict_t *d, const char *key);
size_t dict_count(dict_t *d);

/* iteration */
typedef void (*dict_iter_fn)(const char *key, var_t *val, void *userdata);
void dict_each(dict_t *d, dict_iter_fn fn, void *userdata);

/* display */
void dict_print(dict_t *d);

/* creates a VAR_DICT var_t wrapping a new dict_t */
var_t *var_dict_new(gc_list_t *gc);

#endif
