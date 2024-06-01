#include "rpi.h"
#include "equiv-mmu.h"
#include "full-except.h"
#include "equiv-rw-set.h"
#include "armv6-debug-impl.h"
#include "interleaver.h"
#include "equiv-threads.h"

static uint32_t rw_tracker_enabled;

static rw_tracker_t current_tracker;

static void rw_tracker_data_abort_handler(regs_t* r) {
  uint32_t addr = cp15_far_get();
  uint32_t pc = r->regs[REGS_PC];
  uint32_t dfsr = cp15_dfsr_get();

  // Parse DFSR according to B4-43
  uint32_t status = (bit_isset(dfsr, 10) << 4) | bits_get(dfsr, 0, 3);
  demand(status == 0b01101, only section permission faults expected);
  uint32_t domain = bits_get(dfsr, 4, 7);
  demand(domain != user_dom, we should never fault when accessing the user domain);

  // Read/write bit is stored here in addition to the instruction but this is
  // much more consistent
  uint32_t w = bit_isset(dfsr, 11);
  set_t* destination_set;
  if(w) destination_set = current_tracker.write;
  else destination_set = current_tracker.read;

  /* 
   * Now the challenge! Figure out WHICH BYTES this access actually touched...
   */

  // Luckily, the address mode computations are already factored into the
  // address that triggered the data abort

  uint32_t instruction = GET32(pc);

  // TODO: Alignment?????

  // A3-22 : Load/store word or unsigned byte
  if(bits_get(instruction, 26, 27) == 0b01) {
    // A3-22 : B == 1 means byte
    if(bit_isset(instruction, 22)) {
      set_insert(destination_set, addr);
    } else {
      for(int i = 0; i < 4; i++)
        set_insert(destination_set, addr + i);
    }
  }

  // A3-23 : Load/store halfword, double word, or signed byte
  if(bits_get(instruction, 25, 27) == 0b000) {
    // Ugh
    uint32_t l = bit_isset(instruction, 20);
    uint32_t sh = bits_get(instruction, 5, 6);
    uint32_t lsh = (l << 2) | sh;
    // A5-34
    switch(lsh) {
      // Store halfword
      case 0b001:
      // Load signed half word
      case 0b111:
      // Load unsigned halfword
      case 0b101:
        set_insert(destination_set, addr);
        set_insert(destination_set, addr + 1);
        break;
      // Load double word
      case 0b010:
      // Store double word
      case 0b011:
        for(int i = 0; i < 8; i++)
          set_insert(destination_set, addr + i);
        break;
      // Load signed byte
      case 0b110:
        set_insert(destination_set, addr);
        break;
      default:
        panic("Unexpected LSH combination\n");
    }
  }

  // A3-26 : Load/store multiple
  if(bits_get(instruction, 25, 27) == 0b100) {
    // Weakness - assumes that the LDM/STM traps for ALL accessed data, not
    // only some of the accesses
    uint32_t register_list = bits_get(instruction, 0, 15);
    uint32_t offset = 0;
    for(int i = 0; i < 16; i++) {
      if((register_list >> i) & 0x1) {
        for(int j = 0; j < 4; j++)
          set_insert(destination_set, addr + offset + j);
        offset += 4;
      }
    }
  }

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

// Only arms if enabled
void rw_tracker_arm() { if(rw_tracker_enabled) set_data_faults(1); }

// Always disarms
void rw_tracker_disarm() { set_data_faults(0); }

void set_tracker(rw_tracker_t tracker) { current_tracker = tracker; }

// Finds read and write sets for a function
void find_rw_set(func_ptr exe, set_t* read, set_t* write) {
  // Initialize single stepping & threads
  equiv_init();

  // Disable context switching
  disable_ctx_switch();

  rw_tracker_t tracker = {
    .read = read,
    .write = write
  };
  current_tracker = tracker;

  // Enable the RW tracker
  rw_tracker_enable();

  // Run our function
  eq_th_t* t = equiv_fork(exe, NULL, 0);
  equiv_run();

  // Clean up
  rw_tracker_disable();
}

