#ifndef _DUMPHEX_H_
#define _DUMPHEX_H_

#include <stddef.h>

void dump_uint32( void *ptr, size_t size);
void dump_uint64( void *ptr, size_t size);
int  dumphex( void *ptr, size_t size);

#endif
