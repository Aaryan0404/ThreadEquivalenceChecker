#include "rpi.h"
#include "state-space.h"
#include "equiv-threads.h"
#include <stdbool.h>
#include "permutations.h"
#include "memory.h"

#define NUM_VARS 2
#define NUM_FUNCS 2

/*
POTENTIAL TODOS:
1) Support for multiple variables
2) Multiple functions (and equivalent threads)
3) Multiple context switches
4) Memory - Read/write set consistency with VM
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

// finds valid hashes for each permutation
void find_good_hashes(function_exec* executables, size_t num_funcs, int **itl, size_t num_perms, memory_segments* initial_mem_state, uint64_t *valid_hashes) {
    for (size_t i = 0; i < num_perms; i++) {
        reset_memory_state(initial_mem_state);
        for (size_t j = 0; j < num_funcs; j++) {
            // run func for this permutation with the corresponding variables
            executables[itl[i][j]].func_addr(executables[itl[i][j]].var_list);
        }
        uint64_t hash = capture_memory_state(initial_mem_state);
        valid_hashes[i] = hash;
    }
}

// runs each interleaving for a given number of instructions
void run_interleavings(function_exec* executables, size_t num_funcs, int **itl, size_t num_perms, memory_segments* initial_mem_state, uint64_t *valid_hashes) {
    equiv_init();
    uint32_t max_instrs = 0;

    uint32_t last_tid = 0; 
    for (size_t k = 0; k < num_funcs; k++) {
        for (size_t i = 1; i <= 16384; i++) {
            // halt early once we reach a sequential ordering
            if(max_instrs != 0 && i > max_instrs){
                last_tid = 1 + (i - 1) * (num_funcs) + last_tid;
                break;
            }

            reset_memory_state(initial_mem_state);
            eq_th_t *threads[num_funcs];
            for (size_t f_idx = 0; f_idx < num_funcs; f_idx++) {
                threads[f_idx] = equiv_fork(executables[itl[0][f_idx]].func_addr, NULL, 0);
            }

            // thread tid will run for i instructions, and then context switch.
            uint32_t tid = 1 + (i - 1) * (num_funcs) + last_tid; 
            // uint32_t tid = 2 + (i - 1) * (num_funcs); // B
            // uint32_t tid = (k + 1) + (i - 1) * (num_funcs); // C

            set_ctx_switch_tid(tid); 
            set_ctx_switch_instr_num(i);

            equiv_run();

            // calculate max instructions used for each thread
            for (size_t j = 0; j < num_funcs; j++) {
                if(threads[j]->inst_cnt > max_instrs){
                    max_instrs = threads[j]->inst_cnt;
                }
            }

            uint64_t hash = capture_memory_state(initial_mem_state);
            bool valid = false;
            for (size_t j = 0; j < num_perms; j++) {
                if (hash == valid_hashes[j]) {
                    valid = true;
                    break;
                }
            }

            if (valid) {
                for (size_t j = 0; j < initial_mem_state->num_ptrs; j++) {
                    printk("valid, global var: %d\n", *((int *)initial_mem_state->ptr_list[j]));
                }
            } else {
                for (size_t j = 0; j < initial_mem_state->num_ptrs; j++) {
                    printk("invalid, global var: %d\n", *((int *)initial_mem_state->ptr_list[j]));
                }
            }
        }
    }
}

void notmain() {

    // arbitrary number of global vars, 
    // wrapped in initial_mem_state struct
        // will contain actual memory (to be modified)
        // will contain copy of original memory

    // allocate and initialize global vars
    global_var = kmalloc(sizeof(int));
    global_var2 = kmalloc(sizeof(int));
    *global_var = 5;
    *global_var2 = 10;
    
    // convert global vars to an array of pointers
    int *global_vars[NUM_VARS] = {global_var, global_var2};
    size_t sizes[NUM_VARS] = {sizeof(int), sizeof(int)};

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

    find_good_hashes(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes);
    // on a single thread, run each interleaving
    // for loop of 2d array dim 0 itrs, with 
    // each itr having dim 1 func executions
    // for each itr,
        // capture memory state
        // hash it 
        // store in array (set) of valid hashes
        // reset memory state

    // generate schedule for thread 0, thread 1
    // for i from 0 to 500
        // schedule a
        // {t0, t0, t0, ..., t0} i times
        // {t1, ...} to completion

        // schedule b
        // {t1, t1, t1, ..., t1} i times
        // {t0, ...} to completion
    // total of 1000 schedules

    run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes);
    // do equiv init
    // launch 2 threads
        // thread 0: funcMA
        // thread 1: funcMS
        // run schedule a
    // equiv run
    // capture global state
    // hash it
    // compare to set of valid hashes - if match, print "valid" else, print "invalid"
    // reset memory state
    // equiv_refresh
        // thread 0: funcMS
        // thread 1: funcMA
        // run schedule b
}