#include "trace.h"

/* Global trace state.
 * Default: all subsystem classes enabled, all levels emitted.
 * Override at startup:
 *   global_trace.level     = TRC_LVL_WARNING;   (suppress debug/info)
 *   global_trace.trc_class = MY_SUBSYSTEM_MASK; (filter by subsystem)
 */
trace_t global_trace = {
    .trc_class = UINT64_MAX,    /* all subsystems active */
    .level     = TRC_LVL_ALL,  /* all levels active     */
};

/* Write str to file and flush.
 * Returns 0 on success, -1 if file or str is NULL or fputs fails. */
int trace( FILE *file, const char *str){
    if ( file == NULL || str == NULL) {
        return( -1);
    }
    if ( fputs( str, file) == EOF) {
        return( -1);
    }
    fflush( file);
    return( 0);
}
