typedef struct {
    void *func_addr;    // Pointer to the function
    size_t num_vars;    // Number of variables in the function
    void *var_list; // Pointer to the list of variables
} function_exec; 

typedef struct {
    // number of memory segments
    size_t num_ptrs;
    // list of memory ptrs to segments
    void *ptr_list;
    // list of original memory ptrs to segments
    void *ptr_og_list; 
    // list of sizes of memory segments
    int *size_list;
} memory_segments;

