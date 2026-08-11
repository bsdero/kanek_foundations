#include <stdio.h>
#include <stdlib.h>
#include "panic.h"

#ifdef USER_SPACE
#include <execinfo.h>
#endif


/* ── internal backtrace printer ────────────────────────────────────────── */

static void print_backtrace( FILE *f){
#ifdef USER_SPACE
    void  *bt[PANIC_BT_DEPTH];
    char **symbols;
    int    n, i;

    n = backtrace( bt, PANIC_BT_DEPTH);
    symbols = backtrace_symbols( bt, n);

    fprintf( f, "Stack trace (%d frames):\n", n);

    if ( symbols != NULL) {
        for ( i = 0; i < n; i++) {
            fprintf( f, "  [%2d] %s\n", i, symbols[i]);
        }
        free( symbols);   /* backtrace_symbols result is malloc'd, not GC'd */
    } else {
        /* fallback: at least print raw addresses */
        for ( i = 0; i < n; i++) {
            fprintf( f, "  [%2d] %p\n", i, bt[i]);
        }
    }
#else
    fprintf( f, "  (stack trace not available in kernel-space builds)\n");
#endif
}


/* ── public API ────────────────────────────────────────────────────────── */

void stackdump( FILE *f){
    if ( f == NULL) {
        f = stderr;
    }
    print_backtrace( f);
    fflush( f);
}

void panic( int rc, const char *msg){
    fprintf( stderr, "\n*** PANIC (rc=%d): %s\n",
            rc, (msg != NULL) ? msg : "(no message)");
    print_backtrace( stderr);
    fflush( stderr);
    exit( rc);
}

void panic_at( const char *file, const char *func, int line,
              int rc, const char *msg){
    fprintf( stderr, "\n*** PANIC %s:%s:%d (rc=%d): %s\n",
            file, func, line, rc, (msg != NULL) ? msg : "(no message)");
    print_backtrace( stderr);
    fflush( stderr);
    exit( rc);
}
