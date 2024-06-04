#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"
#include "equiv-checker.h"

#define NUM_FUNCS 2
#define NUM_CTX 2

int* global_var;
int* global_var2;
vibe_check_t cur_vibes; // Global spin lock

EQUIV_USER
void non_intrinsic_atomic_increment(int *ptr) {
    secure_vibes(&cur_vibes);
    *ptr += 1; 
    release_vibes(&cur_vibes);
}

EQUIV_USER
void atomic_decrement(int *ptr) {
    secure_vibes(&cur_vibes);
    *ptr -= 1; 
    release_vibes(&cur_vibes);
}

EQUIV_USER
void atomic_load(int *ptr, int *result) {
    secure_vibes(&cur_vibes);
    *result = *ptr;
    release_vibes(&cur_vibes);
}

EQUIV_USER
void atomic_store(int *ptr, int *val) {
    secure_vibes(&cur_vibes);
    *ptr = *val;
    release_vibes(&cur_vibes);
}

// Function A
EQUIV_USER
void funcA(void **arg) {
    non_intrinsic_atomic_increment(global_var);
}

// Function B 
EQUIV_USER
void funcB(void **arg) {
    atomic_decrement(global_var);
}

// Function A bad
EQUIV_USER
void funcA_bad(void **arg) {
    *global_var += 1;
}

// Function B bad
EQUIV_USER
void funcB_bad(void **arg) {
    *global_var -= 1; 
}

void init_memory() {
  *global_var = 0;
  *global_var2 = 0;
  vibe_init(&cur_vibes);
}

void notmain() {
    equiv_checker_init();
    set_verbosity(1);

    global_var = kmalloc(sizeof(int));
    global_var2 = kmalloc(sizeof(int));

    function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));
    executables[0].func_addr = (func_ptr)funcA;
    executables[1].func_addr = (func_ptr)funcB;

    set_t* shared_mem_hint = set_alloc();
    add_mem(shared_mem_hint, global_var2, sizeof(int));

    memory_tags_t t = mk_tags(3);
    add_tag(&t, global_var, "global 1");
    add_tag(&t, global_var2, "global 2");
    add_tag(&t, &cur_vibes, "vibecheck");

    equiv_checker_run(executables, NUM_FUNCS, NUM_CTX, init_memory, shared_mem_hint, &t);
}
