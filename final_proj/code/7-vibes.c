#include "rpi.h"
#include "equiv-checker.h"

#define NUM_FUNCS 2
#define NUM_CTX 3

int* global_var;
int* global_var2;
vibe_check_t cur_vibes; // Global spin lock

// Function A
EQUIV_USER
void funcA(void **arg) {
    // spin_lock(&lock); // Acquire the lock
    secure_vibes(&cur_vibes); // Acquire the lock
    
    // Perform operations
    *global_var += 1;
    *global_var2 = *global_var;

    release_vibes(&cur_vibes); // Release the lock
    // spin_unlock(&lock); // Release the lock
}

// Function B 
EQUIV_USER
void funcB(void **arg) {
    // spin_lock(&lock); // Acquire the lock
    secure_vibes(&cur_vibes); // Acquire the lock
    
    // Perform operations
    *global_var += 2;
    *global_var2 = *global_var; 

    release_vibes(&cur_vibes); // Release the lock
    // spin_unlock(&lock); // Release the lock
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

