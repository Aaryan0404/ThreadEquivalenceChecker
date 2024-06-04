#include "rpi.h"
#include "equiv-checker.h"

#define NUM_FUNCS 2
#define NUM_CTX 1

EQUIV_USER
static inline uint32_t normal_increment(uint32_t* x) {
  uint32_t t = *x;
  gcc_mb();
  t++;
  gcc_mb();
  *x = t;
  return 1;
}

uint32_t* global;

EQUIV_USER
void funcA(void** arg) {
  atomic_increment(global);
  //normal_increment(global);
}

EQUIV_USER
void funcB(void** rag) {
  atomic_increment(global);
  //normal_increment(global);
}

void init_memory() {
  *global = 0;
}

void notmain() {    
    equiv_checker_init();
    set_verbosity(3);

    // number of interleaved context switches (remaining context switches will result in threads being run to completion)
    global = kmalloc(sizeof(uint32_t));
    *global = 5;

    function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));
    executables[0].func_addr = (func_ptr)funcA;
    executables[1].func_addr = (func_ptr)funcB;

    memory_tags_t t = mk_tags(1);
    add_tag(&t, global, "global");

    equiv_checker_run(executables, NUM_FUNCS, NUM_CTX, init_memory, NULL, &t);
}
