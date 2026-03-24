/*
 * ta.c
 * Tracked allocator implementation.
 *
 * Part of the Kanek Foundation Library (KFL).
 * KANEK Storage Project.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "trace.h"
#include "list.h"
#include "dumphex.h"
#include "ta.h"
#ifdef __GNUC__
#include <sys/types.h>
#endif
#ifdef __MINGW32__
#include <inttypes.h>
#endif


/* ta_debug_display() - print one node's contents to stdout. */
static void ta_debug_display(ta_node_t *node)
{
    printf("Node=0x%lx, ptr=0x%lx\n",
           (uintptr_t)node,
           (uintptr_t)node->data);
    printf("Size=%d\n", node->size);
    printf("reservation log: '%s', \n", node->d_str);

    dumphex((void *)node, sizeof(ta_node_t) + node->size);
}


void *ta_malloc(ta_list_t *ll, size_t size)
{
    size_t total = size + sizeof(ta_node_t);

    list_t    *l    = LIST(ll);
    ta_node_t *node = malloc(total);

    if (node == NULL)
        return NULL;

    memset((void *)node, 'X', total);
    node->size = size;
    node->mark = 0;
    strcpy(node->d_str, "-------");

    list_add(LIST(node), l);

    return (void *)node->data;
}

void ta_free(void *p)
{
    ta_node_t *node = container_of(p, ta_node_t, data);
    list_del(LIST(node));
    free(node);
}


int ta_list_init(ta_list_t *ll)
{
    list_t *l = LIST(ll);

    INIT_LIST_HEAD(LIST(l));

    return 0;
}

void ta_list_destroy(ta_list_t *ll)
{
    list_t *i, *tmp;
    list_t *l = LIST(ll);

    list_for_each_safe(i, tmp, l) {
        free(i);
    }

    ta_list_init(ll);
}

/*
 * ta_list_reset() - free all allocations and reinitialise the tracked
 * allocator list without freeing the list head.
 *
 * Equivalent to ta_list_destroy() followed by ta_list_init() but
 * avoids reallocating the list head itself.
 */
void ta_list_reset(ta_list_t *ll)
{
    ta_list_destroy(ll);
    ta_list_init(ll);
}

void *ta_realloc(ta_list_t *ll, void *ptr, size_t size)
{
    ta_node_t *node = container_of(ptr, ta_node_t, data);
    ta_node_t *mem;

    /* Unlink before realloc — the old address may become invalid. */
    list_del(LIST(node));

    mem = realloc((void *)node, size + sizeof(ta_node_t));

    if (mem == NULL) {
        /* realloc failed; original block is still valid — re-link. */
        list_add(LIST(node), LIST(ll));
        return NULL;
    }

    mem->size = size;
    mem->mark = 0;

    /* Re-link the (possibly moved) node at the head of the list. */
    list_add(LIST(mem), LIST(ll));

    return (void *)mem->data;
}

size_t ta_list_total_mem(ta_list_t *ll)
{
    list_t *i, *tmp;
    list_t *l = LIST(ll);
    size_t  size = 0;

    list_for_each_safe(i, tmp, l) {
        size += ((ta_node_t *)i)->size;
    }

    return size;
}

char *ta_strdup(ta_list_t *ll, char *p)
{
    char  *pstr;
    void  *m;
    size_t string_size;

    if (!p) return NULL;
    string_size = strlen(p);

    m = ta_malloc(ll, string_size + 1);

    pstr = (char *)m;
    strncpy(pstr, p, string_size + 1);
    pstr[string_size] = '\0';

    return pstr;
}


char *ta_strncat(ta_list_t *ll, char *p, char *q)
{
    char  *pstr;
    size_t string_size = strlen(p) + strlen(q) + 1;

    pstr = (char *)ta_realloc(ll, p, string_size);
    if (!pstr) return NULL;
    strncat(pstr, q, strlen(q));
    return pstr;
}

int ta_dump_list(ta_list_t *ll)
{
    list_t *i, *tmp;
    list_t *l = LIST(ll);
    int     j = 0;

    printf("-----------GC LIST DUMP START\r\n");

    list_for_each_safe(i, tmp, l) {
        printf("Node no.-%d\r\n", j++);
        ta_debug_display((ta_list_t *)i);
    }

    printf("-----------GC LIST DUMP END\r\n");
    return 0;
}

void *ta_calloc(ta_list_t *ll, size_t nelements, size_t elementSize)
{
    size_t total = nelements * elementSize;
    void  *p     = ta_malloc(ll, total);

    if (p != NULL)
        memset(p, 0, total);
    return p;
}



void ta_mark(void *ptr)
{
    ta_node_t *node;

    node = (ta_node_t *)container_of(ptr, ta_node_t, data);
    node->mark = 1;
}

void ta_node_set_trace(void *n, char *str)
{
    ta_node_t *node;

    node = (ta_node_t *)container_of(n, ta_node_t, data);
    strncpy(node->d_str, str, MAX_DBG_STR_LEN - 1);
    node->d_str[MAX_DBG_STR_LEN - 1] = '\0';
}

void ta_sweep(ta_list_t *ll)
{
    list_t    *i, *tmp;
    list_t    *l = LIST(ll);
    ta_node_t *n;

    list_for_each_safe(i, tmp, l) {
        n = container_of(i, ta_node_t, list);
        if (n->mark == 1)
            ta_free((void *)n->data);
    }
}

char *ta_strndup(ta_list_t *ll, char *p, int n)
{
    char  *pstr;
    void  *m;
    size_t string_size = n;

    m    = ta_malloc(ll, string_size + 1);
    pstr = (char *)m;

    strncpy(pstr, p, string_size);
    pstr[string_size] = '\0';

    return pstr;
}

void *ta_memclone(ta_list_t *ll, void *p, int n)
{
    void *pm;

    pm = (void *)ta_malloc(ll, n);
    if (!pm) return NULL;
    memcpy(pm, p, n);
    return pm;
}
