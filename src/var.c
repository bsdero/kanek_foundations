#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "var.h"
#include "dict.h"   /* needed for VAR_DICT case in var_print */

#define ARRAY_INIT_CAP  8
#define PRINT_MAX_DEPTH 32


/* ── constructors ──────────────────────────────────────────────────────── */

var_t *var_int(gc_list_t *gc, int64_t v){
    var_t *var = gc_malloc(gc, sizeof(var_t));
    if(!var) return NULL;
    var->type = VAR_INT;
    var->i = v;
    return var;
}

var_t *var_float(gc_list_t *gc, double v){
    var_t *var = gc_malloc(gc, sizeof(var_t));
    if(!var) return NULL;
    var->type = VAR_FLOAT;
    var->f = v;
    return var;
}

var_t *var_bool(gc_list_t *gc, int v){
    var_t *var = gc_malloc(gc, sizeof(var_t));
    if(!var) return NULL;
    var->type = VAR_BOOL;
    var->b = (v != 0);
    return var;
}

var_t *var_str(gc_list_t *gc, const char *s){
    var_t *var = gc_malloc(gc, sizeof(var_t));
    if(!var) return NULL;
    var->type = VAR_STR;
    var->s = s ? gc_strdup(gc, (char *)s) : NULL;
    return var;
}

var_t *var_null(gc_list_t *gc){
    var_t *var = gc_malloc(gc, sizeof(var_t));
    if(!var) return NULL;
    var->type = VAR_NULL;
    return var;
}

var_t *var_array(gc_list_t *gc){
    var_t *var = gc_malloc(gc, sizeof(var_t));
    if(!var) return NULL;
    var->type = VAR_ARRAY;
    var->arr.items = gc_malloc(gc, ARRAY_INIT_CAP * sizeof(var_t *));
    if(!var->arr.items){ gc_free(var); return NULL; }
    var->arr.len = 0;
    var->arr.cap = ARRAY_INIT_CAP;
    return var;
}


/* ── array operations ──────────────────────────────────────────────────── */

int var_push(gc_list_t *gc, var_t *arr, var_t *item){
    if(!arr || arr->type != VAR_ARRAY) return -1;

    if(arr->arr.len == arr->arr.cap){
        size_t new_cap = arr->arr.cap * 2;
        var_t **grown = gc_realloc(gc, arr->arr.items,
                                   new_cap * sizeof(var_t *));
        if(!grown) return -1;
        arr->arr.items = grown;
        arr->arr.cap   = new_cap;
    }

    arr->arr.items[arr->arr.len++] = item;
    return 0;
}

var_t *var_get(var_t *arr, size_t idx){
    if(!arr || arr->type != VAR_ARRAY) return NULL;
    if(idx >= arr->arr.len) return NULL;
    return arr->arr.items[idx];
}

size_t var_len(var_t *arr){
    if(!arr || arr->type != VAR_ARRAY) return 0;
    return arr->arr.len;
}


/* ── type check ────────────────────────────────────────────────────────── */

int var_is_null(var_t *v){
    return (!v || v->type == VAR_NULL);
}


/* ── coercing accessors ────────────────────────────────────────────────── */

int64_t var_to_int(var_t *v){
    if(!v) return 0;
    switch(v->type){
        case VAR_INT:   return v->i;
        case VAR_FLOAT: return (int64_t)v->f;
        case VAR_BOOL:  return (int64_t)v->b;
        case VAR_STR:   return v->s ? strtoll(v->s, NULL, 10) : 0;
        default:        return 0;
    }
}

double var_to_float(var_t *v){
    if(!v) return 0.0;
    switch(v->type){
        case VAR_INT:   return (double)v->i;
        case VAR_FLOAT: return v->f;
        case VAR_BOOL:  return (double)v->b;
        case VAR_STR:   return v->s ? strtod(v->s, NULL) : 0.0;
        default:        return 0.0;
    }
}

int var_to_bool(var_t *v){
    if(!v) return 0;
    switch(v->type){
        case VAR_INT:   return v->i != 0;
        case VAR_FLOAT: return v->f != 0.0;
        case VAR_BOOL:  return v->b;
        case VAR_STR:   return v->s && v->s[0] != '\0';
        case VAR_NULL:  return 0;
        default:        return 1;   /* ARRAY, DICT — truthy if present */
    }
}

char *var_to_str(gc_list_t *gc, var_t *v){
    char buf[64];
    if(!v) return gc_strdup(gc, "null");
    switch(v->type){
        case VAR_INT:
            snprintf(buf, sizeof(buf), "%" PRId64, v->i);
            return gc_strdup(gc, buf);
        case VAR_FLOAT:
            snprintf(buf, sizeof(buf), "%g", v->f);
            return gc_strdup(gc, buf);
        case VAR_BOOL:
            return gc_strdup(gc, v->b ? "true" : "false");
        case VAR_STR:
            return gc_strdup(gc, v->s ? v->s : "null");
        case VAR_NULL:
            return gc_strdup(gc, "null");
        case VAR_ARRAY:
            return gc_strdup(gc, "[array]");
        case VAR_DICT:
            return gc_strdup(gc, "{dict}");
        default:
            return gc_strdup(gc, "null");
    }
}


/* ── display (depth-limited to prevent infinite recursion) ─────────────── */

static void dict_print_depth(dict_t *d, int depth);

static void var_print_depth(var_t *v, int depth){
    size_t i;
    if(depth > PRINT_MAX_DEPTH){ printf("..."); return; }
    if(!v){ printf("null"); return; }
    switch(v->type){
        case VAR_NULL:
            printf("null");
            break;
        case VAR_INT:
            printf("%" PRId64, v->i);
            break;
        case VAR_FLOAT:
            printf("%g", v->f);
            break;
        case VAR_BOOL:
            printf("%s", v->b ? "true" : "false");
            break;
        case VAR_STR:
            printf("\"%s\"", v->s ? v->s : "null");
            break;
        case VAR_ARRAY:
            printf("[");
            for(i = 0; i < v->arr.len; i++){
                var_print_depth(v->arr.items[i], depth + 1);
                if(i < v->arr.len - 1) printf(", ");
            }
            printf("]");
            break;
        case VAR_DICT:
            dict_print_depth((dict_t *)v->dict, depth + 1);
            break;
    }
}

static void dict_print_depth(dict_t *d, int depth){
    int i, first = 1;
    list_t *pos;

    if(!d){ printf("null"); return; }
    if(depth > PRINT_MAX_DEPTH){ printf("..."); return; }

    printf("{");
    for(i = 0; i < DICT_NBUCKETS; i++){
        list_for_each(pos, &d->buckets[i]){
            dict_entry_t *e = container_of(pos, dict_entry_t, node);
            if(!first) printf(", ");
            printf("\"%s\": ", e->key);
            var_print_depth(e->val, depth + 1);
            first = 0;
        }
    }
    printf("}");
}

void var_print(var_t *v){
    var_print_depth(v, 0);
}
