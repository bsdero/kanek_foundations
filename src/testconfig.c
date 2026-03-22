#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gc.h"
#include "var.h"
#include "kfl_config.h"

static int pass = 0, fail = 0;

#define CHECK(cond, desc) do {                          \
    if(cond){ printf("  PASS: %s\n", desc); pass++; }  \
    else     { printf("  FAIL: %s\n", desc); fail++; } \
} while(0)

#define CHECK_STR(v, expected, desc) do {                               \
    const char *_s = (v) ? var_to_str(&gc, (v)) : NULL;                \
    CHECK(_s && strcmp(_s, (expected)) == 0, desc);                     \
} while(0)

#define CHECK_INT(v, expected, desc) \
    CHECK((v) && var_to_int(v) == (int64_t)(expected), desc)

#define CHECK_BOOL(v, expected, desc) \
    CHECK((v) && var_to_bool(v) == (expected), desc)

static gc_list_t gc;

int main(void){
    var_t *v;

    gc_list_init(&gc);

    kfl_cfg_t *cfg = kfl_cfg_new(&gc);
    if(!cfg){
        printf("FATAL: kfl_cfg_new() returned NULL\n");
        return 1;
    }

    printf("=== Loading testdata/test.cfg ===\n");
    printf("--- display output during load ---\n");
    if(kfl_cfg_load(cfg, "testdata/test.cfg") != 0){
        printf("FATAL: kfl_cfg_load() failed\n");
        return 1;
    }
    printf("--- end display output ---\n\n");

    printf("=== Basic types ===\n");
    v = kfl_cfg_get(cfg, "MYINT");
    CHECK_INT(v, 42, "MYINT == 42");

    v = kfl_cfg_get(cfg, "MYFLOAT");
    CHECK(v && fabs(var_to_float(v) - 3.14) < 1e-9, "MYFLOAT ~= 3.14");

    v = kfl_cfg_get(cfg, "MYBOOL_T");
    CHECK_BOOL(v, 1, "MYBOOL_T == true");

    v = kfl_cfg_get(cfg, "MYBOOL_F");
    CHECK_BOOL(v, 0, "MYBOOL_F == false");

    v = kfl_cfg_get(cfg, "MYSTRING");
    CHECK_STR(v, "hello world ", "MYSTRING == 'hello world '");

    v = kfl_cfg_get(cfg, "MYVERSION");
    CHECK_STR(v, "1", "MYVERSION (inline comment stripped)");

    printf("\n=== String concatenation ===\n");
    v = kfl_cfg_get(cfg, "MYCAT");
    CHECK_STR(v, "hello world foo", "MYCAT == MYSTRING + 'foo'");

    v = kfl_cfg_get(cfg, "MYCAT2");
    CHECK_STR(v, "abcdefghi", "MYCAT2 == 'abc'+'def'+'ghi'");

    v = kfl_cfg_get(cfg, "MYCAT3");
    CHECK_STR(v, "v42", "MYCAT3 == 'v' + MYINT (number coercion)");

    printf("\n=== Array literals ===\n");
    v = kfl_cfg_get(cfg, "MYARRAY");
    CHECK(v && v->type == VAR_ARRAY, "MYARRAY type == VAR_ARRAY");
    CHECK(v && var_len(v) == 6, "MYARRAY length == 6");

    printf("\n=== Array indexing ===\n");
    v = kfl_cfg_get(cfg, "ARRVAL0");
    CHECK_INT(v, 0, "ARRVAL0 == 0");

    v = kfl_cfg_get(cfg, "ARRVAL1");
    CHECK_INT(v, 1, "ARRVAL1 == 1");

    v = kfl_cfg_get(cfg, "ARRVAL4");
    CHECK_STR(v, "four", "ARRVAL4 == 'four'");

    printf("\n=== Variable reference ===\n");
    v = kfl_cfg_get(cfg, "MYINT2");
    CHECK_INT(v, 42, "MYINT2 = MYINT (reference to existing var)");

    printf("\n=== += operator ===\n");
    v = kfl_cfg_get(cfg, "MYAPPEND");
    CHECK_STR(v, "hello world", "MYAPPEND after += ' world'");

    printf("\n=== include directive ===\n");
    v = kfl_cfg_get(cfg, "INCLUDED_VAR");
    CHECK_STR(v, "from_included", "INCLUDED_VAR loaded via include");

    v = kfl_cfg_get(cfg, "INCLUDED_NUM");
    CHECK_INT(v, 99, "INCLUDED_NUM == 99");

    v = kfl_cfg_get(cfg, "INCLUDED_FLOAT");
    CHECK(v && fabs(var_to_float(v) - 2.71828) < 1e-4, "INCLUDED_FLOAT ~= 2.71828");

    v = kfl_cfg_get(cfg, "INCLUDED_BOOL");
    CHECK_BOOL(v, 1, "INCLUDED_BOOL == true");

    v = kfl_cfg_get(cfg, "INCLUDED_CAT");
    CHECK_STR(v, "int_is_42", "INCLUDED_CAT uses parent var MYINT");

    printf("\n=== Cross-file concatenation ===\n");
    v = kfl_cfg_get(cfg, "COMBINED");
    CHECK_STR(v, "from_included and hello world bar",
              "COMBINED uses INCLUDED_VAR + MYSTRING");

    printf("\n=== Missing key ===\n");
    v = kfl_cfg_get(cfg, "DOES_NOT_EXIST");
    CHECK(v == NULL, "missing key returns NULL");

    printf("\n=== kfl_cfg_print ===\n");
    kfl_cfg_print(cfg);

    gc_list_destroy(&gc);

    printf("\n--- RESULTS ---\n");
    printf("PASSED: %d  FAILED: %d\n", pass, fail);
    return fail ? 1 : 0;
}
