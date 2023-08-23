#ifndef __LD_MALLOC
#define __LD_MALLOC

#include <stdint.h>
#include <stddef.h>

void ldmalloc_init();

void* ldmalloc(size_t size);
void ldfree(void* ptr);

void* ldcalloc(size_t n, size_t elem);

#endif /* __LD_MALLOC */
