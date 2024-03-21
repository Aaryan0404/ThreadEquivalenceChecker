#include "equiv-threads.h"
#include "memory.h"

typedef void (*func_ptr)(void**);

typedef struct {
    func_ptr func_addr;  // Pointer to the function
    size_t num_vars;    // Number of variables in the function
    void *var_list; // Pointer to the list of variables
} function_exec; 

void reset_threads(eq_th_t **thread_arr, size_t num_threads);
size_t init_threads(eq_th_t **thread_arr, function_exec* executables, int **itl, size_t num_funcs, int *num_instrs);

void find_good_hashes(function_exec* executables, size_t num_funcs, int **itl, size_t num_perms, memory_segments* initial_mem_state, uint64_t *valid_hashes);

// functions to generate schedules as a batch, and then run them later
uint32_t get_num_schedules(int *num_instrs, size_t total_instrs, size_t num_funcs, int ncs);
void interleave(int *counts, int *limits, int *result, uint32_t *count, int n, int total_chars, int switches, int ncs, int level, int lastInt, int **interleave_output);
void generate_schedules(int *num_instrs, size_t total_instrs, size_t num_funcs, int ncs, uint32_t count, uint32_t **tids, uint32_t **instr_nums);
void run_interleavings(function_exec* executables, size_t num_funcs, int **itl, size_t num_perms, memory_segments* initial_mem_state, uint64_t *valid_hashes, int ncs, int loadstr);

// functions to run schedules as we generate them
void run_schedule(uint32_t total_instrs, int num_funcs, int ncs, int *raw_sched, eq_th_t **thread_arr, memory_segments* initial_mem_state);
void interleave_and_run(int *counts, int *limits, int *result, uint32_t *count, int num_funcs, int total_chars, int switches, int ncs, int level, int lastInt, eq_th_t **thread_arr, memory_segments* initial_mem_state, size_t num_perms, uint64_t *valid_hashes, bool* escape);
void generate_and_run_schedules(int *num_instrs, size_t total_instrs, size_t num_funcs, int ncs, eq_th_t **thread_arr, memory_segments* initial_mem_state, size_t num_perms, uint64_t *valid_hashes);
void run_interleavings_as_generated(function_exec* executables, size_t num_funcs, int **itl, size_t num_perms, memory_segments* initial_mem_state, uint64_t *valid_hashes, int ncs, int loadstr);