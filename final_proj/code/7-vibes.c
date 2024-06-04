#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"
#include "equiv-checker.h"

#define NUM_FUNCS 2
#define NUM_CTX 3

#define LOCKED 1
#define UNLOCKED 0

typedef uint32_t vibe_check_t;

/* // Assembly function to try acquiring the lock
extern int vibe_check(vibe_check_t *cur_vibes);
extern void secure_vibes(vibe_check_t *cur_vibes);
extern void release_vibes(vibe_check_t *cur_vibes);*/

EQUIV_USER
static inline uint32_t atomic_compare_and_swap(uint32_t* x, uint32_t old_val, uint32_t new_val) {
  uint32_t result;
  uint32_t status;
  asm volatile (
   "mov %1, #1\n"
   "ldrex %0, [%2]\n"         // Load the value from ptr into result
   "teq %0, %3\n"             // Compare result with old_val
   "strexeq %1, %4, [%2]\n"   // Attempt to store new_val to ptr if equal
   : "=&r" (result), "=&r" (status)
   : "r" (x), "r" (old_val), "r" (new_val)
   : "memory", "cc"
  );
  return status;
}


EQUIV_USER
static inline int vibe_check(vibe_check_t *cur_vibes) {
  if(atomic_compare_and_swap(cur_vibes, UNLOCKED, LOCKED) == 0) return UNLOCKED;
  else return LOCKED;
}

EQUIV_USER
static inline void secure_vibes(vibe_check_t *cur_vibes) {
  while(atomic_compare_and_swap(cur_vibes, UNLOCKED, LOCKED) != 0) {
    sys_equiv_yield();
  }
}

EQUIV_USER
static inline void release_vibes(vibe_check_t *cur_vibes) {
  *cur_vibes = UNLOCKED;
}

// Spin lock functions
void vibe_init(vibe_check_t *cur_vibes) {
    *cur_vibes = UNLOCKED; // 0 indicates that the lock is available
}

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

