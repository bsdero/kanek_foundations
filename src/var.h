#ifndef _VAR_H_
#define _VAR_H_

#include <stdint.h>
#include <stddef.h>
#include "ta.h"

typedef enum {
    VAR_NULL,
    VAR_INT,
    VAR_FLOAT,
    VAR_BOOL,
    VAR_STR,
    VAR_ARRAY,
    VAR_DICT,
} var_type_t;

typedef struct var_t {
    var_type_t type;
    union {
        int64_t   i;        /* VAR_INT   */
        double    f;        /* VAR_FLOAT */
        int       b;        /* VAR_BOOL  */
        char     *s;        /* VAR_STR   */
        struct {            /* VAR_ARRAY */
            struct var_t **items;
            size_t         len;
            size_t         cap;
        } arr;
        void     *dict;     /* VAR_DICT — dict_t *, avoids circular include */
    };
} var_t;

/* constructors */
var_t *var_int  (ta_list_t *gc, int64_t v);
var_t *var_float(ta_list_t *gc, double v);
var_t *var_bool (ta_list_t *gc, int v);
var_t *var_str  (ta_list_t *gc, const char *s);
var_t *var_null (ta_list_t *gc);
var_t *var_array(ta_list_t *gc);

/* array operations */
int    var_push(ta_list_t *gc, var_t *arr, var_t *item);
var_t *var_get (var_t *arr, size_t idx);
size_t var_len (var_t *arr);

/* type check */
int var_is_null(var_t *v);

/* coercing accessors */
int64_t var_to_int  (var_t *v);
double  var_to_float(var_t *v);
int     var_to_bool (var_t *v);
char   *var_to_str  (ta_list_t *gc, var_t *v);  /* result is TA-tracked */

/* display */
void var_print(var_t *v);

#endif
