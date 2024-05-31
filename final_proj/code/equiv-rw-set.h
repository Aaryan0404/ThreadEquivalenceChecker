#ifndef __EQUIV_RW_SET
#define __EQUIV_RW_SET

#include "interleaver.h"

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
 * userspace will be considered a tracked read/write.
 */
void rw_tracker_arm();
void rw_tracker_disarm();

void find_rw_set(func_ptr exe);

#endif
