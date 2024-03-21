#include "memory.h"

// initialize ptr_og_list in memory state with a copy of the values in ptr_list
void initialize_memory_state(memory_segments* memory_state) {
    void** new_ptr_list = kmalloc(memory_state->num_ptrs * sizeof(void *));
    for (int i = 0; i < memory_state->num_ptrs; i++) {
        void *var_addr = kmalloc(memory_state->size_list[i]);
        memcpy(var_addr, memory_state->ptr_list[i], memory_state->size_list[i]);
        new_ptr_list[i] = var_addr;
    }
    memory_state->ptr_og_list = new_ptr_list;
}

// reset ptr_list in memory state with a copy of the values in ptr_og_list
void reset_memory_state(memory_segments* memory_state) {
    for (int i = 0; i < memory_state->num_ptrs; i++) {
        void *ptr = memory_state->ptr_list[i];
        void *ptr_og = memory_state->ptr_og_list[i];
        memcpy(ptr, ptr_og, memory_state->size_list[i]);
    }
}

void sha256_update_state(SHA256_CTX *ctx, memory_segments* memory_state){
    for(size_t i = 0; i < memory_state->num_ptrs; i++){
        sha256_update(ctx, (BYTE*)(memory_state->ptr_list)[i], memory_state->size_list[i]);
    }
}

// get the hash of the bytes in the memory state
uint32_t capture_memory_state(memory_segments* memory_state){
    SHA256_CTX ctx;
    BYTE buf[SHA256_BLOCK_SIZE];
    sha256_init(&ctx);
    sha256_update_state(&ctx, memory_state);
    sha256_final(&ctx, buf);
    uint32_t trunc_hash = *((uint32_t*)buf);
    return trunc_hash;
}

bool verify_memory_state(memory_segments* memory_state, uint64_t *valid_hashes, size_t num_valid_hashes){
    uint64_t hash = capture_memory_state(memory_state);
    for(size_t i = 0; i < num_valid_hashes; i++){
        if(hash == valid_hashes[i]){
            return true;
        }
    }
    return false;
}

void print_memstate(memory_segments* memory_state){
    for (size_t j = 0; j < memory_state->num_ptrs; j++) {
        printk("value at marked memory location %d: %d\n", j, *((int *)memory_state->ptr_list[j]));
    }
}