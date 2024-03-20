
#include "interleaver.h"
#include <stdbool.h>
#include "rpi.h"
#include "memory_trap.h"
int verbose = 3;

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
        num_instrs[f_idx] = th->loadstr_cnt;
    }
    size_t total_instrs = 0;
    for (int i = 0; i < num_funcs; i++) {
        total_instrs += num_instrs[i];
    }
    return total_instrs;
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

void interleave(int *counts, int *limits, int *result, uint32_t *count, int n, int total_chars, int switches, int ncs, int level, int lastInt, int **interleave_output) {
    if (level == total_chars && switches == ncs) {
        if (interleave_output != NULL) {
            if(verbose >= 3){
                for (int i = 0; i < level; i++) {
                    interleave_output[*count][i] = result[i];
                    printk("%d", result[i]);
                }
                printk("\n");
            }
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
    init_memory_trap();
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
        
        if(verbose >= 2){
            printk("schedule %d: ", sched_idx);
            for (int i = 0; i < actual_ncs; i++) {
                printk("(%d", tids[sched_idx][i] - 1);
                printk(", %d) ", instr_nums[sched_idx][i]);
            }
            printk("\n");
        }

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
            if(verbose >= 2){
                for (size_t j = 0; j < initial_mem_state->num_ptrs; j++) {
                    printk("valid, global var: %d\n", *((int *)initial_mem_state->ptr_list[j]));
                }
                printk("\n");
                printk("\n");
            }
        } else {
            if(verbose >= 1){
                for (size_t j = 0; j < initial_mem_state->num_ptrs; j++) {
                    printk("invalid, global var: %d\n", *((int *)initial_mem_state->ptr_list[j]));
                }
                printk("\n");
                printk("\n");
            }
        }
    }
}

void run_schedule(uint32_t total_instrs, int num_funcs, int ncs, int *raw_sched, eq_th_t **thread_arr, memory_segments* initial_mem_state){
    int instr_idx[total_instrs];
            
    int instr_counter[num_funcs];
    for (int j = 0; j < num_funcs; j++) {
        instr_counter[j] = 0;
    }

    // fill in the instruction indices by func
    for (int j = 0; j < total_instrs; j++) {
        instr_idx[j] = instr_counter[raw_sched[j]];
        instr_counter[raw_sched[j]] += 1;
    }
    int actual_ncs = ncs - 1;
    int ncs_idx = 0; 

    uint32_t* tids = kmalloc(actual_ncs * sizeof(uint32_t));
    uint32_t* instr_nums = kmalloc(actual_ncs * sizeof(uint32_t));

    for (int c = 0; c < total_instrs - 1; c++) {
        if (raw_sched[c] != raw_sched[c + 1] && ncs_idx < actual_ncs) {
            tids[ncs_idx] = raw_sched[c] + 1; 
            instr_nums[ncs_idx] = instr_idx[c] + 1;
            ncs_idx += 1;
        }
    }
    if(verbose >= 2){
        for (int i = 0; i < actual_ncs; i++) {
            printk("(%d", tids[i] - 1);
            printk(", %d) ", instr_nums[i]);
        }
        printk("\n");
    }
    
    reset_memory_state(initial_mem_state);
    set_ctx_switches(tids, instr_nums, actual_ncs); 
    equiv_run();
    reset_threads(thread_arr, num_funcs);
}

void interleave_and_run(int *counts, int *limits, int *result, uint32_t *count, int num_funcs, int total_chars, int switches, int ncs, int level, int lastInt, eq_th_t **thread_arr, memory_segments* initial_mem_state, size_t num_perms, uint64_t *valid_hashes, bool* escape) {
    if(*escape){
        return;
    }
    if (level == total_chars && switches == ncs) {
        if(verbose >= 3){
                for (int i = 0; i < level; i++) {
                    printk("%d", result[i]);
                }
                printk("\n");
        }

        run_schedule(total_chars, num_funcs, ncs, result, thread_arr, initial_mem_state);
        bool valid = verify_memory_state(initial_mem_state, valid_hashes, num_perms);

        if (valid) {
            if(verbose >= 2){
                for (size_t j = 0; j < initial_mem_state->num_ptrs; j++) {
                    printk("valid, global var: %d\n", *((int *)initial_mem_state->ptr_list[j]));
                }
                printk("\n");
                printk("\n");
            }
        } else {
            //*escape = true;
            if(verbose >= 1){
                for (size_t j = 0; j < initial_mem_state->num_ptrs; j++) {
                    printk("invalid, global var: %d\n", *((int *)initial_mem_state->ptr_list[j]));
                }
                printk("\n");
                printk("\n");
            }
        }
        (*count)++;
        return;
    }

    int remaining_switches = ncs - switches;
    int remaining_slots = total_chars - level;
    if (remaining_switches > remaining_slots) {
        return;
    }

    for (int i = 0; i < num_funcs; i++) {
        if (counts[i] < limits[i] && (i == lastInt || switches < ncs)) {
            int newSwitches = (i != lastInt) ? switches + 1 : switches;
            result[level] = i;
            counts[i]++;
            interleave_and_run(counts, limits, result, count, num_funcs, total_chars, newSwitches, ncs, level + 1, i, thread_arr, initial_mem_state, num_perms, valid_hashes, escape);
            counts[i]--;
        }
    }
}

// fill in tids and instr_nums with the schedule
void generate_and_run_schedules(int *num_instrs, size_t total_instrs, size_t num_funcs, int ncs, eq_th_t **thread_arr, memory_segments* initial_mem_state, size_t num_perms, uint64_t *valid_hashes){

    int result[total_instrs]; // Result array to store one interleave
    int counts[num_funcs]; // Array to keep track of the counts of each character
    for (int i = 0; i < num_funcs; i++) {
        counts[i] = 0; // Initialize counts as 0
    }
    uint32_t temp_count = 0; // Reset count for the second call
    interleave_and_run(counts, num_instrs, result, &temp_count, num_funcs, total_instrs, 0, ncs + num_funcs, 0, -1, thread_arr, initial_mem_state, num_perms, valid_hashes, false); // Second call to fill the array
}

// runs each interleaving for a given number of instructions
void run_interleavings_as_generated(function_exec* executables, size_t num_funcs, int **itl, size_t num_perms, memory_segments* initial_mem_state, uint64_t *valid_hashes, int ncs) {
    equiv_init();
    uint32_t max_instrs = 0;

    // calculate num instrs for each function
    // via sequential execution
    disable_ctx_switch();
    int* num_instrs = kmalloc(num_funcs * sizeof(int));
    eq_th_t *threads[num_funcs];
    size_t total_instrs = init_threads(threads, executables, itl, num_funcs, num_instrs);
    reset_threads(threads, num_funcs);
    generate_and_run_schedules(num_instrs, total_instrs, num_funcs, ncs, threads, initial_mem_state, num_perms, valid_hashes);
}