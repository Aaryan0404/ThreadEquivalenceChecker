typedef void (*func_ptr)(void**);

typedef struct {
    func_ptr func_addr;  // Pointer to the function
    size_t num_vars;    // Number of variables in the function
    void *var_list; // Pointer to the list of variables
} function_exec; 

typedef struct {
    int tid; 
    int num_instrs; 
} schedule_info; 

uint32_t convert_funcid_to_tid(uint32_t func_id, uint32_t last_tid);