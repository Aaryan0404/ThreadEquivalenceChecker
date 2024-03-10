#include "rpi.h"
#include "state-space.h"
#include "sha256.h"

void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void sha256_update_state(SHA256_CTX *ctx, variable_state* var_states){
    size_t num_vars = 3;
    for(size_t i = 0; i < num_vars; i++){
        sha256_update(ctx, (BYTE*)var_states[i].var_addr, var_states[i].num_bytes);
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

void duplicate_memory_state(variable_state* memory_state, size_t num_states, variable_state* new_state, void* variables[]) {
    for (int i = 0; i < num_states; i++) {        
        void *var_addr = kmalloc(memory_state[i].num_bytes);
        memcpy(var_addr, memory_state[i].var_addr, memory_state[i].num_bytes);

        new_state[i].num_bytes = memory_state[i].num_bytes;
        new_state[i].var_addr = var_addr;

        variables[i] = var_addr;
    }
}

void get_sequential_state_outcomes(void (*functions[])(void**), variable_state* memory_state, size_t num_funcs, size_t num_states) {
    // all permutations of the functions
    int np = 1; 
    for (int i = 1; i <= num_funcs; i++) {
        np *= i; 
    }

    int *sequence = kmalloc(num_funcs * sizeof(int));
    for (int i = 0; i < num_funcs; i++) {
        sequence[i] = i;
    }

    // create a 2d array
    // dim 0 = idx of permutation (from 0 to num_permutations - 1)
    // dim 1 = actual permutation of function idxs
    int permutations[np][num_funcs];
    int perm_idx = 0;
    generate_permutations(sequence, 0, num_funcs - 1, np, num_funcs, permutations, &perm_idx);

    uint32_t hash[np]; 

    // go through all perms
    for (int p_idx = 0; p_idx < np; p_idx++) {
        // retrieve sequence of function idxs
        int *sequence = permutations[p_idx];

        variable_state initial_mem_state[num_states]; 
        void* variables[num_states];
        duplicate_memory_state(memory_state, num_states, initial_mem_state, variables);
        
        for (int f_idx = 0; f_idx < num_funcs; f_idx++) {
            functions[sequence[f_idx]](variables);
            // printk("func: %d, x: %d, y[0]: %d, y[1]: %d, z: %d\n", sequence[f_idx], *(int *)variables[0], *(int *)variables[1], *((int *)variables[1] + 1), *(int *)variables[2]);
        }
        SHA256_CTX ctx;
        BYTE buf[SHA256_BLOCK_SIZE];
        sha256_init(&ctx);
        sha256_update_state(&ctx, initial_mem_state);
        sha256_final(&ctx, buf);
        uint32_t trunc_hash = *((uint32_t*)buf);
        hash[p_idx] = trunc_hash;
        // printk("hash: %x\n", trunc_hash);
        // printk("\n");

    }

    // print out the hash values
    for (int i = 0; i < np; i++) {
        printk("hash: %x\n", hash[i]);
    }
}


// USER CODE
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
    
    // array of function ptrs
    void (*functions[6])(void**) = {func0, func1, func2, func3, func4, func5};
    // initial state and then store in struct
    int x = 1;
    int y[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int z = 2; 

    variable_state states[3] = {{&x, sizeof(int)}, 
                                {&y[0], 10 * sizeof(int)}, 
                                {&z, sizeof(int)}};

    // run the function
    get_sequential_state_outcomes(functions, states, 6, 3);
}