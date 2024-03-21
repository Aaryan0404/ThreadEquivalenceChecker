#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"

#define MAX_STACK_SIZE 100
#define NUM_VARS 1
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
}

int stack_push(AtomicStack *stack, int value) {
    secure_vibes(&stack->lock);
    if (stack->top >= MAX_STACK_SIZE - 1) {
        release_vibes(&stack->lock);
        return -1; // Stack overflow
    }
    stack->top += 1;
    stack->data[stack->top] = value;
    release_vibes(&stack->lock);
    return 0;
}

int stack_pop(AtomicStack *stack, int *value) {
    secure_vibes(&stack->lock);
    if (stack->top < 0) {
        release_vibes(&stack->lock);
        return -1; // Stack underflow
    }
    *value = stack->data[stack->top];
    stack->top -= 1;
    release_vibes(&stack->lock);
    return 0;
}

// Function A: Push to stack and update global_value
void funcA(void **arg) {
    int push_result = stack_push(&stack, 10);

    secure_vibes(&cur_vibes);
    if (push_result == 0) {
        *global_value = 10;  // Update global_value if push is successful
    }
    release_vibes(&cur_vibes);
}

// Function B: Pop from stack and update global_value
void funcB(void **arg) {
    int pop_value;
    int pop_result = stack_pop(&stack, &pop_value);

    secure_vibes(&cur_vibes); 
    if (pop_result == 0) {
        *global_value = pop_value;  // Update global_value if pop is successful
    }
    release_vibes(&cur_vibes);
}

void notmain() {
    stack_init(&stack);
    vibe_init(&cur_vibes);
    
    global_value = kmalloc(sizeof(int));
    *global_value = 0;

    int interleaved_ncs = 1;
    function_exec executables[NUM_FUNCS];
    executables[0].func_addr = (func_ptr)funcA;
    executables[0].num_vars = 0;
    executables[0].var_list = NULL;

    executables[1].func_addr = (func_ptr)funcB;
    executables[1].num_vars = 0;
    executables[1].var_list = NULL;

    const size_t num_perms = factorial(NUM_FUNCS);
    int **itl = kmalloc(num_perms * sizeof(int *));
    for (int i = 0; i < num_perms; i++) {
        itl[i] = kmalloc(NUM_FUNCS * sizeof(int));
    }
    find_permutations(itl, NUM_FUNCS);

    uint64_t valid_hashes[num_perms];
    find_good_hashes(executables, NUM_FUNCS, itl, num_perms, NULL, valid_hashes);

    // Initialize memory state
    int *global_vars[NUM_VARS] = {global_value};  // Include global_value
    size_t sizes[NUM_VARS] = {sizeof(int)};
    memory_segments initial_mem_state = {NUM_VARS, (void **)global_vars, NULL, sizes};
    initialize_memory_state(&initial_mem_state);

    int load_store_mode = 1;
    // run_interleavings(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
    run_interleavings_as_generated(executables, NUM_FUNCS, itl, num_perms, &initial_mem_state, valid_hashes, interleaved_ncs, load_store_mode);
}
