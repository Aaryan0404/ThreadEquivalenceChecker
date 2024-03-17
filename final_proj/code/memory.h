#include "rpi.h"
#include "sha256.h"

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
void sha256_update_state(SHA256_CTX *ctx, memory_segments* memory_state);