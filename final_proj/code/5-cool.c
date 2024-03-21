#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"

#define NUM_VARS 1
#define NUM_FUNCS 2

int* global_var;
int* global_var2;
int* global_var3;

// Function A - increment global_var and set global_var2 to its new value
void funcA(void **arg) {
    *global_var  = 4; 
    *global_var2 = 5; 
    *global_var3 = *global_var + *global_var2;
}

// Function B - double global_var2 and increment global_var based on the new value of global_var2
void funcB(void **arg) {
    *global_var  = 5; 
    *global_var2 = 4; 
    *global_var3 = *global_var + *global_var2;
}

void notmain() {
    int interleaved_ncs = 1; 
    // int interleaved_ncs = 2;

    global_var = kmalloc(sizeof(int));
    *global_var = 0;
    global_var2 = kmalloc(sizeof(int));
    *global_var2 = 0;
    global_var3 = kmalloc(sizeof(int));
    *global_var3 = 0;

    int *global_vars[NUM_VARS] = {global_var3};
    size_t sizes[NUM_VARS] = {sizeof(int)};

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
    run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
}
