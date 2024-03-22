#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"

#define NUM_VARS 2
#define NUM_FUNCS 2
#define load_store_mode 1

typedef int vibe_check_t;

// Assembly function to try acquiring the lock
extern int vibe_check(vibe_check_t *cur_vibes);
extern void secure_vibes(vibe_check_t *cur_vibes);
extern void release_vibes(vibe_check_t *cur_vibes);

// Spin lock functions
void vibe_init(vibe_check_t *cur_vibes) {
    *cur_vibes = 0; // 0 indicates that the lock is available
}

int* global_var;
int* global_var2;
vibe_check_t cur_vibes; // Global spin lock

// Function A
void funcA(void **arg) {
    // spin_lock(&lock); // Acquire the lock
    secure_vibes(&cur_vibes); // Acquire the lock
    
    // Perform operations
    *global_var += 1;
    *global_var2 = *global_var;

    release_vibes(&cur_vibes); // Release the lock
    // spin_unlock(&lock); // Release the lock
}

// Function B 
void funcB(void **arg) {
    // spin_lock(&lock); // Acquire the lock
    secure_vibes(&cur_vibes); // Acquire the lock
    
    // Perform operations
    *global_var += 2;
    *global_var2 = *global_var; 

    release_vibes(&cur_vibes); // Release the lock
    // spin_unlock(&lock); // Release the lock
}

void notmain() {
    int interleaved_ncs = 2;

    global_var = kmalloc(sizeof(int));
    *global_var = 0;
    global_var2 = kmalloc(sizeof(int));
    *global_var2 = 0;

    int *mem_locations[NUM_VARS] = {global_var, global_var2};
    size_t sizes[NUM_VARS] = {sizeof(int), sizeof(int)};

    memory_segments initial_mem_state = {NUM_VARS, (void **)mem_locations, NULL, sizes};
    initialize_memory_state(&initial_mem_state);

    const size_t num_perms = factorial(NUM_FUNCS);
    int **itl = get_func_permutations(NUM_FUNCS);
    uint64_t valid_hashes[num_perms];

    function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));
    executables[0].func_addr = (func_ptr)funcA;
    executables[1].func_addr = (func_ptr)funcB;

    find_good_hashes(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes);
 
    vibe_init(&cur_vibes);
    // run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
    run_interleavings_as_generated(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
}

