/* File: allocator.h
 * -----------------
 * Interface file for the custom heap allocator.
 */
#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H

#include <stddef.h>  // for size_t

// Alignment requirement for all blocks
#define ALIGNMENT 8

// maximum size of block that must be accommodated
#define MAX_REQUEST_SIZE (1 << 30)

uint32_t equiv_malloc_init(void *heap_start, size_t heap_size);
void* equiv_malloc(size_t requested_size);
void* equiv_realloc(void *old_ptr, size_t new_size);
void equiv_free(void *ptr);
void equiv_dump_heap();

#endif
