#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"
#include "equiv-checker.h"

#define NUM_VARS 1
#define NUM_FUNCS 2
#define load_store_mode 0

// USER CODE
int* global_var;

// multiply by 4 and add 1
__attribute__((section(".user")))
void funcMA(void **arg) {
    int a = *global_var;
    a += 1; 
    *global_var = a;   
}

// subtracts 1 from global var a
__attribute__((section(".user")))
void funcMS(void **arg) {
    int a = *global_var;
    a *= 2;
    *global_var = a;
}

void notmain() {    
    equiv_checker_init();
    // number of interleaved context switches (remaining context switches will result in threads being run to completion)
    int interleaved_ncs = 1; 

    global_var = kmalloc(sizeof(int));
    *global_var = 5;
    
    // convert global vars to an array of pointers
    int *mem_locations[NUM_VARS] = {global_var};
    size_t sizes[NUM_VARS] = {sizeof(int)};

    memory_segments initial_mem_state = {NUM_VARS, (void **)mem_locations, NULL, sizes}; 
    initialize_memory_state(&initial_mem_state);

    const size_t num_perms = factorial(NUM_FUNCS);
    int **itl = get_func_permutations(NUM_FUNCS);
    uint64_t valid_hashes[num_perms];

    function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));
    executables[0].func_addr = (func_ptr)funcMA;
    executables[1].func_addr = (func_ptr)funcMS;

    find_good_hashes(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes);
    run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
    // run_interleavings_as_generated(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
}
