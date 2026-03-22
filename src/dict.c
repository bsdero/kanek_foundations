#include <stdio.h>
#include <string.h>
#include "dict.h"
#include "hash.h"


/* ── internal helpers ──────────────────────────────────────────────────── */

static size_t bucket_for(const char *key){
    uint32_t h = xxh32(key, strlen(key), 0);
    return (size_t)(h & (DICT_NBUCKETS - 1));
}


/* ── lifecycle ─────────────────────────────────────────────────────────── */

dict_t *dict_new(gc_list_t *gc){
    int i;
    dict_t *d = gc_malloc(gc, sizeof(dict_t));
    if(!d) return NULL;
    d->gc    = gc;
    d->count = 0;
    for(i = 0; i < DICT_NBUCKETS; i++){
        INIT_LIST_HEAD(&d->buckets[i]);
    }
    return d;
}


/* ── CRUD ──────────────────────────────────────────────────────────────── */

int dict_set(dict_t *d, const char *key, var_t *val){
    size_t b;
    list_t *pos;
    dict_entry_t *e;

    if(!d || !key) return -1;
    b = bucket_for(key);

    /* update in place if key already exists */
    list_for_each(pos, &d->buckets[b]){
        e = container_of(pos, dict_entry_t, node);
        if(strcmp(e->key, key) == 0){
            e->val = val;
            return 0;
        }
    }

    /* new entry */
    e = gc_malloc(d->gc, sizeof(dict_entry_t));
    if(!e) return -1;
    e->key = gc_strdup(d->gc, (char *)key);
    if(!e->key){ gc_free(e); return -1; }
    e->val = val;
    list_add_tail(&e->node, &d->buckets[b]);
    d->count++;
    return 0;
}

var_t *dict_get(dict_t *d, const char *key){
    size_t b;
    list_t *pos;

    if(!d || !key) return NULL;
    b = bucket_for(key);

    list_for_each(pos, &d->buckets[b]){
        dict_entry_t *e = container_of(pos, dict_entry_t, node);
        if(strcmp(e->key, key) == 0) return e->val;
    }
    return NULL;
}

int dict_del(dict_t *d, const char *key){
    size_t b;
    list_t *pos, *tmp;

    if(!d || !key) return -1;
    b = bucket_for(key);

    list_for_each_safe(pos, tmp, &d->buckets[b]){
        dict_entry_t *e = container_of(pos, dict_entry_t, node);
        if(strcmp(e->key, key) == 0){
            list_del(pos);
            gc_free(e->key);
            gc_free(e);
            d->count--;
            return 0;
        }
    }
    return -1;   /* key not found */
}

/* dict_has: searches for key existence, independent of stored value.
 * dict_get returning NULL could mean key-not-found OR key maps to NULL val. */
int dict_has(dict_t *d, const char *key){
    size_t b;
    list_t *pos;

    if(!d || !key) return 0;
    b = bucket_for(key);

    list_for_each(pos, &d->buckets[b]){
        dict_entry_t *e = container_of(pos, dict_entry_t, node);
        if(strcmp(e->key, key) == 0) return 1;
    }
    return 0;
}

size_t dict_count(dict_t *d){
    if(!d) return 0;
    return d->count;
}


/* ── iteration ─────────────────────────────────────────────────────────── */

/* list_for_each_safe allows the callback to call dict_del on the current key */
void dict_each(dict_t *d, dict_iter_fn fn, void *userdata){
    int i;
    list_t *pos, *tmp;

    if(!d) return;
    for(i = 0; i < DICT_NBUCKETS; i++){
        list_for_each_safe(pos, tmp, &d->buckets[i]){
            dict_entry_t *e = container_of(pos, dict_entry_t, node);
            fn(e->key, e->val, userdata);
        }
    }
}


/* ── display ───────────────────────────────────────────────────────────── */

void dict_print(dict_t *d){
    /* depth-limited printing is handled via var_print → dict_print_depth
     * in var.c; this entry point just delegates to the depth-0 path by
     * printing directly (single-level display, no recursion guard needed
     * since var_print already guards sub-values). */
    int i;
    list_t *pos;
    int first = 1;

    if(!d){ printf("null"); return; }

    printf("{");
    for(i = 0; i < DICT_NBUCKETS; i++){
        list_for_each(pos, &d->buckets[i]){
            dict_entry_t *e = container_of(pos, dict_entry_t, node);
            if(!first) printf(", ");
            printf("\"%s\": ", e->key);
            var_print(e->val);
            first = 0;
        }
    }
    printf("}");
}


/* ── var_t wrapper ─────────────────────────────────────────────────────── */

var_t *var_dict_new(gc_list_t *gc){
    var_t *v = gc_malloc(gc, sizeof(var_t));
    if(!v) return NULL;
    v->type = VAR_DICT;
    v->dict = dict_new(gc);
    if(!v->dict){ gc_free(v); return NULL; }
    return v;
}
