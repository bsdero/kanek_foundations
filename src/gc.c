#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "trace.h"
#include "list.h"
#include "dumphex.h"
#include "gc.h"
#ifdef __GNUC__
#include <sys/types.h>
#endif
#ifdef __MINGW32__
#include <inttypes.h>
#endif


/* comment */
void gc_debug_display( gc_node_t *node){
    printf( "Node=0x%lx, ptr=0x%lx\n", 
            (uintptr_t) node, 
            (uintptr_t) node->data);
    printf("Size=%d\n", node->size);
    printf("reservation log: '%s', \n", node->d_str);

    dumphex( (void *) node, sizeof( gc_node_t) + node->size);
}


void *gc_malloc( gc_list_t *ll, size_t size){
    size_t total = size + sizeof( gc_node_t);

    list_t *l = LIST( ll);
    gc_node_t *node = malloc( total);

    if( node == NULL)
        return NULL;

    memset( (void *) node, 'X', total);
    node->size = size;
    node->mark = 0;
    strcpy( node->d_str, "-------");

    list_add( LIST( node), l);

    return( (void *) node->data);
}

void gc_free( void *p){
    gc_node_t *node = container_of( p, gc_node_t, data);
    list_del( LIST( node));
    free( node);
}


int gc_list_init( gc_list_t *ll){
    list_t *l = LIST( ll);


    INIT_LIST_HEAD( LIST( l));

    return( 0);
}

void gc_list_destroy( gc_list_t *ll){
    list_t *i, *tmp;
    list_t *l = LIST( ll);


    list_for_each_safe( i, tmp, l){
        free( i);
    }

    gc_list_init( ll);
}

void *gc_realloc( gc_list_t *ll, void *ptr, size_t size){
    gc_node_t *node = container_of( ptr, gc_node_t, data);
    gc_node_t *mem;

    /* Unlink before realloc — the old address may become invalid. */
    list_del( LIST( node));

    mem = realloc( (void *) node, size + sizeof( gc_node_t));

    if( mem == NULL){
        /* realloc failed; original block is still valid — re-link it. */
        list_add( LIST( node), LIST( ll));
        return NULL;
    }

    mem->size = size;
    mem->mark = 0;

    /* Re-link the (possibly moved) node at the head of the list. */
    list_add( LIST( mem), LIST( ll));

    return (void *) mem->data;
}

size_t gc_list_total_mem( gc_list_t *ll){
    list_t *i, *tmp;
    list_t *l = LIST( ll);

    size_t size = 0;

    list_for_each_safe( i, tmp, l){
        size += ((gc_node_t *)i)->size;
    }

    return size;
}

char *gc_strdup( gc_list_t *ll, char *p){
    char *pstr;
    void *m;
    size_t string_size;

    if(!p) return NULL;
    string_size = strlen( p);

    m = gc_malloc( ll, string_size + 1);

    pstr = ( char *) m;
    strncpy( pstr, p, string_size+1);
    pstr[string_size] = '\0';

    return( pstr);
}


char *gc_strncat( gc_list_t *ll, char *p, char *q){
    char *pstr;

    size_t string_size = strlen( p) + strlen(q) + 1;

    pstr = (char *) gc_realloc( ll, p, string_size );
    if(!pstr) return NULL;
    strncat( pstr, q, strlen(q));
    return( pstr);
}

int gc_dump_list( gc_list_t *ll){
    list_t *i, *tmp;
    list_t *l = LIST( ll);
    int j = 0;

    printf("-----------GC LIST DUMP START\r\n");

    list_for_each_safe( i, tmp, l){
        printf("Node no.-%d\r\n", j++);
        gc_debug_display( (gc_list_t *)i);
    }

    printf("-----------GC LIST DUMP END\r\n");
    return 0;
}

void *gc_calloc( gc_list_t *ll, size_t nelements, size_t elementSize){
    size_t total = nelements * elementSize;
    void *p = gc_malloc( ll, total);
    if( p != NULL){
        memset( p, 0, total);
    }
    return p;
}



void gc_mark( void *ptr){
    gc_node_t *node;

    node = (gc_node_t *) container_of( ptr, gc_node_t, data);
    node->mark = 1;
}

void gc_node_set_trace( void *n, char *str){
    gc_node_t *node;

    node = (gc_node_t *) container_of( n, gc_node_t, data);
    strncpy( node->d_str, str, MAX_DBG_STR_LEN - 1);
    node->d_str[MAX_DBG_STR_LEN - 1] = '\0';
}

void gc_sweep(gc_list_t *ll){
    list_t *i, *tmp;
    list_t *l = LIST( ll);
    gc_node_t *n;

    list_for_each_safe( i, tmp, l){
        n = container_of( i, gc_node_t, list);
        if( n->mark == 1){
            gc_free( (void *) n->data);
        }
    }
}

char *gc_strndup( gc_list_t *ll, char *p, int n){
    char *pstr;
    void *m;

    size_t string_size = n;

    m = gc_malloc( ll, string_size + 1);
    pstr = (char *) m;    

    strncpy( pstr, p, string_size);
    pstr[string_size] = '\0';

    return( pstr);
}

void *gc_memclone( gc_list_t *ll, void *p, int n){
    void *pm;

    pm = (void *) gc_malloc( ll, n);
    if(!pm) return NULL;
    memcpy( pm, p, n);
    return( pm);
}

