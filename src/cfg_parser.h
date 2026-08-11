#ifndef _CFG_PARSER_H_
#define _CFG_PARSER_H_

#include "ta.h"
#include "var.h"
#include "dict.h"

/* Opaque config context.  All allocations are tracked in the GC list
 * supplied to kfl_cfg_new(); destroying that list frees everything. */
typedef struct {
    ta_list_t *gc;
    dict_t    *vars;
} kfl_cfg_t;

/* Create a new, empty config context backed by 'gc'. */
kfl_cfg_t *kfl_cfg_new  (ta_list_t *gc);

/* Parse 'filename' and merge its variables into cfg.
 * 'include' directives are resolved relative to the file being parsed.
 * Returns 0 on success, -1 on error. */
int        kfl_cfg_load (kfl_cfg_t *cfg, const char *filename);

/* Look up a variable by name.  Returns NULL if not set. */
var_t     *kfl_cfg_get  (kfl_cfg_t *cfg, const char *key);

/* Print all variables to stdout in "KEY = value" form. */
void       kfl_cfg_print( kfl_cfg_t *cfg);

#endif
