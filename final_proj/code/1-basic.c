#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"
#include "equiv-checker.h"
#include "equiv-rw-set.h"
#include "memory.h"

#define NUM_VARS 1
#define NUM_FUNCS 2
#define load_store_mode 0

// USER CODE
int* global_var;

// multiply by 4 and add 1
EQUIV_USER
void funcMA(void **arg) {
    int a = *global_var;
    a += 1; 
    *global_var = a;   
}

// subtracts 1 from global var a
EQUIV_USER
void funcMS(void **arg) {
    int a = *global_var;
    a *= 2;
    *global_var = a;
}

void init_memory() {
  *global_var = 5;
}

void notmain() {    
    equiv_checker_init();

    // number of interleaved context switches (remaining context switches will result in threads being run to completion)
    int interleaved_ncs = 1; 

    global_var = kmalloc(sizeof(int));
    *global_var = 5;

    set_t* ms_read_set = set_alloc();
    set_t* ms_write_set = set_alloc();

    find_rw_set(funcMS, ms_read_set, ms_write_set);
    set_print("MS read set\n", ms_read_set);
    set_print("MS write set\n", ms_write_set);

    set_t* ma_read_set = set_alloc();
    set_t* ma_write_set = set_alloc();

    find_rw_set(funcMA, ma_read_set, ma_write_set);
    set_print("MA read set\n", ma_read_set);
    set_print("MA write set\n", ma_write_set);

    set_t* shared_memory = set_alloc();
    set_intersection_inplace(ms_read_set, ma_write_set);
    set_intersection_inplace(ma_read_set, ms_write_set);
    set_intersection(shared_memory, ms_write_set, ma_write_set);
    set_union_inplace(shared_memory, ms_read_set);
    set_union_inplace(shared_memory, ma_read_set);


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

    set_t* vh = set_alloc();
    find_good_hashes_2(
      executables, NUM_FUNCS,
      init_memory,
      itl, num_perms,
      shared_memory,vh 
    );

    set_print("Valid hashes\n", vh);

    find_good_hashes(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes);
    run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
    // run_interleavings_as_generated(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
}
