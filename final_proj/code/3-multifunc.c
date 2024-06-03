#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"
#include "equiv-checker.h"

#define NUM_FUNCS 3
#define NUM_CTX 3

// USER CODE
int* global_var;
int* global_var2;
int* global_var3;

// multiply by 4 and add 1
EQUIV_USER
void funcMA(void **arg) {
    int a = *global_var;
    a += 1; 
    *global_var = a;   

    int b = *global_var2;
    b *= 2;
    *global_var2 = b;
}

// subtracts 1 from global var a
EQUIV_USER
void funcMS(void **arg) {
    int a = *global_var2; 
    a *= 2;
    *global_var2 = a;

    int b = *global_var;
    b += 1;
    *global_var = b;
}

// independent third function
EQUIV_USER
void funcIndep(void **arg) {
    int a = *global_var3; 
    a *= 2;
    a += 1;

    *global_var3 = a;
}

void init_memory() {
  *global_var = 5;
  *global_var2 = 10;
  *global_var3 = 15;
}

void notmain() {    
    equiv_checker_init();

    global_var = kmalloc(sizeof(int));
    global_var2 = kmalloc(sizeof(int));
    global_var3 = kmalloc(sizeof(int));
    
    function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));
    executables[0].func_addr = (func_ptr)funcMA;
    executables[1].func_addr = (func_ptr)funcMS;
    executables[2].func_addr = (func_ptr)funcIndep;

    equiv_checker_run(executables, NUM_FUNCS, NUM_CTX, init_memory, NULL);
}
