#include "interleaver.h"
#include <stdbool.h>
#include "rpi.h"
#include "equiv-malloc.h"
#include "equiv-rw-set.h"

int verbose = 3;

void set_verbosity(int v){
    verbose = v;
}

// NEW

executables, NUM_FUNCS, valid_hashes, interleaved_ncs

// runs each interleaving for a given number of instructions
void run_interleavings(function_exec* executables, size_t num_funcs, uint64_t *valid_hashes, int ncs, set_t *shared_memory) {
    equiv_init();

    disable_ctx_switch();
    eq_th_t *threads[num_funcs];
    init_threads(threads, executables, num_funcs);
    reset_threads(threads, num_funcs);

    uint32_t *tids       = (uint32_t *)equiv_malloc((ncs + 1) * sizeof(uint32_t));
    uint32_t *instr_nums = (uint32_t *)equiv_malloc((ncs)     * sizeof(uint32_t));

    // all zero
    for (int i = 0; i < ncs + 1; i++) {
        tids[i] = 0;
    }
    for (int i = 0; i < ncs; i++) {
        instr_nums[i] = 0;
    }

    set_ctx_switches(tids, instr_nums, ncs);

    // for (int i = 0; i < count; i++) {
    //     tids[i] = (uint32_t *)equiv_malloc(actual_ncs * sizeof(uint32_t));
    //     instr_nums[i] = (uint32_t *)equiv_malloc(actual_ncs * sizeof(uint32_t));
    // }

    // generate schedules
    // generate_schedules(num_instrs, total_instrs, num_funcs, ncs, count, tids, instr_nums);

    // for (int sched_idx = 0; sched_idx < count; sched_idx++) {
    //     reset_memory_state(initial_mem_state);
        
    //     for (int i = 0; i < actual_ncs; i++) {
    //         tids[sched_idx][i] = tids[sched_idx][i];
    //     }
        
    //     if(verbose >= 1){
    //         printk("schedule %d: ", sched_idx);
    //         for (int i = 0; i < actual_ncs; i++) {
    //             printk("(fid: %d", tids[sched_idx][i] - 1);
    //             printk(", instr: %d) ", instr_nums[sched_idx][i]);
    //         }
    //         printk("\n");
    //     }

    //     // load next schedule
    //      
    //     equiv_run();
    //     reset_threads(threads, num_funcs);

    //     uint64_t hash = capture_memory_state(initial_mem_state);
    //     bool valid = false;
    //     for (size_t j = 0; j < num_perms; j++) {
    //         if (hash == valid_hashes[j]) {
    //             valid = true;
    //             break;
    //         }
    //     }
    //     if (valid) {
    //         if(verbose >= 2){
    //             printk("valid state:\n");
    //             print_memstate(initial_mem_state);
    //             printk("\n");
    //             printk("\n");
    //         }
    //     } else {
    //         if(verbose >= 1){
    //             printk("----invalid state detected----\n");
    //             print_memstate(initial_mem_state);
    //             printk("\n");
    //             printk("\n");
    //         }
    //     }
    // }
}

void find_good_hashes(
    function_exec* executables, size_t n_funcs,
    init_memory_func init,
    int** itl, size_t n_perms,
    set_t* shared_memory, set_t* valid_hashes
) {
  if(verbose >= 1){
    printk("Finding valid end states\n");
  }
  for(size_t i = 0; i < n_perms; i++) {
    init();

    // Run (no need for single stepping)
    for (size_t j = 0; j < n_funcs; j++) {
      // run func for this permutation with the corresponding variables
      executables[itl[i][j]].func_addr(executables[itl[i][j]].var_list);
    }

    uint32_t hash = hash_mem(shared_memory);
    if(!set_insert(valid_hashes, hash) && verbose >= 1) {
      print_mem("Valid state found: \n", shared_memory);
      printk("\tPermutation: ");
      for(size_t j = 0; j < n_funcs; j++) {
        printk("%d ", itl[i][j]);
      }
      printk("\n");
    }
  }
}

void find_shared_memory(
    function_exec* executables, size_t n_funcs,
    set_t* shared_memory
) {
  // Allocate read & write sets & compute them
  set_t** read_sets = equiv_malloc(sizeof(set_t*) * n_funcs);
  set_t** write_sets = equiv_malloc(sizeof(set_t*) * n_funcs);
  for(size_t i = 0; i < n_funcs; i++) {
    read_sets[i] = set_alloc();
    write_sets[i] = set_alloc();

    find_rw_set(executables[i].func_addr, read_sets[i], write_sets[i]);

    if(verbose >= 1) {
      printk("Read set #%d\n", i);
      set_print(NULL, read_sets[i]);
      printk("Write set #%d\n", i);
      set_print(NULL, write_sets[i]);
    }
  }


  for(size_t i = 0; i < n_funcs; i++) {
    for(size_t j = 0; j < n_funcs; j++) {
      if(i == j) continue;

      set_t* tmp = set_alloc();

      set_intersection(tmp, read_sets[i], write_sets[j]);
      set_union_inplace(shared_memory, tmp);

      set_free(tmp);
    }
  }

  for(size_t i = 0; i < n_funcs; i++) {
    for(size_t j = i+1; j < n_funcs; j++) {
      set_t* tmp = set_alloc();

      set_intersection(tmp, read_sets[i], write_sets[j]);
      set_union_inplace(shared_memory, tmp);

      set_free(tmp);
    }
  }

  if(verbose >= 1) {
    set_print("Shared memory: \n", shared_memory);
  }
}

void reset_threads(eq_th_t **thread_arr, size_t num_threads){
    for (int i = 0; i < num_threads; i++) {
        equiv_refresh(thread_arr[i]);
    }
}

// init threads from executables and store total num instructions per func in num_instrs 
void init_threads(eq_th_t **thread_arr, function_exec* executables, size_t num_funcs) {
    reset_ntids(); 
    for (size_t f_idx = 0; f_idx < num_funcs; f_idx++) {
        let th = equiv_fork(executables[f_idx].func_addr, NULL, 0);
        thread_arr[f_idx] = th;
    }
}

// OLD

void interleave(int *counts, int *limits, int *result, uint32_t *count, int n, int total_chars, int switches, int ncs, int level, int lastInt, int **interleave_output) {
    if (level == total_chars && switches == ncs) {
        if (interleave_output != NULL) {
            for (int i = 0; i < level; i++) {
                interleave_output[*count][i] = result[i];
            }
            if(verbose >= 3){
                for (int i = 0; i < level; i++) {
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
    if(verbose >= 1){
        printk("schedule: ");
        for (int i = 0; i < actual_ncs; i++) {
            printk("(fid: %d", tids[i] - 1);
            printk(", instr: %d) ", instr_nums[i]);
        }
        printk("\n");
}
    
    reset_memory_state(initial_mem_state);
    set_ctx_switches(tids, instr_nums, actual_ncs); 
    if(verbose >= 5){
        trace("about to equiv run\n");
    }
    equiv_run();
    if(verbose >= 5){
        trace("resetting threads\n");
    }
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
                printk("valid state:\n");
                print_memstate(initial_mem_state);
                printk("\n");
                printk("\n");
            }
        } else {
            *escape = true;
            if(verbose >= 1){
                printk("----invalid state detected----\n");
                print_memstate(initial_mem_state);
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

void run_one_schedule(function_exec* executables, size_t num_funcs, memory_segments* initial_mem_state, int loadstr, uint32_t* tid, uint32_t* instr_nums, uint32_t num_context_switches){
    // we don't support loadstr mode for running one schedule
    assert(loadstr == 0);
    
    equiv_set_load_str_mode(loadstr);
    equiv_init();
    eq_th_t *threads[num_funcs];
    for (size_t f_idx = 0; f_idx < num_funcs; f_idx++) {
        let th = equiv_fork(executables[f_idx].func_addr, NULL, 0);
        threads[f_idx] = th;
        // increment fid by one to convert to tid
        tid[f_idx] = f_idx + 1;
    }
    reset_memory_state(initial_mem_state);
    set_ctx_switches(tid, instr_nums, num_context_switches + num_funcs - 1); 
    equiv_run();
    reset_threads(threads, num_funcs);
    printk("Memory state after running schedule:\n");
    print_memstate(initial_mem_state);
}
