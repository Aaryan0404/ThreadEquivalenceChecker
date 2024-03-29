#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"

#define NUM_VARS 3
#define NUM_FUNCS 3
#define load_store_mode 1

// USER CODE
int* global_var;
int* global_var2;
int** global_var3;

// multiply by 4 and add 1
void funcMA(void **arg) {
    int a = *global_var;
    a += 1; 
    *global_var = a;   

    int b = *global_var2;
    b *= 2;
    *global_var2 = b;
}

// subtracts 1 from global var a
void funcMS(void **arg) {
    int a = *global_var2; 
    a *= 2;
    *global_var2 = a;

    int b = *global_var;
    b += 1;
    *global_var = b;
}

// set all elements of global_var3 to global_var
void funcIndep(void **arg) {
    for (int i = 0; i < 3; i++) {
        *global_var3[i] = *global_var;
    }
}

void notmain() {    
    set_verbosity(2);
    // number of interleaved context switches (remaining context switches will result in threads being run to completion)
    int interleaved_ncs = 1; 

    global_var = kmalloc(sizeof(int));
    global_var2 = kmalloc(sizeof(int));
    global_var3 = kmalloc(sizeof(int) * 3);
    *global_var = 5;
    *global_var2 = 10;

    *global_var3[0] = 0;
    *global_var3[1] = 0;
    *global_var3[2] = 0;
    
    // convert global vars to an array of pointers
    void *mem_locations[NUM_VARS] = {(void *)global_var, (void *)global_var2, (void *)global_var3};
    size_t sizes[NUM_VARS] = {sizeof(int), sizeof(int), sizeof(int) * 3};

    memory_segments initial_mem_state = {NUM_VARS, (void **)mem_locations, NULL, sizes}; 
    initialize_memory_state(&initial_mem_state);

    const size_t num_perms = factorial(NUM_FUNCS);
    int **itl = get_func_permutations(NUM_FUNCS);
    uint64_t valid_hashes[num_perms];

    function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));
    executables[0].func_addr = (func_ptr)funcMA;
    executables[1].func_addr = (func_ptr)funcMS;
    executables[2].func_addr = (func_ptr)funcIndep;

    find_good_hashes(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes);
    run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
    // run_interleavings_as_generated(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
}