#ifndef _PANIC_H_
#define _PANIC_H_

#include <stdio.h>

#define PANIC_BT_DEPTH  64      /* max stack frames captured */

/* Print a stack trace of up to PANIC_BT_DEPTH frames to 'f'.
 * Only available in user-space builds (-DUSER_SPACE); a no-op otherwise. */
void stackdump(FILE *f);

/* Print msg and a full stack trace to stderr, then exit(rc).
 * Use for unrecoverable errors. */
void panic(int rc, const char *msg);

/* Convenience macro: same as panic() but prepends file:function:line context,
 * matching the style of TRACE_ERR. */
#define PANIC(rc, msg) \
    panic_at(__FILE__, __func__, __LINE__, (rc), (msg))

/* Internal — called by PANIC(), use the macro instead. */
void panic_at(const char *file, const char *func, int line,
              int rc, const char *msg);

#endif
