// function that takes in n function pointers and n states to keep track of


// given n threads and n functions and an initial state, 
// run n! permutations of the functions and store end 
// state in a set 




#include "sha256.h"


// int x
// struct queue *q
// int y[10]
// [&x, q, y]

// save (states[0]) ... (states[0] + nybtes[0])

// states[i] = [ptr to variable, size of variable]: make struct for this
/*
funcs: array of func pointers
states: array of state_data_t consisting of state we want to save and number of bytes we'd save
num_funcs: number of functions
num_states: number of states we are keeping track of
*/

void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
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

void get_sequential_state_outcomes(void** funcs, variable_state* memory_state, size_t num_funcs, size_t num_states) {
    // all permutations of the functions
    int np = 1; 
    for (int i = 1; i <= num_funcs; i++) {
        np *= i; 
    }

    int *sequence = malloc(num_funcs * sizeof(int));
    for (int i = 0; i < num_funcs; i++) {
        sequence[i] = i;
    }

    // create a 2d array
    // dim 0 = idx of permutation (from 0 to num_permutations - 1)
    // dim 1 = actual permutation of function idxs
    int permutations[np][num_funcs];
    int perm_idx = 0;
    generate_permutations(sequence, 0, num_funcs - 1, np, num_funcs, permutations, &perm_idx);
    
    for (int i = 0; i < np; i++) {
        for (int j = 0; j < num_funcs; j++) {
            printk("%d ", permutations[i][j]);
        }
        printk("\n");
    }

    free(sequence);

    return 0;

    // array of hashes of final states
    uint64_t final_states[np];

    // generate num_permutations of possible final states
    // state_data_t possible_final_states[num_permutations][num_states];

    // for (int i = 0; i < num_permutations; i++) {
    //     possible_final_states[i] = {0};

    //     SHA256_CTX ctx;
    //     BYTE buf[SHA256_BLOCK_SIZE];
    //     sha256_init(&ctx);
    //     sha256_update(&ctx, possible_final_states[i]);
    //     sha256_final(&ctx, buf);
    //     // truncate to 64 bits
    //     final_states[i] = *((uint64_t*)buf);

    // }
    
}

void func1(int *x) {
    *x += 1; 
}

void func2(int *x) {
    *x += 1; 
}

void func3(int *x) {
    *x += 1; 
}

void func4(int *x) {
    *x += 1; 
}

void notmain() {
    
    // array of function ptrs
    void *(*functions[4])(void *) = {func1, func2, func3, func4};

    // initial state and then store in struct
    int x = 0;
    state_data_t states[1] = {&x, sizeof(int)};

    // run the function
    get_sequential_state_outcomes(functions, states, 4, 1);
}