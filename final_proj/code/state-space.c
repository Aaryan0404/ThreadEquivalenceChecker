#include "rpi.h"
#include "state-space.h"
#include "sha256.h"
#include "equiv-threads.h"
#include <stdbool.h>

#define NUM_PTRS 4
#define NUM_FUNCS 2

int* global_var;

void sha256_update_state(SHA256_CTX *ctx, memory_segments* memory_state){
    for(size_t i = 0; i < memory_state->num_ptrs; i++){
        sha256_update(ctx, (BYTE*)(memory_state->ptr_list)[i], memory_state->size_list[i]);
    }
}

// initialize ptr_og_list in memory state with a copy of the values in ptr_list
void initialize_memory_state(memory_segments* memory_state) {
    void** new_ptr_list = kmalloc(memory_state->num_ptrs * sizeof(void *));
    for (int i = 0; i < memory_state->num_ptrs; i++) {
        void *var_addr = kmalloc(memory_state->size_list[i]);
        memcpy(var_addr, memory_state->ptr_list[i], memory_state->size_list[i]);
        new_ptr_list[i] = var_addr;
    }
    memory_state->ptr_og_list = new_ptr_list;
}

// reset ptr_list in memory state with a copy of the values in ptr_og_list
void reset_memory_state(memory_segments* memory_state) {
    for (int i = 0; i < memory_state->num_ptrs; i++) {
        void *ptr = memory_state->ptr_list[i];
        void *ptr_og = memory_state->ptr_og_list[i];
        memcpy(ptr, ptr_og, memory_state->size_list[i]);
    }
}

// get the hash of the bytes in the memory state
uint32_t capture_memory_state(memory_segments* memory_state){
    SHA256_CTX ctx;
    BYTE buf[SHA256_BLOCK_SIZE];
    sha256_init(&ctx);
    sha256_update_state(&ctx, memory_state);
    sha256_final(&ctx, buf);
    uint32_t trunc_hash = *((uint32_t*)buf);
    return trunc_hash;
}

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

void run_interleavings(function_exec* executables, size_t num_funcs, int **itl, size_t num_perms, memory_segments* initial_mem_state, uint64_t *valid_hashes) {
    equiv_init();
    uint32_t max_instrs = 0;
    for (size_t i = 1; i <= 16384; i++) {
        // halt early once we reach a sequential ordering
        if(max_instrs != 0 && i > max_instrs){
            break;
        }
        reset_memory_state(initial_mem_state);
        eq_th_t *threads[num_funcs];
        for (size_t f_idx = 0; f_idx < num_funcs; f_idx++) {
            threads[f_idx] = equiv_fork(executables[itl[0][f_idx]].func_addr, NULL, 0);
        }

        // thread tid will run for i instructions, and then context switch.
        uint32_t tid = 2;
        tid = 2 + (i - 1) * (num_funcs);
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
            printk("valid, global var: %d\n", *global_var); 
        } else {
            printk("invalid, global var: %d\n", *global_var);
        }
    }
}

void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void permute(int **itl, int *arr, int start, int end, int *count) {
    if (start == end) {
        for (int i = 0; i <= end; i++) {
            itl[*count][i] = arr[i];
        }
        (*count)++;
    } else {
        for (int i = start; i <= end; i++) {
            swap(&arr[start], &arr[i]);
            permute(itl, arr, start + 1, end, count);
            swap(&arr[start], &arr[i]); // backtrack
        }
    }
}

int factorial(int n) {
    if (n == 0) return 1;
    int fact = 1;
    for (int i = 1; i <= n; i++) {
        fact *= i;
    }
    return fact;
}

void find_permutations(int **itl, int num_funcs) {
    int *arr = kmalloc(num_funcs * sizeof(int)); 
    for (int i = 0; i < num_funcs; i++) {
        arr[i] = i;
    }

    int count = 0;
    permute(itl, arr, 0, num_funcs - 1, &count);
}

// USER CODE

void funcMA(void **arg) {
    // multiply by 4 and add 1
    int a = *global_var;
    a += 1; 
    *global_var = a;   
}

void funcMS(void **arg) {
    // subtracts 1 from global var a
    int a = *global_var;
    a *= 2;
    *global_var = a;
}

void notmain() {

    // arbitrary number of global vars, 
    // wrapped in initial_mem_state struct
        // will contain actual memory (to be modified)
        // will contain copy of original memory
    global_var = kmalloc(sizeof(int));
    *global_var = 5;

    size_t sizes[1] = {sizeof(int)};

    memory_segments initial_mem_state = {1, (void **)&global_var, NULL, sizes}; 
    initialize_memory_state(&initial_mem_state);

    // print statement that shows value of 
    // initial memory state
    printk("global var: %d\n", *global_var);


    // dependeing on num functions, generate
    // a 2d array of function interleavings (sequential outcomes)
    // itl[0] = {0, 1}
    // itl[1] = {1, 0}
    size_t num_funcs = 2;
    size_t num_perms = factorial(num_funcs);
    int **itl = kmalloc(num_perms * sizeof(int *));
    for (int i = 0; i < num_perms; i++) {
        itl[i] = kmalloc(num_funcs * sizeof(int));
    }
    find_permutations(itl, num_funcs);

    // create an array (set) of memory hashes
    uint64_t valid_hashes[num_perms];

    function_exec *executables = kmalloc(num_funcs * sizeof(function_exec));

    executables[0].func_addr = (func_ptr)funcMA;
    executables[0].num_vars = 0; 
    executables[0].var_list = NULL;

    executables[1].func_addr = (func_ptr)funcMS;
    executables[1].num_vars = 0; 
    executables[1].var_list = NULL;

    find_good_hashes(executables, num_funcs, itl, num_perms, &initial_mem_state, valid_hashes);
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

    run_interleavings(executables, num_funcs, itl, num_perms, &initial_mem_state, valid_hashes);
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
    // equiv run
    // equiv_init();
    // eq_th_t* threads[2]; 

    // // args is null
    // void *args[0];

    // threads[0] = equiv_fork(funcMA, args, 0);
    // threads[1] = equiv_fork(funcMS, args, 0);

    // printk("thread 0: %d\n", threads[0]); 

    // equiv_run();

    // printk("global var: %d\n", *global_var);
}

    // memory_segments initial_mem_state = {1, (void **)&global_var, (void **)&global_var, (size_t *)&global_var};


    // printk("Hello, world!\n"); 
    // char *msg_1  = "A1\n";
    // char *msg_2  = "B2\n";
    // char *msg_3  = "A3\n";
    // char *msg_4  = "B4\n";
    // void *ptr_list[NUM_PTRS] = {msg_1, msg_2, msg_3, msg_4};

    // size_t size_list[NUM_PTRS] = {strlen(msg_1), strlen(msg_2), strlen(msg_3), strlen(msg_4)};

    // memory_segments initial_mem_state = {NUM_PTRS, ptr_list, ptr_list, size_list};
    // void (*functions[NUM_FUNCS])(void**) = {msgA, msgB};
    // // initialize a 2d boolean array signifying which elements in ptr_list are arguments for functions msgA and msgB
    // // initialize the array as all false
    // bool arg_map[NUM_FUNCS][NUM_PTRS];
    // for (int i = 0; i < NUM_FUNCS; i++) {
    //     for (int j = 0; j < NUM_PTRS; j++) {
    //         arg_map[i][j] = false;
    //     }
    // }

    // // set the elements in ptr_list that are arguments for msgA to true
    // arg_map[0][0] = true;
    // arg_map[0][2] = true;

    // // set the elements in ptr_list that are arguments for msgB to true
    // arg_map[1][1] = true;
    // arg_map[1][3] = true;

    // get_function_interleavings(functions, initial_mem_state, NUM_FUNCS, arg_map);
