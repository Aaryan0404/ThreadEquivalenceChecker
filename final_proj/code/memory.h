#ifndef __MEMORY_H
#define __MEMORY_H

#include "rpi.h"
#include <stdbool.h>

#include "set.h"

typedef struct {
    // number of memory segments
    size_t num_ptrs;
    // list of memory ptrs to segments
    void **ptr_list;
    // list of original memory ptrs to segments
    void **ptr_og_list; 
    // list of sizes of memory segments
    size_t *size_list;
} memory_segments;

void initialize_memory_state(memory_segments* memory_state);
void reset_memory_state(memory_segments* memory_state);
uint32_t capture_memory_state(memory_segments* memory_state);
bool verify_memory_state(memory_segments* memory_state, uint64_t *valid_hashes, size_t num_valid_hashes);
void print_memstate(memory_segments* memory_state);

void print_mem(const char* msg, set_t* mem);
uint32_t hash_mem(set_t* mem);

#endif
