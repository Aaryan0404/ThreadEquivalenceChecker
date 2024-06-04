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

// runs each interleaving for a given number of instructions

uint32_t tids_valid(uint32_t* tids, uint32_t ncs) {
  for(int i = 0; i < ncs - 1; i++) {
    if(tids[i] == tids[i+1])
      return 0;
  }
  return 1;
}

uint32_t next_tid(uint32_t* tids, uint32_t ncs, uint32_t num_funcs) {
  do {
    uint32_t carry = 0;
    for(int i = 0; i < ncs + 1; i++) {
      // Increment
      if(i == 0) {
        tids[0]++;
      }

      if(carry) {	
        tids[i]++;
        carry = 0;	
      }

      if(tids[i] > num_funcs) {
        tids[i] = 1;
        carry = 1;
      }
    }
    if(carry) {
      return 0;
    }
  } while(!tids_valid(tids, ncs + 1));
  return 1;
}

void run_interleavings(
  function_exec* executables,size_t num_funcs,
  set_t *valid_hashes,
  init_memory_func init,
  int ncs,
  set_t *shared_memory,
  memory_tags_t* tags
) {
    equiv_init();

    disable_ctx_switch();
    eq_th_t *threads[num_funcs];
    init_threads(threads, executables, num_funcs);
    reset_threads(threads, num_funcs);

    uint32_t *tids       = (uint32_t *)equiv_malloc((ncs + 1) * sizeof(uint32_t));
    uint32_t *instr_nums = (uint32_t *)equiv_malloc((ncs)     * sizeof(uint32_t));

    // Generate an initial thread ordering
    for (int i = 0; i < ncs + 1; i++) {
        tids[i] = 1;
    }
    next_tid(tids, ncs, num_funcs);

    // Generate an initial instruction count set
    for (int i = 0; i < ncs; i++) {
        instr_nums[i] = 1;
    }

    uint32_t done = 0;
    schedule_t schedule = {
      .tids = tids,
      .instr_counts = instr_nums,
      .n_ctx_switches = ncs,
      .n_funcs = num_funcs
    };
    while(!done) {
      schedule_report_t* report = NULL;
      if(verbose >= 3) {
        // Setup of schedule report
        report = equiv_malloc(sizeof(schedule_report_t*));
        report->pcs = equiv_malloc(sizeof(uint32_t*) * schedule.n_ctx_switches);
        for(int i = 0; i < schedule.n_ctx_switches; i++)
          report->pcs[i] = equiv_malloc(sizeof(uint32_t) * schedule.instr_counts[i]);
        
      }
      schedule.report = report;

      init();
      reset_threads(threads, num_funcs);
      set_memory_touch_handler(ctx_switch_handler);
      enable_ctx_switch(&schedule, shared_memory);
      rw_tracker_enable();

      equiv_run();

      let status = get_ctx_switch_status();
      
      rw_tracker_disable();
      disable_ctx_switch();

      if(status.yielded) {
        if(verbose >= 3) {
          print_schedule("Schedule yielded \n", &schedule);
        }
      }
      
      if(!status.yielded && status.ctx_switch == ncs) {
        // Happy state, schedule was valid
        uint32_t hash = hash_mem(shared_memory);

        if(!set_lookup(valid_hashes, hash)) {
          if(verbose >= 1) {
            print_mem_tags("\nInvalid memory state detected\n", shared_memory, tags);
            print_schedule("With schedule \n", &schedule);
          }
        } else {
          if(verbose >= 3) {
            print_mem_tags("\nValid memory state detected\n", shared_memory, tags);
            print_schedule("With schedule \n", &schedule);
          }
        }
      }

      if(verbose >= 3) {
        // Free schedule report
        for(int i = 0; i < schedule.n_ctx_switches; i++)
          equiv_free(report->pcs[i]);
        equiv_free(report->pcs);
        equiv_free(report);
      }

      // Advance instruction numbers
      if(status.ctx_switch > 0) {
        instr_nums[status.ctx_switch - 1]++;
        if(status.ctx_switch < ncs) {
          for(int i = status.ctx_switch; i < ncs; i++)
            instr_nums[i] = 1;
        }
      }
      // Advance TIDs
      else {
        for(int i = 0; i < ncs; i++) instr_nums[i] = 1;
        if(!next_tid(tids, ncs, num_funcs)) done = 1;
      }
    }
}

void find_good_hashes(
    function_exec* executables, size_t n_funcs,
    init_memory_func init,
    int** itl, size_t n_perms,
    set_t* shared_memory, set_t* valid_hashes
) {
  if(verbose >= 3){
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
    if(!set_insert(valid_hashes, hash) && verbose >= 3) {
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

    if(verbose >= 3) {
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

  /*for(int i = 0; i < n_funcs; i++) {
    set_free(read_sets[i]);
    set_free(write_sets[i]);
  }*/

  if(verbose >= 1) {
    set_print("Automagically found shared memory: \n", shared_memory);
  }
}

void reset_threads(eq_th_t **thread_arr, size_t num_threads){
    for (int i = 0; i < num_threads; i++) {
        equiv_refresh(thread_arr[i]);
    }
}

// init threads from executables and store total num instructions per func in num_instrs 
void init_threads(
  eq_th_t **thread_arr,
  function_exec* executables,
  size_t num_funcs
) {
    reset_ntids(); 
    for (size_t f_idx = 0; f_idx < num_funcs; f_idx++) {
        let th = equiv_fork(executables[f_idx].func_addr, NULL, 0);
        thread_arr[f_idx] = th;
    }
}
