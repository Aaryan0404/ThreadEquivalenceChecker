#include "rpi.h"
#include "equiv-checker.h"

#define NUM_FUNCS 2
#define NUM_CTX 2

// USER CODE
int* global_var;

// multiply by 4 and add 1
EQUIV_USER
void funcMA(void **arg) {
    int a = *global_var;
    gcc_mb();
    a += 1; 
    a *= 3;
    a++;
    gcc_mb();
    *global_var = a;   
}

// subtracts 1 from global var a
EQUIV_USER
void funcMS(void **arg) {
    int a = *global_var;
    gcc_mb();
    a *= 2;
    a -= 7;
    a *= 3;
    gcc_mb();
    *global_var = a;
}

void init_memory() {
  *global_var = 5;
}

void notmain() {    
    equiv_checker_init();
    set_verbosity(1);

    // number of interleaved context switches (remaining context switches will result in threads being run to completion)
    global_var = kmalloc(sizeof(int));
    *global_var = 5;

    function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));
    executables[0].func_addr = (func_ptr)funcMA;
    executables[1].func_addr = (func_ptr)funcMS;

    equiv_checker_run(executables, NUM_FUNCS, NUM_CTX, init_memory, NULL);
}
