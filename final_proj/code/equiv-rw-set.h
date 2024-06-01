#ifndef __EQUIV_RW_SET
#define __EQUIV_RW_SET

#include "interleaver.h"
#include "set.h"

typedef struct {
  set_t* read;
  set_t* write;
} rw_tracker_t;

/*
 * Definitions, handlers, and logic for read-write set maintenence.
 */

/*
 * Initialize the read-write tracker.
 */
void rw_tracker_init(uint32_t enabled);

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
 * Set the current tracker. Only one tracker can be "the tracker" at once. It
 * receives the reads and writes that are caught by the data abort.
 */
void set_tracker(rw_tracker_t tracker);

void find_rw_set(func_ptr exe, set_t* read, set_t* write);

#endif
