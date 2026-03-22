#ifndef _TRACE_H_
#define _TRACE_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

/* ── levels ──────────────────────────────────────────────────────────── */

#define TRC_LVL_ALL              0
#define TRC_LVL_DEBUG            1
#define TRC_LVL_INFO             2
#define TRC_LVL_NOTICE           3
#define TRC_LVL_WARNING          4
#define TRC_LVL_ERROR            5
#define TRC_LVL_CRITICAL         6
#define TRC_LVL_ALERT            7
#define TRC_LVL_EMERGENCY        8

/* Set at build time (-DTRACE_MIN_LEVEL=TRC_LVL_INFO) to eliminate all
 * trace calls below the given level at compile time.  Defaults to ALL
 * so nothing is stripped unless the user asks for it. */
#ifndef TRACE_MIN_LEVEL
#define TRACE_MIN_LEVEL          TRC_LVL_ALL
#endif


/* ── trace state ─────────────────────────────────────────────────────── */

typedef struct {
    uint64_t trc_class;   /* bitmask — which subsystems are active   */
    uint16_t level;       /* minimum level to emit at runtime        */
} trace_t;

/* Defined once in trace.c; configure at startup to filter output. */
extern trace_t global_trace;


/* ── unconditional macros ────────────────────────────────────────────── */

/* TRACE_DBG — debug info to stdout */
#if TRC_LVL_DEBUG >= TRACE_MIN_LEVEL
#define TRACE_DBG(fmt,...) do {                                              \
    fprintf(stdout, "%s:%s:%d: " fmt "\n",                                  \
            __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);               \
    fflush(stdout);                                                          \
} while(0)
#else
#define TRACE_DBG(fmt,...) do { } while(0)
#endif

/* TRACE_ERR — error message to stderr */
#if TRC_LVL_ERROR >= TRACE_MIN_LEVEL
#define TRACE_ERR(fmt,...) do {                                              \
    fprintf(stderr, "ERROR:%s:%s:%d: " fmt "\n",                            \
            __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);               \
    fflush(stderr);                                                          \
} while(0)
#else
#define TRACE_ERR(fmt,...) do { } while(0)
#endif

/* TRACE_SYSERR — error + errno + strerror to stderr */
#if TRC_LVL_ERROR >= TRACE_MIN_LEVEL
#define TRACE_SYSERR(fmt,...) do {                                           \
    fprintf(stderr, "SYSERR:%d:%s:%s:%s:%d: " fmt "\n",                     \
            errno, strerror(errno),                                          \
            __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);               \
    fflush(stderr);                                                          \
} while(0)
#else
#define TRACE_SYSERR(fmt,...) do { } while(0)
#endif

/* TRACE_ERRNO — compact errno report to stderr */
#if TRC_LVL_ERROR >= TRACE_MIN_LEVEL
#define TRACE_ERRNO(fmt,...) do {                                            \
    fprintf(stderr, "ERRNO: %d:%s: " fmt "\n",                              \
            errno, strerror(errno), ##__VA_ARGS__);                         \
    fflush(stderr);                                                          \
} while(0)
#else
#define TRACE_ERRNO(fmt,...) do { } while(0)
#endif


/* ── format into a caller-supplied buffer ────────────────────────────── */

/* TRACE_STR — format file:func:line + message into str[size].
 * Uses snprintf so the result is always null-terminated. */
#define TRACE_STR(str, size, fmt, ...) do {                                  \
    snprintf((str), (size), "%s:%s:%d: " fmt "\n",                          \
             __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);              \
} while(0)


/* ── runtime-filtered macro ──────────────────────────────────────────── */

/* TRACE — emit only when (level >= global_trace.level) AND
 *         (trc_cls & global_trace.trc_class) != 0.
 * Formats into str[size] first, then writes to file via trace().
 *
 * Parameters:
 *   file    — FILE * to write to
 *   trc_cls — subsystem class bitmask for this call site
 *   level   — TRC_LVL_* severity of this message
 *   str     — char buffer to format into
 *   size    — sizeof(str)
 *   fmt,... — printf-style format and arguments
 */
#define TRACE(file, trc_cls, level, str, size, fmt, ...) do {               \
    if ((level) >= global_trace.level &&                                     \
        ((trc_cls) & global_trace.trc_class) != 0) {                        \
        TRACE_STR((str), (size), fmt, ##__VA_ARGS__);                       \
        trace((file), (str));                                                \
    }                                                                        \
} while(0)


/* Write str to file and flush. Returns 0 on success, -1 on error. */
int trace(FILE *file, const char *str);

#endif
