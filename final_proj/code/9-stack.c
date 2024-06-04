#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"
#include "equiv-checker.h"

#define MAX_STACK_SIZE 100
#define NUM_FUNCS 2
#define NUM_CTX 2

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

EQUIV_USER
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
EQUIV_USER
void funcA(void **arg) {
    stack_pop(&stack, global_value);
}

// Function B: Pop from stack and update global_value (similar to funcA)
EQUIV_USER
void funcB(void **arg) {
    stack_pop(&stack, global_value);
}

EQUIV_USER
void funcA_bad(void **arg) {
    if (stack.top < 0) {
        return;
    }
    *global_value = stack.data[stack.top];
    stack.top -= 1;
}

EQUIV_USER
void funcB_bad(void **arg) {
    if (stack.top < 0) {
        return;
    }
    *global_value = stack.data[stack.top];
    stack.top -= 1;
}

void init_memory() {
  stack_init(&stack);
  vibe_init(&stack.lock);
  *global_value = 0;
}

void notmain() {
    equiv_checker_init();
    set_verbosity(1);

    global_value = kmalloc(sizeof(int));

    function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));
    /*executables[0].func_addr = (func_ptr)funcA;
    executables[1].func_addr = (func_ptr)funcB;*/
    /*executables[0].func_addr = (func_ptr)funcA_bad;
    executables[1].func_addr = (func_ptr)funcB_bad;*/

    /*set_t* shared_mem_hint = set_alloc();
    add_mem(shared_mem_hint, global_var2, sizeof(int));*/
    set_t* shared_mem_hint = NULL;

    memory_tags_t t = mk_tags(4);
    add_tag(&t, global_value, "global 1");
    add_tag(&t, &stack.lock, "stack lock");
    add_tag(&t, &stack.top, "stack top");
    add_tag(&t, &stack.data, "stack data");

    equiv_checker_run(executables, NUM_FUNCS, NUM_CTX, init_memory, shared_mem_hint, &t);
}
