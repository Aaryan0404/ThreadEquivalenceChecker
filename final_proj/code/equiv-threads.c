// setup so we can do hashing equivalance of threads.
#include "rpi.h"
#include "mini-step.h"
#include "equiv-threads.h"
#include "fast-hash32.h"

enum { stack_size = 8192 * 8 };
_Static_assert(stack_size > 1024, "too small");
_Static_assert(stack_size % 8 == 0, "not aligned");

typedef struct rq {
    eq_th_t *head, *tail;
} rq_t;

#include "queue-ext-T.h"
gen_queue_T(eq, rq_t, head, tail, eq_th_t, next)

static rq_t equiv_runq;
static eq_th_t * volatile cur_thread;
static regs_t start_regs;

// an array of the instruction numbers for each thread at which we'll context switch
static uint32_t* ctx_switch_instr_num;
// an array of the thread ids at which we will context switch
static uint32_t* ctx_switch_tid;
// the current context switch we're on (aka the current idx of arrays)
static int ctx_switch_idx = -1;
// the total number of context switches
static size_t num_context_switches = 0;

static int verbose_p = 1;
void equiv_verbose_on(void) {
    verbose_p = 1;
}
void equiv_verbose_off(void) {
    verbose_p = 0;
}

#undef trace
#define trace(args...) do {                                 \
    if(verbose_p) {                                         \
        printk("TRACE:%s:", __FUNCTION__); printk(args);    \
    }                                                       \
} while(0)
    

// retrieve the thread with the specified tid from the queue
eq_th_t * retrieve_tid_from_queue(uint32_t tid) {
    eq_th_t *  th = eq_pop(&equiv_runq);
    uint32_t first_tid = cur_thread->tid;
    while(th->tid != tid) {
        eq_th_t * old_thread = th;
        th = eq_pop(&equiv_runq);
        eq_push(&equiv_runq, old_thread);
        if(th->tid == first_tid) {
            panic("specified tid %d is not in the queue\n", tid);
        }
    }
    return th;
}

/********************************************************************
 * create threads: this is roughly the code from mini-step and the 
 * test.
 *
 * make processes, put them on a runqueue.
 */


static __attribute__((noreturn)) 
void equiv_schedule(void) 
{
    assert(cur_thread);

    eq_th_t * th = NULL;
    // if we have context switches remaining, switch to the next thread in the schedule
    // otherwise we'll just run whatever the next thread in the queue is to completion
    if(ctx_switch_idx < num_context_switches) {
        th = retrieve_tid_from_queue(ctx_switch_tid[ctx_switch_idx - 1]);
    }
    else{
        th = eq_pop(&equiv_runq);
    }
    
    if(th) {
        if(th->verbose_p)
            output("switching from tid=%d,pc=%x to tid=%d,pc=%x,sp=%x\n", 
                cur_thread->tid, 
                cur_thread->regs.regs[REGS_PC],
                th->tid,
                th->regs.regs[REGS_PC],
                th->regs.regs[REGS_SP]);
        eq_append(&equiv_runq, cur_thread);
        cur_thread = th;
    }
    uart_flush_tx();
    mismatch_run(&cur_thread->regs);
}

/******************************************************************
 * tiny syscall setup.
 */
int syscall_trampoline(int sysnum, ...);

enum {
    EQUIV_EXIT = 0,
    EQUIV_PUTC = 1
};


void set_ctx_switches(uint32_t* tid, uint32_t* n, uint32_t ncs) {
    ctx_switch_instr_num = n;
    ctx_switch_tid = tid;
    ctx_switch_idx = 0;
    num_context_switches = ncs;
}

void disable_ctx_switch(){
    ctx_switch_instr_num = NULL;
    ctx_switch_tid = NULL;
    ctx_switch_idx = -1;
}

// in staff-start.S
void sys_equiv_exit(uint32_t ret);

// for the moment we just die.
static void check_sp(eq_th_t *th) {
    let sp = th->regs.regs[REGS_SP];
    if(sp < th->stack_start)
        panic("stack is too small: %x, lowest legal=%x\n",
            sp, th->stack_start);
    if(sp > th->stack_end)
        panic("stack is too high: %x, highest legal=%x\n",
            sp, th->stack_end);
}

// our two system calls: exit (get the next thread if there is one)
// and putc (so we can handle race conditions with prints)
static int equiv_syscall_handler(regs_t *r) {
    let th = cur_thread;
    assert(th);
    th->regs = *r;  // update the registers

    uart_flush_tx();
    check_sp(th);

    unsigned sysno = r->regs[0];
    switch(sysno) {
    case EQUIV_PUTC: 
        uart_put8(r->regs[1]);
        break;
    case EQUIV_EXIT: 
        // trace("thread=%d exited with code=%d, hash=%x\n", 
        //     th->tid, r->regs[1], th->reg_hash);

        // check hash.
        if(!th->expected_hash)
            th->expected_hash = th->reg_hash;
        else if(th->expected_hash) {
            let exp = th->expected_hash;
            let got = th->reg_hash;
            if(exp == got) {
                trace("EXIT HASH MATCH: tid=%d: hash=%x\n", 
                    th->tid, exp, got);
            } else {
                panic("MISMATCH ERROR: tid=%d: expected hash=%x, have=%x\n", 
                    th->tid, exp, got);
            }
        }

        // this could be cleaner: sorry.
        eq_th_t *th = eq_pop(&equiv_runq);

        // if no more threads we are done.
        if(!th) {
            //trace("done with all threads\n");
            switchto(&start_regs);
        }
        // otherwise do the next one.
        cur_thread = th;
        mismatch_run(&cur_thread->regs);
        not_reached();

    default:
        panic("illegal system call: %d\n", sysno);
    }

    equiv_schedule();
}

// this is used to reinitilize registers.
static inline regs_t equiv_regs_init(eq_th_t *p) {
    // get our current cpsr and clear the carry and set the mode
    uint32_t cpsr = cpsr_inherit(USER_MODE, cpsr_get());

    // XXX: which code had the partial save?  the ss rwset?
    regs_t regs = (regs_t) {
        .regs[0] = p->args,
        .regs[REGS_PC] = p->fn,      // where we want to jump to
        .regs[REGS_SP] = p->stack_end,      // the stack pointer to use.
        .regs[REGS_LR] = (uint32_t)sys_equiv_exit, // where to jump if return.
        .regs[REGS_CPSR] = cpsr             // the cpsr to use.
    };
    return regs;
}

// fork <fn(arg)> as a pre-emptive thread.
eq_th_t *equiv_fork(void (*fn)(void**), void **args, uint32_t expected_hash) {
    eq_th_t *th = kmalloc_aligned(stack_size, 8);

    assert((uint32_t)th%8==0);
    th->expected_hash = expected_hash;

    static unsigned ntids = 1;
    th->tid = ntids++;

    th->fn = (uint32_t)fn;
    th->args = (uint32_t)args;

    // allocate the 8byte aligned stack
    th->stack_start = (uint32_t)th;
    th->stack_end = th->stack_start + stack_size;
    demand(th->stack_end % 8 == 0, sp is not aligned);
    
    th->regs = equiv_regs_init(th);
    check_sp(th);

    eq_push(&equiv_runq, th);
    return th;
}
eq_th_t *equiv_fork_nostack(void (*fn)(void**), void **args, uint32_t expected_hash) {
    let th = equiv_fork(fn,args,expected_hash);
    th->regs.regs[REGS_SP] = 0;
    th->stack_start = th->stack_end = 0;
    return th;
}

// re-initialize and put back on the run queue
void equiv_refresh(eq_th_t *th) {
    th->regs = equiv_regs_init(th); 
    check_sp(th);
    th->inst_cnt = 0;
    th->reg_hash = 0;
    eq_push(&equiv_runq, th);
}


// just print out the pc and instruction count.
static void equiv_hash_handler(void *data, step_fault_t *s) {
    gcc_mb();
    let th = cur_thread;
    assert(th);
    th->regs = *s->regs;
    th->inst_cnt++;

    let regs = s->regs->regs;
    uint32_t pc = regs[15];
    //output("tid=%d: pc=%x, cnt=%d\n", th->tid, pc, th->inst_cnt);

    th->reg_hash = fast_hash_inc32(&th->regs, sizeof th->regs, th->reg_hash);

    // should let them turn it off.
    if(th->verbose_p)
        output("hash: tid=%d: cnt=%d: pc=%x, hash=%x\n", 
            th->tid, th->inst_cnt, pc, th->reg_hash);

    gcc_mb();
    // if we reach the instruction on the tid specified in the schedule, context switch
    // equiv_schedule will look at the array to figure out the next thread in the schedule, and switch to that
    if(ctx_switch_idx < 0){
        equiv_schedule();
    }
    else if (th->tid == ctx_switch_tid[ctx_switch_idx] && th->inst_cnt == ctx_switch_instr_num[ctx_switch_idx]) {
        ctx_switch_idx++;
        equiv_schedule();
    }
}

// run all the threads.
void equiv_run(void) {
    if(ctx_switch_idx < 0) {
        cur_thread = eq_pop(&equiv_runq);
    }
    else{
        cur_thread = retrieve_tid_from_queue(ctx_switch_tid[ctx_switch_idx]);
    }
    
    if(!cur_thread)
        panic("empty run queue?\n");

    // this is roughly the same as in mini-step.c
    mismatch_on();
    mismatch_pc_set(cur_thread->regs.regs[15]);
    switchto_cswitch(&start_regs, &cur_thread->regs);
    mismatch_off();
    //trace("done, returning\n");
}

// one time initialazation
void equiv_init(void) {
    kmalloc_init();
    mini_step_init(equiv_hash_handler, 0);
    full_except_set_syscall(equiv_syscall_handler);
}
