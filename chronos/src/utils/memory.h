#ifndef CHRONOS_MEMORY_H
#define CHRONOS_MEMORY_H

#include <stddef.h>
#include "../common.h"

void* chronos_malloc(size_t size);
void chronos_free(void* ptr);
void* chronos_realloc(void* ptr, size_t size);
void* chronos_calloc(size_t num, size_t size);

#endif
