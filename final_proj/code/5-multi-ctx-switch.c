#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"
#include "equiv-checker.h"

#define NUM_VARS 1
#define NUM_FUNCS 2
#define load_store_mode 1

int* global_var;
int* global_var2;
int* global_var3;

// Function A - increment global_var and set global_var2 to its new value
EQUIV_USER
void funcA(void **arg) {
    *global_var  = 4; 
    *global_var2 = 5; 
    *global_var3 = *global_var + *global_var2;
}

// Function B - double global_var2 and increment global_var based on the new value of global_var2
EQUIV_USER
void funcB(void **arg) {
    *global_var  = 5; 
    *global_var2 = 4; 
    *global_var3 = *global_var + *global_var2;
}

void notmain() {
    // int interleaved_ncs = 1; 
    int interleaved_ncs = 2;

    equiv_checker_init();

    global_var = kmalloc(sizeof(int));
    *global_var = 0;
    global_var2 = kmalloc(sizeof(int));
    *global_var2 = 0;
    global_var3 = kmalloc(sizeof(int));
    *global_var3 = 0;

    int *mem_locations[NUM_VARS] = {global_var3};
    size_t sizes[NUM_VARS] = {sizeof(int)};

    memory_segments initial_mem_state = {NUM_VARS, (void **)mem_locations, NULL, sizes};
    initialize_memory_state(&initial_mem_state);

    const size_t num_perms = factorial(NUM_FUNCS);
    int **itl = get_func_permutations(NUM_FUNCS);
    uint64_t valid_hashes[num_perms];

    function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));
    executables[0].func_addr = (func_ptr)funcA;
    executables[1].func_addr = (func_ptr)funcB;

    find_good_hashes(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes);
    run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
}
