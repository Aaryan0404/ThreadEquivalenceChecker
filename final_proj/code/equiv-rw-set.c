#include "rpi.h"
#include "equiv-mmu.h"
#include "full-except.h"
#include "equiv-rw-set.h"
#include "armv6-debug-impl.h"

static uint32_t rw_tracker_enabled;

static void rw_tracker_data_abort_handler(regs_t* r) {
  uint32_t addr = cp15_far_get();
  uint32_t pc = r->regs[REGS_PC];
  uint32_t dfsr = cp15_dfsr_get();

  // Parse DFSR according to B4-43
  uint32_t status = (bit_isset(dfsr, 10) << 4) | bits_get(dfsr, 0, 3);
  demand(status == 0b01101, only section permission faults expected);
  uint32_t domain = bits_get(dfsr, 4, 7);
  demand(domain != user_dom, we should never fault when accessing the user domain);
  uint32_t w = bit_isset(dfsr, 11);

  if(w) printk("instruction %x writes to %x\n", pc, addr);
  else printk("instruction %x reads from %x\n", pc, addr);

  // TODO: Here is where we'd maintain a read and write set

  // Disable data aborts
  rw_tracker_disarm();
}

void rw_tracker_init(uint32_t enabled) {
  full_except_set_data_abort(rw_tracker_data_abort_handler);
  rw_tracker_disarm();
  rw_tracker_enabled = enabled;
}

void rw_tracker_enable() { rw_tracker_enabled = 1; }
void rw_tracker_disable() { rw_tracker_enabled = 0; }

void set_data_faults(uint32_t enable) {
  uint32_t domain_acl = domain_access_ctrl_get();

  // Data faults will occur if the kernel domain has permission checks enabled.
  // In this state, the MMU will check the permissions of kernel pages and
  // fault because kernel pages all have perm_rw_priv and user code is run in
  // user mode.
  if(enable) domain_acl = bits_set(domain_acl, user_dom, user_dom+1, DOM_client);
  else       domain_acl = bits_set(domain_acl, user_dom, user_dom+1, DOM_manager);
  domain_access_ctrl_set(domain_acl);
}

void rw_tracker_arm() { set_data_faults(1); }
void rw_tracker_disarm() { set_data_faults(0); }
