#include "rpi.h"
#include "state-space.h"
#include "equiv-threads.h"
#include <stdbool.h>
#include "permutations.h"
#include "memory.h"

#define NUM_VARS 3
#define NUM_FUNCS 3

/*
POTENTIAL TODOS:
1) Memory - Read/write set consistency with VM
2) Speed - run interleaving after generating each permutation
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

void interleave(int *counts, int *limits, int *result, uint32_t *count, int n, int total_chars, int switches, int ncs, int level, int lastInt, int **interleave_output) {
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

    int remaining_switches = ncs - switches;
    int remaining_slots = total_chars - level;
    if (remaining_switches > remaining_slots) {
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

void reset_threads(eq_th_t **thread_arr, size_t num_threads){
    for (int i = 0; i < num_threads; i++) {
        equiv_refresh(thread_arr[i]);
    }
}

// init threads from executables and store total num instructions per func in num_instrs 
size_t init_threads(eq_th_t **thread_arr, function_exec* executables, int **itl, size_t num_funcs, int *num_instrs) {
    for (size_t f_idx = 0; f_idx < num_funcs; f_idx++) {
        let th = equiv_fork(executables[itl[0][f_idx]].func_addr, NULL, 0);
        thread_arr[f_idx] = th;
        equiv_run();
        num_instrs[f_idx] = th->inst_cnt;
    }
    size_t total_instrs = 0;
    for (int i = 0; i < num_funcs; i++) {
        total_instrs += num_instrs[i];
    }
    return total_instrs;
}

// return total number of schedules that are possible with this number of ncs
uint32_t get_num_schedules(int *num_instrs, size_t total_instrs, size_t num_funcs, int ncs){
    int result[total_instrs]; // Result array to store one interleave
    int counts[num_funcs]; // Array to keep track of the counts of each character
    for (int i = 0; i < num_funcs; i++) {
        counts[i] = 0; // Initialize counts as 0
    }

    uint32_t count = 0; // Counter for the number of interleavings
    interleave(counts, num_instrs, result, &count, num_funcs, total_instrs, 0, ncs + num_funcs, 0, -1, NULL); // First call to get count
    return count;
}

// fill in tids and instr_nums with the schedule
void generate_schedules(int *num_instrs, size_t total_instrs, size_t num_funcs, int ncs, uint32_t count, uint32_t **tids, uint32_t **instr_nums){
    // arr is count * total_instrs, will store all schedules
    int **all_schedules = (int **)kmalloc(count * sizeof(int *));
    for (int i = 0; i < count; i++) {
        all_schedules[i] = (int *)kmalloc(total_instrs * sizeof(int));
    }

    int result[total_instrs]; // Result array to store one interleave
    int counts[num_funcs]; // Array to keep track of the counts of each character
    for (int i = 0; i < num_funcs; i++) {
        counts[i] = 0; // Initialize counts as 0
    }
    uint32_t temp_count = 0; // Reset count for the second call
    interleave(counts, num_instrs, result, &temp_count, num_funcs, total_instrs, 0, ncs + num_funcs, 0, -1, all_schedules); // Second call to fill the array

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
    int actual_ncs = ncs + num_funcs - 1;

    // fill in the tids and instruction numbers
    for (int i = 0; i < count; i++) {
        int* tid = all_schedules[i];
        int* instr_num = instr_idx[i];

        int ncs_idx = 0; 
        for (int c = 0; c < total_instrs - 1; c++) {
            if (tid[c] != tid[c + 1] && ncs_idx < actual_ncs) {
                tids[i][ncs_idx] = tid[c] + 1; 
                instr_nums[i][ncs_idx] = instr_num[c] + 1;
                ncs_idx += 1;
            }
        }
    }
}

// runs each interleaving for a given number of instructions
void run_interleavings(function_exec* executables, size_t num_funcs, int **itl, size_t num_perms, memory_segments* initial_mem_state, uint64_t *valid_hashes, int ncs) {
    equiv_init();
    uint32_t max_instrs = 0;

    // calculate num instrs for each function
    // via sequential execution
    disable_ctx_switch();
    int* num_instrs = kmalloc(num_funcs * sizeof(int));
    eq_th_t *threads[num_funcs];
    size_t total_instrs = init_threads(threads, executables, itl, num_funcs, num_instrs);
    reset_threads(threads, num_funcs);
    
    // calculate number of schedules
    uint32_t count = get_num_schedules(num_instrs, total_instrs, num_funcs, ncs);

    int actual_ncs = ncs + num_funcs - 1;

    uint32_t **tids       = (uint32_t **)kmalloc(count * sizeof(uint32_t *));
    uint32_t **instr_nums = (uint32_t **)kmalloc(count * sizeof(uint32_t *));
    for (int i = 0; i < count; i++) {
        tids[i] = (uint32_t *)kmalloc(actual_ncs * sizeof(uint32_t));
        instr_nums[i] = (uint32_t *)kmalloc(actual_ncs * sizeof(uint32_t));
    }

    // generate schedules
    generate_schedules(num_instrs, total_instrs, num_funcs, ncs, count, tids, instr_nums);

    for (int sched_idx = 0; sched_idx < count; sched_idx++) {
        reset_memory_state(initial_mem_state);
        
        for (int i = 0; i < actual_ncs; i++) {
            tids[sched_idx][i] = tids[sched_idx][i];
        }

        for (int i = 0; i < actual_ncs; i++) {
            printk("(%d", tids[sched_idx][i] - 1);
            printk(", %d) ", instr_nums[sched_idx][i]);
        }
        printk("\n");
        //continue;

        // load next schedule
        set_ctx_switches(tids[sched_idx], instr_nums[sched_idx], actual_ncs); 
        equiv_run();
        reset_threads(threads, num_funcs);

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
        printk("\n");
        printk("\n");
    }
}

void notmain() {    
    // number of interleaved context switches (remaining context switches will result in threads being run to completion)
    int interleaved_ncs = 2; 

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
    run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs); 
}