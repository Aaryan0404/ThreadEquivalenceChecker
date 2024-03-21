#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"

#define NUM_VARS 2
#define NUM_FUNCS 2

typedef int spin_lock_t;

// Assembly function to try acquiring the lock
extern int try_lock(spin_lock_t *lock);
extern void spin_lock(spin_lock_t *lock);
extern void spin_unlock(spin_lock_t *lock);

// Spin lock functions
void spin_init(spin_lock_t *lock) {
    *lock = 0; // 0 indicates that the lock is available
}

int* global_var;
int* global_var2;
spin_lock_t lock; // Global spin lock

// Function A
void funcA(void **arg) {
    // spin_lock(&lock); // Acquire the lock
    spin_lock(&lock); // Acquire the lock
    
    // Perform operations
    *global_var += 1;
    *global_var2 = *global_var;

    spin_unlock(&lock); // Release the lock
    // spin_unlock(&lock); // Release the lock
}

// Function B 
void funcB(void **arg) {
    // spin_lock(&lock); // Acquire the lock
    spin_lock(&lock); // Acquire the lock
    
    // Perform operations
    *global_var += 2;
    *global_var2 = *global_var; 

    spin_unlock(&lock); // Release the lock
    // spin_unlock(&lock); // Release the lock
}

void notmain() {
    int interleaved_ncs = 2;

    global_var = kmalloc(sizeof(int));
    *global_var = 0;
    global_var2 = kmalloc(sizeof(int));
    *global_var2 = 0;

    int *global_vars[NUM_VARS] = {global_var, global_var2};
    size_t sizes[NUM_VARS] = {sizeof(int), sizeof(int)};

    memory_segments initial_mem_state = {NUM_VARS, (void **)global_vars, NULL, sizes};
    initialize_memory_state(&initial_mem_state);

    const size_t num_perms = factorial(NUM_FUNCS);
    int **itl = kmalloc(num_perms * sizeof(int *));
    for (int i = 0; i < num_perms; i++) {
        itl[i] = kmalloc(NUM_FUNCS * sizeof(int));
    }
    find_permutations(itl, NUM_FUNCS);

    uint64_t valid_hashes[num_perms];

    function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));
    executables[0].func_addr = (func_ptr)funcA;
    executables[0].num_vars = 0;
    executables[0].var_list = NULL;

    executables[1].func_addr = (func_ptr)funcB;
    executables[1].num_vars = 0;
    executables[1].var_list = NULL;

    find_good_hashes(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes);

    int load_store_mode = 1;  
    spin_init(&lock);
    run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
}

