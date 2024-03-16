#include "rpi.h"
#include "state-space.h"
#include "sha256.h"
#include "equiv-threads.h"
#include <stdbool.h>

#define NUM_PTRS 4
#define NUM_FUNCS 2

void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void sha256_update_state(SHA256_CTX *ctx, memory_segments var_states){
    for(size_t i = 0; i < var_states.num_ptrs; i++){
        sha256_update(ctx, (BYTE*)(var_states.ptr_list)[i], var_states.size_list[i]);
    }
}

void generate_permutations(int *arr, int start, int end, int np, int num_funcs, int permutations[][num_funcs], int *perm_idx) {
    if (start == end) {
        for (int i = 0; i < num_funcs; i++) {
            permutations[*perm_idx][i] = arr[i];
        }
        (*perm_idx)++;
        return;
    }

    for (int i = start; i <= end; i++) {
        swap(&arr[start], &arr[i]);
        generate_permutations(arr, start + 1, end, np, num_funcs, permutations, perm_idx);
        swap(&arr[start], &arr[i]); // Backtrack
    }
}



// void get_sequential_state_outcomes(function_exec* executables, size_t num_executables, size_t num_states, memory_segments* initial_mem_state) {
//     // all permutations of the functions
//     int np = 1; 
//     for (int i = 1; i <= num_executables; i++) {
//         np *= i; 
//     }

//     int *sequence = kmalloc(num_executables * sizeof(int));
//     for (int i = 0; i < num_funcs; i++) {
//         sequence[i] = i;
//     }

//     // create a 2d array
//     // dim 0 = idx of permutation (from 0 to num_permutations - 1)
//     // dim 1 = actual permutation of function idxs
//     int permutations[np][num_executables];
//     int perm_idx = 0;
//     generate_permutations(sequence, 0, num_funcs - 1, np, num_funcs, permutations, &perm_idx);

//     uint32_t hash[np]; 

//     // go through all perms
//     for (int p_idx = 0; p_idx < np; p_idx++) {
//         // retrieve sequence of function idxs
//         int *sequence = permutations[p_idx];

//        // set value of ptr_list to ptr_og_list
//        for (int i = 0; i < initial_mem_state->num_ptrs; i++) {
//            void *ptr = initial_mem_state->ptr_list[i];
//            void *ptr_og = initial_mem_state->ptr_og_list[i];
//            memcpy(ptr, ptr_og, initial_mem_state->size_list[i]);
//        }
        
//         for (int f_idx = 0; f_idx < num_funcs; f_idx++) {
//             functions[sequence[f_idx]](variables);
//             // printk("func: %d, x: %d, y[0]: %d, y[1]: %d, z: %d\n", sequence[f_idx], *(int *)variables[0], *(int *)variables[1], *((int *)variables[1] + 1), *(int *)variables[2]);
//         }
//         SHA256_CTX ctx;
//         BYTE buf[SHA256_BLOCK_SIZE];
//         sha256_init(&ctx);
//         sha256_update_state(&ctx, initial_mem_state);
//         sha256_final(&ctx, buf);
//         uint32_t trunc_hash = *((uint32_t*)buf);
//         hash[p_idx] = trunc_hash;
//         // printk("hash: %x\n", trunc_hash);
//         // printk("\n");

//     }

//     // print out the hash values
//     for (int i = 0; i < np; i++) {
//         printk("hash: %x\n", hash[i]);
//     }
// }

// static eq_th_t* launch_thread(void (*f)(void**), void **args, int verbose_p, size_t num_states) {
//     void* variables[num_states];
//     let th = equiv_fork(functions[i], variables, 0); 
//     th->verbose_p = verbose_p;

//     equiv_run();
// }

// reset ptr_list in memory state with a copy of the values in ptr_og_list
void duplicate_memory_state(memory_segments memory_state) {
    void** new_ptr_list = kmalloc(memory_state.num_ptrs * sizeof(void *));
    for (int i = 0; i < memory_state.num_ptrs; i++) {
        void *var_addr = kmalloc(memory_state.size_list[i]);
        memcpy(var_addr, memory_state.ptr_og_list[i], memory_state.size_list[i]);
        new_ptr_list[i] = var_addr;
        // printk("new_ptr_list: %x\n", new_ptr_list[i]);
        // printk("old str: %s\n", (char *)memory_state.ptr_og_list[i]);
        // printk("new str: %s\n", (char *)new_ptr_list[i]);
    }
    memory_state.ptr_list = new_ptr_list;
    
}

size_t get_num_func_args(bool arg_map[][NUM_PTRS], size_t func_idx, size_t total_args) {
    size_t num_args = 0;
    for(int j = 0; j < total_args; j++) {
        if (arg_map[func_idx][j]) {
            num_args++;
        }
    }
    return num_args;
}

void get_function_interleavings(void (*functions[])(void**), memory_segments initial_mem_state, size_t num_funcs, bool arg_map[][NUM_PTRS]) {
    equiv_init();
    // duplicate memory state here and we'll use those for the variables
    duplicate_memory_state(initial_mem_state);

    eq_th_t *threads[num_funcs];
    for (size_t i = 0; i < num_funcs; i++) {
        printk("doing iteration %d\n", i);
        size_t num_func_args = get_num_func_args(arg_map, i, initial_mem_state.num_ptrs);
        void *args[num_func_args];
        size_t arg_num = 0;
        for (int j = 0; j < initial_mem_state.num_ptrs; j++) {
            if (arg_map[i][j]) {
                args[arg_num] = initial_mem_state.ptr_list[j];
                // args will be an array of char *, print each value
                // printk("arg val %s for val %d\n", (char *)args[arg_num], i);
                arg_num++;
            
            }
        }
        // functions[i](args);
        threads[i] = equiv_fork(functions[i], args, 0);
    }
    equiv_run();
    printk("IGNORE: %d\n", threads[0]->tid);
    // printk("str1: %s\n", (char *)initial_mem_state.ptr_list[0]);
    // printk("str2: %s\n", (char *)initial_mem_state.ptr_list[1]);
    // printk("str3: %s\n", (char *)initial_mem_state.ptr_list[2]);
    // printk("str4: %s\n", (char *)initial_mem_state.ptr_list[3]);
}


// USER CODE
int* global_var;

void funcMA() {
    // multiply by 4 and add 1
    int a = *global_var;
    a += 1; 
    *global_var = a;   
}

void funcMS() {
    // subtracts 1 from global var a
    int a = *global_var;
    a *= 2;
    *global_var = a;
}

void notmain() {

    // arbitrary number of global vars, 
    // wrapped in initial_mem_state struct
    global_var = kmalloc(sizeof(int));
    *global_var = 5;

    // print statement that shows value of 
    // initial memory state
    printk("global var: %d\n", *global_var);

    equiv_init();
    eq_th_t* threads[2]; 

    // args is null
    void *args[0];

    threads[0] = equiv_fork(funcMA, args, 0);
    threads[1] = equiv_fork(funcMS, args, 0);

    printk("thread 0: %d\n", threads[0]); 

    equiv_run();

    printk("global var: %d\n", *global_var);
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
