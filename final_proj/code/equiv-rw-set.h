#ifndef __EQUIV_RW_SET
#define __EQUIV_RW_SET

#include "interleaver.h"
#include "set.h"
#include "rpi.h"

typedef struct {
  // If defined, reads/writes are put here
  set_t* read;
  set_t* write;
  // If both defined, accesses to shared memory are stored in flagged_pcs
  set_t* shared_memory;
  set_t* flagged_pcs;
} rw_tracker_t;

/*
 * Definitions, handlers, and logic for read-write set maintenence.
 */

/*
 * Initialize the read-write tracker.
 */
void rw_tracker_init(uint32_t enabled);

static rw_tracker_t rw_tracker_mk(set_t* r, set_t* w) {
  rw_tracker_t t = {
    .read = r,
    .write = w,
    .shared_memory = NULL,
    .flagged_pcs = NULL
  };
  return t;
}

static rw_tracker_t pc_tracker_mk(set_t* shared_mem, set_t* pcs) {
  rw_tracker_t t = {
    .read = NULL,
    .write = NULL,
    .shared_memory = shared_mem,
    .flagged_pcs = pcs
  };
  return t;
}

/*
 * Enable or disable read-write tracking.
 */
void rw_tracker_enable();
void rw_tracker_disable();

/*
 * Arm/disarm read-write tracker. The next access of kernel memory from
 * userspace will be considered a tracked read/write. Arm is ALWAYS called from
 * single step, so if you want to disable RW tracking you should call
 * rw_tracker_disable()
 */
void rw_tracker_arm();
void rw_tracker_disarm();

/*
 * Record a syscall. Used to also track locks and other synchronization primitives.
 */
void rw_tracker_record_syscall(eq_th_t* th, regs_t* r);

/*
 * Set the current tracker. Only one tracker can be "the tracker" at once. It
 * receives the reads and writes that are caught by the data abort.
 */
void set_tracker(rw_tracker_t tracker);

/*
 * Finds the set of read and write addresses (byte addresses) for a function
 */
void find_rw_set(func_ptr exe, set_t* read, set_t* write);

/*
 * Finds the set of PCs that access shared memory
 */
void find_pc_set(func_ptr exe, set_t* shared_memory, set_t* pcs);

#endif
