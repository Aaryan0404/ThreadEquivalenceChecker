// setup so we can do hashing equivalance of threads.
#include "rpi.h"
#include "mini-step.h"
#include "equiv-threads.h"
#include "fast-hash32.h"
#include "equiv-mmu.h"
#include "equiv-rw-set.h"

enum { stack_size = 1024 * 2 };
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

static ctx_switch_status_t ctx_switch_status;
static set_t* shared_memory = NULL;
static schedule_t* schedule = NULL;

static uint32_t init = 0;

static int verbose_p = 1;
void equiv_verbose_on(void) {
    verbose_p = 1;
}
void equiv_verbose_off(void) {
    verbose_p = 0;
}

static int load_str_mode = 1;
void equiv_set_load_str_mode(int mode){
    load_str_mode = mode;
}

#undef trace
#define trace(args...) do {                                 \
    if(verbose_p) {                                         \
        printk("TRACE:%s:", __FUNCTION__); printk(args);    \
    }                                                       \
} while(0)
    

// retrieve the thread with the specified tid from the queue
eq_th_t * retrieve_tid_from_queue(uint32_t tid) {
    eq_th_t * th = eq_pop(&equiv_runq);
    // printk("retrieved thread at start %d\n", th->tid);

    uint32_t first_tid = th->tid;
    while(th->tid != tid) {
        // printk("popped thread %d\n", th->tid);
        eq_th_t * old_thread = th;
        th = eq_pop(&equiv_runq);
        // printk("#1 append tid %d\n", old_thread->tid);
        eq_append(&equiv_runq, old_thread);
        if(th->tid == first_tid) {
            panic("specified tid %d is not in the queue\n", tid);
        }
    }
    return th;
}

static __attribute__((noreturn)) 
void equiv_schedule(void) 
{
    assert(cur_thread);

    eq_th_t * th = NULL;

    // if we have context switches remaining, switch to the next thread in the
    // schedule otherwise we'll just run whatever the next thread in the queue
    // is to completion
    if(
      schedule &&
      ctx_switch_status.ctx_switch < schedule->n_ctx_switches
    ) {
      panic("BAD");
        uint32_t tid_idx = ctx_switch_status.ctx_switch + 1;
        th = retrieve_tid_from_queue(schedule->tids[tid_idx]);
    }
    else{
        th = eq_pop(&equiv_runq);
    }
    
    if(th) {
        if(th->verbose_p)
            trace("switching from tid=%d,pc=%x to tid=%d,pc=%x,sp=%x\n", 
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

void print_schedule(const char* msg, schedule_t* schedule) {
  if(msg) printk(msg);

  /* Code golf */
  for(int f = 0; f < schedule->n_funcs; f++) {
    printk("\t%d", f);
  }
  printk("\n");
  for(int ctx_switch = 0; ctx_switch < schedule->n_ctx_switches; ctx_switch++) {
    for(int instr = 0; instr < schedule->instr_counts[ctx_switch]; instr++) {
      for(int f = 0; f < schedule->n_funcs; f++) {
        printk("\t");
        if(schedule->tids[ctx_switch]-1 == f) printk("|");
        else printk(" ");
      }
      printk("\n");
    }
  }
  for(int f = 0; f < schedule->n_funcs; f++) {
    printk("\t");
    if(schedule->tids[schedule->n_ctx_switches]-1 == f) printk("R", f);
    else printk(" ");
  }
  printk("\n");
}

void ctx_switch_handler(set_t *touched_memory, uint32_t pc) {
    // If we don't have a schedule, give up
    if(!schedule) return;

    // If we are out of context switches, run to completion
    if(ctx_switch_status.ctx_switch >= schedule->n_ctx_switches) return;

    // find intersection of shared memory and touched memory
    set_t *intersection = set_alloc();
    set_intersection(intersection, shared_memory, touched_memory);

    // if the intersection is non-empty
    if (!set_empty(intersection)) {
        if(cur_thread->verbose_p)
          trace("PC %x touched shared memory\n", pc);
        set_free(intersection);
        ctx_switch_status.do_instr_count = 1;
    }
}



/******************************************************************
 * tiny syscall setup.
 */
int syscall_trampoline(int sysnum, ...);

// page A3-2
enum {
    LS_IMMEDIATE_OFFSET = 0b010,
    LS_REGISTER_OFFSET = 0b011,
    LS_MULTIPLE = 0b100,
};

// page A3-39

void set_schedule(schedule_t* s) { schedule = s; }

void set_shared_memory(set_t* sh) { shared_memory = sh; }

void reset_ctx_switch() {
  ctx_switch_status.instr_count = 0;
  ctx_switch_status.ctx_switch = 0;
  ctx_switch_status.do_instr_count = 0;
}

void enable_ctx_switch(schedule_t* sched, set_t* shared_mem) {
  schedule = sched;
  shared_memory = shared_mem;
  reset_ctx_switch();
}

ctx_switch_status_t get_ctx_switch_status() { return ctx_switch_status; }

void disable_ctx_switch(){
    schedule = NULL;
    shared_memory = NULL;
    reset_ctx_switch();
}

// in staff-start.S

void sys_equiv_exit(uint32_t ret);

void sys_equiv_switch(uint32_t ret);

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
        if(schedule) {
          // Run to completion. Disable traps
          if(ctx_switch_status.ctx_switch >= schedule->n_ctx_switches) {
            if(cur_thread->verbose_p)
              trace("Done with schedule\n");
            schedule = NULL;
           // Finished early, disable traps
          } else {
            if(cur_thread->verbose_p)
              trace("Finished early, running to completion\n");
            schedule = NULL;
          }
        }
        th = eq_pop(&equiv_runq);
        
        if(th && th->verbose_p)
          trace("thread %d next\n", th->tid);

        // if no more threads we are done.
        if(!th) {
            if(th->verbose_p) trace("done with all threads\n");
            switchto(&start_regs);
        }
        // otherwise do the next one.
        cur_thread = th;
        mismatch_run(&cur_thread->regs);
        not_reached();

    // case EQUIV_SWITCH:
    //     trace("thread=%d switched back with code=%d, hash=%x\n", 
    //         th->tid, r->regs[1], th->reg_hash);

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

static unsigned ntids = 1;

void reset_ntids() {
  assert(eq_empty(&equiv_runq));
  ntids = 1;
}

// fork <fn(arg)> as a pre-emptive thread.
eq_th_t *equiv_fork(void (*fn)(void**), void **args, uint32_t expected_hash) {
    eq_th_t *th = kmalloc_aligned(stack_size, 8);

    assert((uint32_t)th%8==0);
    th->tid = ntids++;

    //th->verbose_p = 1;

    th->fn = (uint32_t)fn;
    th->args = (uint32_t)args;

    // allocate the 8byte aligned stack
    th->stack_start = (uint32_t)th;
    th->stack_end = th->stack_start + stack_size;
    demand(th->stack_end % 8 == 0, sp is not aligned);
    
    th->regs = equiv_regs_init(th);
    check_sp(th);

    // printk("#4 pushing tid %d\n", th->tid);
    eq_push(&equiv_runq, th);
    //printk("FORK pushed thread to queue %d\n", th->tid);
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
    eq_push(&equiv_runq, th);
}


// just print out the pc and instruction count.
static void equiv_hash_handler(void *data, step_fault_t *s) {
    rw_tracker_arm();

    if(ctx_switch_status.do_instr_count) {
      ctx_switch_status.do_instr_count = 0;
      // increment 
      ctx_switch_status.instr_count++;

      // If we have reached the number of expected instructions, context
      // switch
      if (
        ctx_switch_status.instr_count >= 
        schedule->instr_counts[ctx_switch_status.ctx_switch]
      ) {
        if(cur_thread->verbose_p)
          trace(
            "Finished executing %d instructions on thread %d, switching...\n",
            ctx_switch_status.instr_count, cur_thread->tid
          );

          ctx_switch_status.ctx_switch++;
          ctx_switch_status.instr_count = 0;

          // context switch
          // equiv_schedule();
          uint32_t tid_idx = ctx_switch_status.ctx_switch;
          eq_th_t* th = retrieve_tid_from_queue(schedule->tids[tid_idx]);
          eq_append(&equiv_runq, cur_thread);
          
          if(th->verbose_p)
            trace("switching from tid=%d,pc=%x to tid=%d,pc=%x,sp=%x\n", 
                cur_thread->tid, 
                cur_thread->regs.regs[REGS_PC],
                th->tid,
                th->regs.regs[REGS_PC],
                th->regs.regs[REGS_SP]);

          cur_thread = th;
          uart_flush_tx();
          mismatch_run(&cur_thread->regs);
      }
    }
}

// run all the threads.
void equiv_run(void) {
    // printk("starting equiv_run\n");
    if(!schedule) {
        cur_thread = eq_pop(&equiv_runq);
    }
    else{
        // printk("current thread %d\n", cur_thread->tid);
        // printk("retrieving thread %d\n", ctx_switch_tid[ctx_switch_idx]);
        cur_thread = retrieve_tid_from_queue(schedule->tids[0]);
    }
    // printk("starting thread %d\n", cur_thread->tid);
    
    if(!cur_thread)
        panic("empty run queue?\n");

    // this is roughly the same as in mini-step.c
    mismatch_on();
    mismatch_pc_set(0);
    switchto_cswitch(&start_regs, &cur_thread->regs);
    mismatch_off();
    //trace("done, returning\n");

    // reset the context switch index
}



// one time initialazation
void equiv_init(void) {
    if(init)
        return;
    mini_step_init(equiv_hash_handler, 0); 
    full_except_set_syscall(equiv_syscall_handler); 
}
