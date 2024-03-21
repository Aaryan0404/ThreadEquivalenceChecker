#ifndef __EQUIV_THREAD_H__
#define __EQUIV_THREAD_H__

/*******************************************************************
 * simple process support.
 */
#include "switchto.h"
typedef struct eq_th {
    // thread's registers.
    regs_t regs;

    struct eq_th *next;

    // if non-zero: the hash we expect to get 
    uint32_t expected_hash;

    // the current cumulative hash
    uint32_t reg_hash;          

    uint32_t tid;           // thread id.

    uint32_t fn;
    uint32_t args;
    uint32_t stack_start;
    uint32_t stack_end;
    uint32_t refork_cnt;

    // how many instructions we executed.
    uint32_t inst_cnt;
    uint32_t loadstr_cnt;
    unsigned verbose_p;  // if you want alot of information.
} eq_th_t;

typedef void (*equiv_fn_t)(void*);

void set_ctx_switches(uint32_t* tid, uint32_t* n, uint32_t num_context_switches);

// a very heavy handed initialization just for today's lab.
// assumes it has total control of system calls etc.
void equiv_init(void);

eq_th_t *equiv_fork(void (*fn)(void**), void **args, uint32_t expected_hash);

eq_th_t * retrieve_tid_from_queue(uint32_t tid);

// run all the threads until there are no more.
void equiv_run(void);

// called by client code.
void sys_equiv_exit(uint32_t ret);

void equiv_refresh(eq_th_t *th);

void disable_ctx_switch();

// don't set stack pointer.
eq_th_t *equiv_fork_nostack(void (*fn)(void**), void **args, uint32_t expected_hash);

void equiv_verbose_on(void);
void equiv_verbose_off(void);
void equiv_set_load_str_mode(int mode);
#endif
