#ifndef __EQUIV_THREAD_H__
#define __EQUIV_THREAD_H__

/*******************************************************************
 * simple process support.
 */
#include "switchto.h"
#include "set.h"

typedef struct {
  uint32_t* tids;
  uint32_t* instr_counts;
  uint32_t n_ctx_switches;
  uint32_t n_funcs;
} schedule_t;

typedef struct eq_th {
    // thread's registers.
    regs_t regs;

    struct eq_th *next;

    uint32_t tid;           // thread id.

    uint32_t fn;
    uint32_t args;
    uint32_t stack_start;
    uint32_t stack_end;
    uint32_t refork_cnt;

    // how many instructions we executed.
    unsigned verbose_p;  // if you want alot of information.
} eq_th_t;

typedef struct {
  uint32_t ctx_switch;
  uint32_t instr_count;
  // Set to true before R/W commits, read by prefetch abort for next instruction
  uint32_t do_instr_count;
} ctx_switch_status_t;

enum {
    EQUIV_EXIT = 0,
    EQUIV_PUTC = 1,
    EQUIV_SWITCH = 2
};

typedef void (*equiv_fn_t)(void*);

// Handler for touch events. Updates context switch status
void ctx_switch_handler(set_t *touched_memory, uint32_t pc);

void print_schedule(const char* msg, schedule_t* schedule);

// Sets the current schedule
void set_schedule(schedule_t* s);

// Sets the shared memory set
void set_shared_memory(set_t* sh);

ctx_switch_status_t get_ctx_switch_status();

// Resets the context switching state
void reset_ctx_switch();

// Sets the schedule and shared mem then turns on context switching
void enable_ctx_switch(schedule_t* sched, set_t* shared_mem);

// Disables context switching
void disable_ctx_switch();

// a very heavy handed initialization just for today's lab.
// assumes it has total control of system calls etc.
void equiv_init(void);

void reset_ntids();

eq_th_t *equiv_fork(void (*fn)(void**), void **args, uint32_t expected_hash);

eq_th_t * retrieve_tid_from_queue(uint32_t tid);

// run all the threads until there are no more.
void equiv_run(void);

// called by client code.
void sys_equiv_exit(uint32_t ret);

void equiv_refresh(eq_th_t *th);

// don't set stack pointer.
eq_th_t *equiv_fork_nostack(void (*fn)(void**), void **args, uint32_t expected_hash);

void equiv_verbose_on(void);
void equiv_verbose_off(void);
void equiv_set_load_str_mode(int mode);
#endif
