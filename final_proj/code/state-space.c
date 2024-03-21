#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"

#define NUM_VARS 3
#define NUM_FUNCS 3

/*
POTENTIAL TODOS:
1) Memory - Read/write set consistency with VM
2) Speed - run interleaving after generating each permutation
3) General - compile at a higher optimization (also will reduce instructions which reduces speed)
*/

/*
APPLICATIONS: 
- basic self-generated correct and incorrect examples
- GPT generated lock-free data structures
- c standard atomics
*/

// USER CODE
int* global_var;
int* global_var2;
int* global_var3;

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

// independent third function
void funcIndep(void **arg) {
    int a = *global_var3; 
    a *= 2;
    a += 1;

    *global_var3 = a;
}

void notmain() {    
    // number of interleaved context switches (remaining context switches will result in threads being run to completion)
    int interleaved_ncs = 1; 

    // arbitrary number of global vars, 
    // wrapped in initial_mem_state struct
        // will contain actual memory (to be modified)
        // will contain copy of original memory

    // allocate and initialize global vars
    global_var = kmalloc(sizeof(int));
    global_var2 = kmalloc(sizeof(int));
    global_var3 = kmalloc(sizeof(int));
    *global_var = 5;
    *global_var2 = 10;
    *global_var3 = 15;
    
    // convert global vars to an array of pointers
    int *global_vars[NUM_VARS] = {global_var, global_var2, global_var3};
    size_t sizes[NUM_VARS] = {sizeof(int), sizeof(int), sizeof(int)};

    memory_segments initial_mem_state = {NUM_VARS, (void **)global_vars, NULL, sizes}; 
    initialize_memory_state(&initial_mem_state);

    // print statement that shows value of 
    // initial memory state
    for (int i = 0; i < initial_mem_state.num_ptrs; i++) {
        printk("initial_mem_state.ptr_list[%d]: %d\n", i, *((int *)initial_mem_state.ptr_list[i]));
    }

    // dependeing on num functions, generate
    // a 2d array of function interleavings (sequential outcomes)
    const size_t num_perms = factorial(NUM_FUNCS);
    int **itl = kmalloc(num_perms * sizeof(int *));
    for (int i = 0; i < num_perms; i++) {
        itl[i] = kmalloc(NUM_FUNCS * sizeof(int));
    }
    find_permutations(itl, NUM_FUNCS);

    // create an array (set) of memory hashes
    uint64_t valid_hashes[num_perms];

    function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));

    executables[0].func_addr = (func_ptr)funcMA;
    executables[0].num_vars = 0; 
    executables[0].var_list = NULL;

    executables[1].func_addr = (func_ptr)funcMS;
    executables[1].num_vars = 0; 
    executables[1].var_list = NULL;

    executables[2].func_addr = (func_ptr)funcIndep;
    executables[2].num_vars = 0;
    executables[2].var_list = NULL;

    find_good_hashes(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes);
    // run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, 0); 
    run_interleavings_as_generated(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, 1);
}