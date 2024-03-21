#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"

#define MAX_STACK_SIZE 100
#define NUM_VARS 2
#define NUM_FUNCS 2

typedef int vibe_check_t;

// Assembly function to try acquiring the lock
extern int vibe_check(vibe_check_t *cur_vibes);
extern void secure_vibes(vibe_check_t *cur_vibes);
extern void release_vibes(vibe_check_t *cur_vibes);

vibe_check_t cur_vibes;

void vibe_init(vibe_check_t *cur_vibes) {
    *cur_vibes = 0; // 0 indicates that the lock is available
}

typedef struct {
    int data[MAX_STACK_SIZE];
    int top;
    vibe_check_t lock;
} AtomicStack;

AtomicStack stack;
int *global_value;  // Global variable for the value

void stack_init(AtomicStack *stack) {
    stack->top = -1;
    vibe_init(&stack->lock);
    // Initialize the stack with values from 0 to 99
    for (int i = 0; i < MAX_STACK_SIZE; i++) {
        stack->data[++stack->top] = i;
    }
}

int stack_pop(AtomicStack *stack, int *global_value) {
    secure_vibes(&stack->lock);
    if (stack->top < 0) {
        release_vibes(&stack->lock);
        return -1; // Stack underflow
    }
    *global_value = stack->data[stack->top];
    stack->top -= 1;
    release_vibes(&stack->lock);
    return 0;
}

// Function A: Pop from stack and update global_value
void funcA(void **arg) {
    stack_pop(&stack, global_value);
}

// Function B: Pop from stack and update global_value (similar to funcA)
void funcB(void **arg) {
    stack_pop(&stack, global_value);
}

void funcA_bad(void **arg) {
    if (stack.top < 0) {
        return;
    }
    *global_value = stack.data[stack.top];
    stack.top -= 1;
}

void funcB_bad(void **arg) {
    if (stack.top < 0) {
        return;
    }
    *global_value = stack.data[stack.top];
    stack.top -= 1;
}

void notmain() {
    stack_init(&stack);
    vibe_init(&cur_vibes);
    
    global_value = kmalloc(sizeof(int));
    *global_value = 0;

    int interleaved_ncs = 2;

    function_exec executables[NUM_FUNCS];
    executables[0].func_addr = (func_ptr)funcA;
    executables[0].num_vars = 0;
    executables[0].var_list = NULL;

    executables[1].func_addr = (func_ptr)funcB;
    executables[1].num_vars = 0;
    executables[1].var_list = NULL;

    // function_exec executables[NUM_FUNCS];
    // executables[0].func_addr = (func_ptr)funcA_bad;
    // executables[0].num_vars = 0;
    // executables[0].var_list = NULL;

    // executables[1].func_addr = (func_ptr)funcB_bad;
    // executables[1].num_vars = 0;
    // executables[1].var_list = NULL;

    const size_t num_perms = factorial(NUM_FUNCS);
    int **itl = kmalloc(num_perms * sizeof(int *));
    for (int i = 0; i < num_perms; i++) {
        itl[i] = kmalloc(NUM_FUNCS * sizeof(int));
    }
    find_permutations(itl, NUM_FUNCS);

    uint64_t valid_hashes[num_perms];

    // Initialize memory state
    void *global_vars[NUM_VARS] = {(void *)global_value, (void *)&stack};
    size_t sizes[NUM_VARS] = {sizeof(int), sizeof(int) * (MAX_STACK_SIZE + 1)}; 
    
    memory_segments initial_mem_state = {NUM_VARS, (void **)global_vars, NULL, sizes};
    initialize_memory_state(&initial_mem_state);

    find_good_hashes(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes);

    int load_store_mode = 1;
    // run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
    run_interleavings_as_generated(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
}
