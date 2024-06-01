#include "memory.h"
#define XXH_INLINE_ALL 1
#include "xxhash.h"

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

// get the hash of the bytes in the memory state
uint32_t capture_memory_state(memory_segments* memory_state){
    XXH32_state_t* state = XXH32_createState();
    for(size_t i = 0;i < memory_state->num_ptrs; i++) {
      XXH32_update(state, (memory_state->ptr_list)[i], memory_state->size_list[i]);
    }
    XXH32_hash_t hash = XXH32_digest(state);
    return hash;
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
        printk("marked memory %d value: %d\n", j, *((int *)memory_state->ptr_list[j]));
    }
}


void print_mem_value(uint32_t v, void* arg) {
  printk("\t%x : %x\n", v, *((volatile char*)v));
}

void print_mem(const char* msg, set_t* mem) {
  if(msg) printk(msg);
  set_foreach(mem, print_mem_value, NULL);
  printk("\tHash: %x\n", hash_mem(mem));
}

void hash_mem_value(uint32_t v, void* arg) {
  XXH32_state_t* state = (XXH32_state_t*)arg;
  XXH32_update(state, (char*)v, 1);
}

uint32_t hash_mem(set_t* mem) {
  XXH32_state_t* state = XXH32_createState();
  set_foreach(mem, hash_mem_value, state);
  XXH32_hash_t hash = XXH32_digest(state);
  return hash;
}
