#include "rpi.h"
#include "permutations.h"
#include "interleaver.h"
#include "equiv-checker.h"

#define NUM_FUNCS 2
#define NUM_CTX 3

int* global_var;
int* global_var2;

// Function A - increment global_var and set global_var2 to its new value
EQUIV_USER
void funcA(void **arg) {
    int a = *global_var;
    a += 1;
    a *= 4;
    a /= 2;
    a *= 10; 
    a += 3;
    a += 1;
    a *= 4;
    a /= 2;
    a *= 10; 
    a += 3; 
    *global_var = a;
}

// Function B - double global_var2 and increment global_var based on the new value of global_var2
EQUIV_USER
void funcB(void **arg) {
    int b = *global_var2;
    b *= 2;
    b += 1;
    b *= 4;
    b *= 7; 
    b += 2;
    b *= 2;
    b += 1;
    b *= 4;
    b *= 7; 
    b += 2;
    *global_var = b;
}

void init_memory() {
  *global_var = 1;
  *global_var2 = 2;
}

void notmain() {
    equiv_checker_init();

    global_var = kmalloc(sizeof(int));
    global_var2 = kmalloc(sizeof(int));

    function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));
    executables[0].func_addr = (func_ptr)funcA;
    executables[1].func_addr = (func_ptr)funcB;

    equiv_checker_run(executables, NUM_FUNCS, NUM_CTX, init_memory, NULL);
}
