#ifndef _TA_H_
#define _TA_H_

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include "list.h"


/*
 * container_of - cast a member of a structure out to the containing
 * structure.
 *
 * @ptr:    the pointer to the member.
 * @type:   the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 */
#ifndef container_of
#define container_of(ptr, type, member) ({                      \
    const typeof( ((type *)0)->member) *__mptr = (ptr);        \
    (type *)( (char *)__mptr - offsetof( type,member) );})
#endif

/*
 * ta_node_t - tracked allocator node.
 * Every allocation managed by the tracked allocator is wrapped in one
 * of these.  The list field links it into the ta_list_t context.
 * size  - user-data size in bytes.
 * mark  - set to 1 by ta_mark(); cleared nodes are freed by ta_sweep().
 * d_str - short debug label set by ta_node_set_trace().
 * data  - flexible array member; user data starts here.
 */
typedef struct {
    list_t   list;
    uint32_t size;
    uint32_t mark;
/* Maximum length of the per-node debug label string (incl. NUL). */
#define MAX_DBG_STR_LEN 48
    char d_str[MAX_DBG_STR_LEN];
    char data[];
} ta_node_t;

/*
 * ta_list_t - tracked allocator context.
 * Alias for ta_node_t; the head of the list acts as a sentinel node.
 * Initialise with ta_list_init(), destroy with ta_list_destroy().
 */
typedef ta_node_t ta_list_t;

/* Lifecycle */
int     ta_list_init( ta_list_t *ll);
void    ta_list_destroy( ta_list_t *ll);

/*
 * ta_list_reset() - free all allocations and reinitialise the tracked
 * allocator list without freeing the list head.
 *
 * Equivalent to ta_list_destroy() followed by ta_list_init() but
 * avoids reallocating the list head itself.
 *
 * Use for allocator contexts that are reused across many repeated
 * operations (e.g. processing one KV lookup in a tight loop) to avoid
 * per-operation list head alloc overhead.
 */
void    ta_list_reset( ta_list_t *ll);

/*
 * ta_list_total_mem() - return total tracked bytes.
 * Returns the sum of user-data sizes for all live nodes in ll.
 */
size_t  ta_list_total_mem( ta_list_t *ll);

/*
 * ta_dump_list() - hex-dump every node in the list to stdout.
 * Returns 0.
 */
int     ta_dump_list( ta_list_t *ll);

/* Allocation */
void   *ta_malloc( ta_list_t *ll, size_t size);
void    ta_free( void *p);
void   *ta_realloc( ta_list_t *ll, void *ptr, size_t size);
void   *ta_calloc( ta_list_t *ll, size_t nelements, size_t elementSize);
char   *ta_strdup( ta_list_t *ll, char *p);
char   *ta_strndup( ta_list_t *ll, char *p, int n);
char   *ta_strncat( ta_list_t *ll, char *p, char *q);
void   *ta_memclone( ta_list_t *ll, void *p, int n);

/* Mark and sweep */
void    ta_mark( void *ptr);
void    ta_sweep( ta_list_t *ll);

/* Diagnostics */
void    ta_node_set_trace( void *n, char *str);


/*
 * Backward-compatibility aliases: gc_* → ta_*
 * These are deprecated; prefer the ta_ names in new code.
 */
typedef ta_node_t  gc_node_t;  /* deprecated: use ta_node_t  */
typedef ta_list_t  gc_list_t;  /* deprecated: use ta_list_t  */

#define gc_list_init(ll)          ta_list_init( ll)
#define gc_list_destroy(ll)       ta_list_destroy( ll)
#define gc_list_total_mem(ll)     ta_list_total_mem( ll)
#define gc_dump_list(ll)          ta_dump_list( ll)
#define gc_malloc(ll, sz)         ta_malloc( (ll), (sz))
#define gc_free(p)                ta_free( p)
#define gc_realloc(ll, ptr, sz)   ta_realloc( (ll), (ptr), (sz))
#define gc_calloc(ll, n, esz)     ta_calloc( (ll), (n), (esz))
#define gc_strdup(ll, p)          ta_strdup( (ll), (p))
#define gc_strndup(ll, p, n)      ta_strndup( (ll), (p), (n))
#define gc_strncat(ll, p, q)      ta_strncat( (ll), (p), (q))
#define gc_memclone(ll, p, n)     ta_memclone( (ll), (p), (n))
#define gc_mark(ptr)              ta_mark( ptr)
#define gc_sweep(ll)              ta_sweep( ll)
#define gc_node_set_trace(n, s)   ta_node_set_trace( (n), (s))


#endif
