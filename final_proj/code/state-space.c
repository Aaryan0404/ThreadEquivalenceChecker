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

int a = 1;
int b = 2;
int c = 3;

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

void sys_equiv_putc(uint8_t ch);

void equiv_puts(char *msg) {
    for(; *msg; msg++)
        sys_equiv_putc(*msg);
}

void msgA(void **args) {
    void *msg1 = (char *)args[0];
    void *msg2 = (char *)args[1];
    // concat A to msg1
    char * to_print = "A0";
    // strcat(to_print, msg1);
    // printk("to_print: %s\n", to_print);
    printk("msg1: %s\n", msg1);
    // strcat(to_print, msg1);
    // strcpy(msg1, "overwritten\n");
    // printk("to_print after: %s\n", to_print);

    // print  msg1 and msg2
    // printk("A msg1: %s\n", msg1);
    // printk("A msg2: %s\n", msg2);

    // equiv_puts(to_print);
    // equiv_puts(msg2);
    // equiv_puts("A1\n");
    // equiv_puts("A3\n");
    // equiv_puts("A5\n");
    // equiv_puts("A7\n");
    // equiv_puts("A9\n");
}

void msgB(void **args) {
    void *msg1 = (char *)args[0];
    void *msg2 = (char *)args[1];
    char * to_print = "B0";
    // strcat(to_print, msg1);
    // printk("to_print: %s\n", to_print);
    printk("msg1: %s\n", msg1);
    // strcat(to_print, msg1);
    // overwrite msg1 to say "overwritten"
    // strcpy(msg1, "overwritten\n");
    // printk("to_print after: %s\n", to_print);
    // prinkt("msg1: %s\n", msg1);
    // print start address of msg1 and msg2
    // printk("B msg1: %s\n", msg1);
    // printk("B msg2: %s\n", msg2);

    // equiv_puts(to_print);
    // equiv_puts(msg2);
    // equiv_puts("B1\n");
    // equiv_puts("B3\n");
    // equiv_puts("B5\n");
    // equiv_puts("B7\n");
    // equiv_puts("B9\n");
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
    // print out the new ptr_list
    // for (int i = 0; i < initial_mem_state.num_ptrs; i++) {
    //     printk("ptr_list: %x\n", initial_mem_state.ptr_list[i]);
    // }
    // printk("str1: %s\n", (char *)initial_mem_state.ptr_list[0]);
    // printk("str2: %s\n", (char *)initial_mem_state.ptr_list[1]);
    // char* strmsgA = "A\n";
    // char* strmsgB = "B\n";
    // let th1 = equiv_fork(functions[0], (void**)&strmsgA, 0);
    // let th2 = equiv_fork(functions[1], (void**)&strmsgB, 0);
    // create array of threads, one for each function

    // print arg map
    // for (int i = 0; i < num_funcs; i++) {
    //     for (int j = 0; j < initial_mem_state.num_ptrs; j++) {
    //         printk("arg_map[%d][%d]: %d\n", i, j, arg_map[i][j]);
    //     }
    // }

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
        functions[i](args);
        // threads[i] = equiv_fork(functions[i], args, 0);
    }
    // equiv_run();
    // printk("IGNORE: %d\n", threads[0]->tid);
    printk("str1: %s\n", (char *)initial_mem_state.ptr_list[0]);
    printk("str2: %s\n", (char *)initial_mem_state.ptr_list[1]);
    printk("str3: %s\n", (char *)initial_mem_state.ptr_list[2]);
    printk("str4: %s\n", (char *)initial_mem_state.ptr_list[3]);
    // run 2
    // equiv_refresh(th1);
    // equiv_refresh(th2);
    // equiv_run();
    // let th1 = launch_thread(functions[0], memory_state, num_states, 1, num_states);
    // let th2 = launch_thread(functions[1], memory_state, num_states, 1, num_states);
}


// USER CODE
void funcA(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // read set = x, y
    // write set = z

    // read from x
    int x_val = *x;

    // read from y
    int y_val = *y;

    // set z to x + y
    *z = x_val + y_val;
}

void funcB(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // read set = y
    // write set = x, z

    // read from y
    int y_val = *y;

    // set x to y + 1
    *x = y_val + 1;

    // set z to y - 1
    *z = y_val - 1;
}

void func0(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // set y[0] to 1
    *y = 1;

    // set y[1] to 2
    *(y + 1) = 2;
}

void func1(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // if y[0] is 1, set x to 2
    if (*y == 1) {
        *x = 2;
    }
}

void func2(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // if y[1] is 2, set x to 3
    if (*(y + 1) == 2) {
        *x = 3;
    }
}

void func3(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // set z to 1
    *z = 1;
}

void func4(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // increment z
    *z += 1;
}

void func5(void **args) {
    int *x = (int *)args[0];
    int *y = (int *)args[1];
    int *z = (int *)args[2];

    // multiply z by 2
    *z *= 2;
}

void notmain() {

    printk("Hello, world!\n"); 
    char *msg_1  = "A1\n";
    char *msg_2  = "B2\n";
    char *msg_3  = "A3\n";
    char *msg_4  = "B4\n";
    void *ptr_list[NUM_PTRS] = {msg_1, msg_2, msg_3, msg_4};

    size_t size_list[NUM_PTRS] = {strlen(msg_1), strlen(msg_2), strlen(msg_3), strlen(msg_4)};

    memory_segments initial_mem_state = {NUM_PTRS, ptr_list, ptr_list, size_list};
    void (*functions[NUM_FUNCS])(void**) = {msgA, msgB};
    // initialize a 2d boolean array signifying which elements in ptr_list are arguments for functions msgA and msgB
    // initialize the array as all false
    bool arg_map[NUM_FUNCS][NUM_PTRS];
    for (int i = 0; i < NUM_FUNCS; i++) {
        for (int j = 0; j < NUM_PTRS; j++) {
            arg_map[i][j] = false;
        }
    }

    // set the elements in ptr_list that are arguments for msgA to true
    arg_map[0][0] = true;
    arg_map[0][2] = true;

    // set the elements in ptr_list that are arguments for msgB to true
    arg_map[1][1] = true;
    arg_map[1][3] = true;

    get_function_interleavings(functions, initial_mem_state, NUM_FUNCS, arg_map);

    // // array of function ptrs
    // void (*functions[6])(void**) = {func0, func1, func2, func3, func4, func5};
    // // initial state and then store in struct
    // int x = 1;
    // int y[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    // int z = 2; 

    // variable_state states[3] = {{&x, sizeof(int)}, 
    //                             {&y[0], 10 * sizeof(int)}, 
    //                             {&z, sizeof(int)}};

    // // run the function
    // get_sequential_state_outcomes(functions, states, 6, 3);
}