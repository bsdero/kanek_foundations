#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ta.h"
#include "var.h"
#include "dict.h"

static int passed = 0;
static int failed = 0;

#define CHECK(cond, msg) do {                                   \
    if(cond){ printf("  PASS: %s\n", msg); passed++; }         \
    else    { printf("  FAIL: %s\n", msg); failed++; }         \
} while(0)


/* ── helpers ────────────────────────────────────────────────────────────── */

static int count_iter;
static void count_cb(const char *key, var_t *val, void *ud){
    (void)key; (void)val; (void)ud;
    count_iter++;
}


/* ── test groups ────────────────────────────────────────────────────────── */

static void test_primitives(ta_list_t *gc){
    printf("\n[primitives]\n");

    var_t *vi = var_int(gc, 42);
    var_t *vf = var_float(gc, 3.14);
    var_t *vb = var_bool(gc, 1);
    var_t *vb0 = var_bool(gc, 0);
    var_t *vs = var_str(gc, "hello");
    var_t *vn = var_null(gc);

    CHECK(vi->type == VAR_INT  && vi->i == 42,               "int value");
    CHECK(vf->type == VAR_FLOAT,                              "float type");
    CHECK(vb->type == VAR_BOOL && vb->b == 1,                "bool true");
    CHECK(vb0->type == VAR_BOOL && vb0->b == 0,              "bool false");
    CHECK(vs->type == VAR_STR  && strcmp(vs->s,"hello")==0,  "str value");
    CHECK(var_is_null(vn),                                    "null check");
    CHECK(!var_is_null(vi),                                   "non-null check");
}

static void test_coercions(ta_list_t *gc){
    printf("\n[coercions]\n");

    var_t *vi  = var_int(gc, 7);
    var_t *vi0 = var_int(gc, 0);
    var_t *vf  = var_float(gc, 2.9);
    var_t *vb  = var_bool(gc, 1);
    var_t *vs  = var_str(gc, "99");
    var_t *vsf = var_str(gc, "1.5");
    var_t *vn  = var_null(gc);

    /* to int */
    CHECK(var_to_int(vi)  == 7,     "int->int");
    CHECK(var_to_int(vf)  == 2,     "float->int truncates");
    CHECK(var_to_int(vb)  == 1,     "bool->int");
    CHECK(var_to_int(vs)  == 99,    "str->int");
    CHECK(var_to_int(vn)  == 0,     "null->int is 0");

    /* to float */
    CHECK(var_to_float(vi)  == 7.0,  "int->float");
    CHECK(var_to_float(vsf) == 1.5,  "str->float");
    CHECK(var_to_float(vn)  == 0.0,  "null->float is 0.0");

    /* to bool */
    CHECK(var_to_bool(vi)  == 1,    "nonzero int is truthy");
    CHECK(var_to_bool(vi0) == 0,    "zero int is falsy");
    CHECK(var_to_bool(vb)  == 1,    "bool true");
    CHECK(var_to_bool(vs)  == 1,    "non-empty str is truthy");
    CHECK(var_to_bool(var_str(gc, "")) == 0, "empty str is falsy");
    CHECK(var_to_bool(vn)  == 0,    "null is falsy");

    /* to str */
    char *s;
    s = var_to_str(gc, vi);  CHECK(strcmp(s,"7")==0,     "int->str");
    s = var_to_str(gc, vb);  CHECK(strcmp(s,"true")==0,  "bool->str true");
    s = var_to_str(gc, var_bool(gc,0)); CHECK(strcmp(s,"false")==0, "bool->str false");
    s = var_to_str(gc, vs);  CHECK(strcmp(s,"99")==0,    "str->str");
    s = var_to_str(gc, vn);  CHECK(strcmp(s,"null")==0,  "null->str");
}

static void test_array(ta_list_t *gc){
    printf("\n[array]\n");

    var_t *arr = var_array(gc);
    CHECK(arr != NULL,          "var_array allocates");
    CHECK(var_len(arr) == 0,    "new array is empty");

    var_push(gc, arr, var_int(gc, 1));
    var_push(gc, arr, var_str(gc, "two"));
    var_push(gc, arr, var_float(gc, 3.0));
    CHECK(var_len(arr) == 3,                              "len after 3 pushes");
    CHECK(var_to_int(var_get(arr,0)) == 1,                "get index 0");
    CHECK(strcmp(var_get(arr,1)->s, "two") == 0,          "get index 1");
    CHECK(var_to_float(var_get(arr,2)) == 3.0,            "get index 2");
    CHECK(var_get(arr, 99) == NULL,                       "out-of-bounds returns NULL");
    CHECK(var_get(NULL, 0) == NULL,                       "get on NULL is safe");

    /* force realloc by pushing past initial capacity (8) */
    int i;
    for(i = 0; i < 10; i++) var_push(gc, arr, var_int(gc, i));
    CHECK(var_len(arr) == 13,    "len after growing past cap");
    CHECK(var_to_int(var_get(arr, 12)) == 9, "value after realloc");
}

static void test_dict(ta_list_t *gc){
    printf("\n[dict]\n");

    dict_t *d = dict_new(gc);
    CHECK(d != NULL,             "dict_new allocates");
    CHECK(dict_count(d) == 0,    "new dict is empty");

    dict_set(d, "name",    var_str(gc,   "kanek"));
    dict_set(d, "version", var_int(gc,   1));
    dict_set(d, "ratio",   var_float(gc, 3.14));
    dict_set(d, "ready",   var_bool(gc,  1));
    CHECK(dict_count(d) == 4,            "count after 4 sets");
    CHECK(dict_has(d, "name"),           "has existing key");
    CHECK(!dict_has(d, "missing"),       "does not have absent key");

    var_t *v = dict_get(d, "name");
    CHECK(v && strcmp(v->s, "kanek")==0, "get correct value");

    /* update existing key — count must not grow */
    dict_set(d, "version", var_int(gc, 2));
    CHECK(dict_count(d) == 4,                          "update keeps count");
    CHECK(var_to_int(dict_get(d,"version")) == 2,      "updated value");

    /* delete */
    CHECK(dict_del(d, "ready") == 0,     "del returns 0 on success");
    CHECK(dict_count(d) == 3,            "count after delete");
    CHECK(!dict_has(d, "ready"),         "deleted key gone");
    CHECK(dict_del(d, "ready") == -1,    "del missing key returns -1");

    /* iteration */
    count_iter = 0;
    dict_each(d, count_cb, NULL);
    CHECK(count_iter == 3,               "dict_each visits all entries");
}

static void test_nested(ta_list_t *gc){
    printf("\n[nested]\n");

    /* array of dicts */
    var_t *arr = var_array(gc);
    var_t *d1  = var_dict_new(gc);
    var_t *d2  = var_dict_new(gc);
    dict_set((dict_t *)d1->dict, "x", var_int(gc, 10));
    dict_set((dict_t *)d2->dict, "x", var_int(gc, 20));
    var_push(gc, arr, d1);
    var_push(gc, arr, d2);
    CHECK(var_len(arr) == 2,                              "array of dicts len");
    dict_t *inner = (dict_t *)var_get(arr,0)->dict;
    CHECK(var_to_int(dict_get(inner, "x")) == 10,         "nested dict get");

    /* dict with array value */
    dict_t *d = dict_new(gc);
    var_t *tags = var_array(gc);
    var_push(gc, tags, var_str(gc, "fs"));
    var_push(gc, tags, var_str(gc, "kfl"));
    dict_set(d, "tags", tags);
    var_t *got = dict_get(d, "tags");
    CHECK(got && got->type == VAR_ARRAY,    "array value in dict");
    CHECK(var_len(got) == 2,                "nested array length");
    CHECK(strcmp(var_get(got,0)->s,"fs")==0,"nested array element");

    /* dict inside dict */
    var_t *meta = var_dict_new(gc);
    dict_set((dict_t *)meta->dict, "author", var_str(gc, "armando"));
    dict_set(d, "meta", meta);
    var_t *got_meta = dict_get(d, "meta");
    CHECK(got_meta && got_meta->type == VAR_DICT, "dict value in dict");
    dict_t *meta_inner = (dict_t *)got_meta->dict;
    CHECK(strcmp(var_to_str(gc, dict_get(meta_inner,"author")),"armando")==0,
          "deeply nested value");
}

static void test_display(ta_list_t *gc){
    printf("\n[display]\n");

    dict_t *d = dict_new(gc);
    dict_set(d, "lib",     var_str(gc,   "kfl"));
    dict_set(d, "version", var_int(gc,   1));
    dict_set(d, "pi",      var_float(gc, 3.14159));
    dict_set(d, "ready",   var_bool(gc,  1));

    var_t *arr = var_array(gc);
    var_push(gc, arr, var_int(gc, 0));
    var_push(gc, arr, var_str(gc, "one"));
    var_push(gc, arr, var_bool(gc, 1));
    var_push(gc, arr, var_null(gc));
    dict_set(d, "items", arr);

    printf("  dict: ");
    dict_print(d);
    printf("\n  array: ");
    var_print(arr);
    printf("\n");
    /* no assertions — just verify it doesn't crash */
    CHECK(1, "display does not crash");
}


/* ── main ───────────────────────────────────────────────────────────────── */

int main(void){
    ta_list_t gc;
    ta_list_init(&gc);

    test_primitives(&gc);
    test_coercions(&gc);
    test_array(&gc);
    test_dict(&gc);
    test_nested(&gc);
    test_display(&gc);

    ta_list_destroy(&gc);

    printf("\n--- RESULTS ---\n");
    printf("PASSED: %d  FAILED: %d\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
