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

void atomic_increment(int *ptr) {
    secure_vibes(&cur_vibes);
    *ptr += 1; 
    release_vibes(&cur_vibes);
}

void atomic_decrement(int *ptr) {
    secure_vibes(&cur_vibes);
    *ptr -= 1; 
    release_vibes(&cur_vibes);
}

void atomic_load(int *ptr, int *result) {
    secure_vibes(&cur_vibes);
    *result = *ptr;
    release_vibes(&cur_vibes);
}

void atomic_store(int *ptr, int *val) {
    secure_vibes(&cur_vibes);
    *ptr = *val;
    release_vibes(&cur_vibes);
}

// Function A
void funcA(void **arg) {
    atomic_increment(global_var); 
}

// Function B 
void funcB(void **arg) {
    atomic_decrement(global_var);
}

// Function A bad
void funcA_bad(void **arg) {
    *global_var += 1;
}

// Function B bad
void funcB_bad(void **arg) {
    *global_var -= 1; 
}

void funcC_bad(void **arg) {
    if (*global_var != 1) {
        *global_var = 5; 
    }
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
    // executables[0].func_addr = (func_ptr)funcA_bad;
    // executables[1].func_addr = (func_ptr)funcB_bad;

    find_good_hashes(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes);

    vibe_init(&cur_vibes);
    // run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
    run_interleavings_as_generated(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
}
