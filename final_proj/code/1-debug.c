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
void funcMA(void **arg) {
    int a = *global_var;
    a += 1; 
    *global_var = a;   
}

// subtracts 1 from global var a
void funcMS(void **arg) {
    int b = *global_var;
    b *= 2;
    *global_var = b;
}

void notmain() {    
    // number of interleaved context switches (remaining context switches will result in threads being run to completion)
    equiv_checker_init();

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

    /*
    schedule 2: (fid: 0, instr: 4) (fid: 1, instr: 7) 
    ----invalid state detected----
    marked memory 0 value: 6

    */
    uint32_t sched_fid[2] = {0, 1};
    uint32_t sched_instr[2] = {4, 7};
    int interleaved_ncs = 1; 

    run_one_schedule(executables, NUM_FUNCS, &initial_mem_state, load_store_mode, sched_fid, sched_instr, interleaved_ncs);
}