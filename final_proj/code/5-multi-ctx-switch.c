#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"
#include "equiv-checker.h"

#define NUM_FUNCS 2
#define NUM_CTX 2
#define load_store_mode 1

int* global_var;
int* global_var2;
int* global_var3;

// Function A - increment global_var and set global_var2 to its new value
EQUIV_USER
void funcA(void **arg) {
    *global_var  = 4; 
    *global_var2 = 5; 
    *global_var3 = *global_var + *global_var2;
}

// Function B - double global_var2 and increment global_var based on the new value of global_var2
EQUIV_USER
void funcB(void **arg) {
    *global_var  = 5; 
    *global_var2 = 4; 
    *global_var3 = *global_var + *global_var2;
}

void init_memory() {
  *global_var = 0;
  *global_var2 = 0;
  *global_var3 = 0;
}

void notmain() {
  equiv_checker_init();

  global_var = kmalloc(sizeof(int));
  global_var2 = kmalloc(sizeof(int));
  global_var3 = kmalloc(sizeof(int));
  

  function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));
  executables[0].func_addr = (func_ptr)funcA;
  executables[1].func_addr = (func_ptr)funcB;

  equiv_checker_run(executables, NUM_FUNCS, NUM_CTX, init_memory, NULL);
}
