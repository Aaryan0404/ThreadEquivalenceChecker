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
1) Support for multiple variables - DONE
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
uint32_t convert_funcid_to_tid(uint32_t func_id, uint32_t last_tid){
    return func_id + last_tid + 1;
}

// void interleave(int *counts, int *limits, char *result, int *count, int n, int total_chars, int switches, int ncs, int level, char lastChar) {
//     if (level == total_chars && switches == ncs) {
//         result[level] = '\0';
//         printk("%s\n", result);
//         (*count)++;
//         return;
//     }

//     for (int i = 0; i < n; i++) {
//         if (counts[i] < limits[i] && ((i + '0') == lastChar || switches < ncs)) {
//             //int newSwitches = ((i + '0') != lastChar) ? switches + 1 : switches;
//             int newSwitches = switches;
//             if((i + '0') != lastChar) {
//                 newSwitches = switches + 1;
//             }
//             result[level] = i + '0';
//             counts[i]++;
//             interleave(counts, limits, result, count, n, total_chars, newSwitches, ncs, level + 1, (i + '0'));
//             counts[i]--;
//         }
//     }
// }

void interleave(int *counts, int *limits, int *result, int *count, int n, int total_chars, int switches, int ncs, int level, int lastInt, int **interleave_output) {
    if (level == total_chars && switches == ncs) {
        if (interleave_output != NULL) {
            for (int i = 0; i < level; i++) {
                interleave_output[*count][i] = result[i];
                printk("%d", result[i]);
            }
            printk("\n");
        }
        (*count)++;
        return;
    }

    for (int i = 0; i < n; i++) {
        if (counts[i] < limits[i] && (i == lastInt || switches < ncs)) {
            int newSwitches = (i != lastInt) ? switches + 1 : switches;
            result[level] = i;
            counts[i]++;
            interleave(counts, limits, result, count, n, total_chars, newSwitches, ncs, level + 1, i, interleave_output);
            counts[i]--;
        }
    }
}

// runs each interleaving for a given number of instructions
void run_interleavings(function_exec* executables, size_t num_funcs, int **itl, size_t num_perms, memory_segments* initial_mem_state, uint64_t *valid_hashes, int ncs) {
    equiv_init();
    uint32_t max_instrs = 0;
    uint32_t last_tid = 0; 

    // TODO: calculate num instrs for each function
    // via sequential execution
    int* num_instrs = kmalloc(num_funcs * sizeof(int));
    for (size_t f_idx = 0; f_idx < num_funcs; f_idx++) {
        let th = equiv_fork(executables[itl[0][f_idx]].func_addr, NULL, 0);
        last_tid += 1;
        equiv_run();
        num_instrs[f_idx] = th->inst_cnt;
    }
    // Number instructions per function 
    // int num_instrs[] = {2, 2, 2};

    
    int total_instrs = 0;
    for (int i = 0; i < num_funcs; i++) {
        total_instrs += num_instrs[i];
    }

    int result[total_instrs]; // Result array to store one interleave
    int counts[num_funcs]; // Array to keep track of the counts of each character
    for (int i = 0; i < num_funcs; i++) {
        counts[i] = 0; // Initialize counts as 0
    }

    int count = 0; // Counter for the number of interleavings
    interleave(counts, num_instrs, result, &count, num_funcs, total_instrs, 0, ncs + num_funcs, 0, -1, NULL); // First call to get count
    
    // arr is count * total_instrs, will store all schedules
    int **all_schedules = (int **)kmalloc(count * sizeof(int *));
    for (int i = 0; i < count; i++) {
        all_schedules[i] = (int *)kmalloc(total_instrs * sizeof(int));
    }

    count = 0; // Reset count for the second call
    interleave(counts, num_instrs, result, &count, num_funcs, total_instrs, 0, ncs + num_funcs, 0, -1, all_schedules); // Second call to fill the array

    // create an array of instruction indices (num_interleavings * num_funcs)
    int **instr_idx = (int **)kmalloc(count * sizeof(int *));
    for (int i = 0; i < count; i++) {
        instr_idx[i] = (int *)kmalloc(total_instrs * sizeof(int));
    }

    // fills in the instruction indices
    for (int i = 0; i < count; i++) {

        // one bin per function, each bin has a counter
        int* instr_counter = kmalloc(num_funcs * sizeof(int));
        for (int j = 0; j < num_funcs; j++) {
            instr_counter[j] = 0;
        }

        // fill in the instruction indices by func
        for (int j = 0; j < total_instrs; j++) {
            instr_idx[i][j] = instr_counter[all_schedules[i][j]];
            instr_counter[all_schedules[i][j]] += 1;
        }
    }

    // create an array of tids and instruction numbers
    uint32_t **tids       = (uint32_t **)kmalloc(count * sizeof(uint32_t *));
    uint32_t **instr_nums = (uint32_t **)kmalloc(count * sizeof(uint32_t *));
    for (int i = 0; i < count; i++) {
        tids[i] = (uint32_t *)kmalloc(ncs * sizeof(uint32_t));
        instr_nums[i] = (uint32_t *)kmalloc(ncs * sizeof(uint32_t));
    }

    // fill in the tids and instruction numbers
    for (int i = 0; i < count; i++) {
        int* tid = all_schedules[i];
        int* instr_num = instr_idx[i];

        int ncs_idx = 0; 
        for (int c = 0; c < total_instrs - 1; c++) {
            if (tid[c] != tid[c + 1] && ncs_idx < ncs) {
                tids[i][ncs_idx] = tid[c] + 1; 
                instr_nums[i][ncs_idx] = instr_num[c] + 1;
                ncs_idx += 1;
            }
        }
    }

    for (int sched_idx = 0; sched_idx < count; sched_idx++) {
        reset_memory_state(initial_mem_state);
        eq_th_t *threads[num_funcs];

        

        for (int i = 0; i < ncs; i++) {
            tids[sched_idx][i] = convert_funcid_to_tid(tids[sched_idx][i], last_tid);
        }

        for (size_t f_idx = 0; f_idx < num_funcs; f_idx++) {
            
            threads[f_idx] = equiv_fork(executables[itl[0][f_idx]].func_addr, NULL, 0);
            last_tid += 1;
        }

        
        for (int i = 0; i < ncs; i++) {
            printk("(%d", tids[sched_idx][i]);
            printk(", %d) ", instr_nums[sched_idx][i]);
        }
        printk("\n");
        printk("ncs: %d\n", ncs);
        set_ctx_switches(tids[sched_idx], instr_nums[sched_idx], ncs); 

        equiv_run();
        printk("threads[0]: %d\n", threads[0]);

        // uint64_t hash = capture_memory_state(initial_mem_state);
        // bool valid = false;
        // for (size_t j = 0; j < num_perms; j++) {
        //     if (hash == valid_hashes[j]) {
        //         valid = true;
        //         break;
        //     }
        // }

        // if (valid) {
        //     for (size_t j = 0; j < initial_mem_state->num_ptrs; j++) {
        //         printk("valid, global var: %d\n", *((int *)initial_mem_state->ptr_list[j]));
        //     }
        // } else {
        //     for (size_t j = 0; j < initial_mem_state->num_ptrs; j++) {
        //         printk("invalid, global var: %d\n", *((int *)initial_mem_state->ptr_list[j]));
        //     }
        // }
    }
    //printk("\n");
}

void notmain() {    
    // number of context switches
    int ncs = 1; 

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
    // for (int i = 0; i < initial_mem_state.num_ptrs; i++) {
    //     printk("initial_mem_state.ptr_list[%d]: %d\n", i, *((int *)initial_mem_state.ptr_list[i]));
    // }

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

    run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, ncs); 
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