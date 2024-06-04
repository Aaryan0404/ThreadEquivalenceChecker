#include "rpi.h"
#include "equiv-mmu.h"
#include "full-except.h"
#include "equiv-rw-set.h"
#include "armv6-debug-impl.h"
#include "interleaver.h"
#include "equiv-threads.h"

static uint32_t rw_tracker_enabled;

static rw_tracker_t current_tracker;

static void get_touched_bytes(uint32_t instruction, uint32_t addr, set_t* destination_set) {
  // TODO: Alignment?????

  // A4-213 SWP
  if(bits_get(instruction, 20, 27) == 0b00010000) {
    for(int i = 0; i < 4; i++)
      set_insert(destination_set, addr + i);
  }
  // A4-214 SWPB
  else if(bits_get(instruction, 20, 27) == 0b00010100) {
    set_insert(destination_set, addr);
  }
  // A4-52 & A4-202 LDREX/STREXX
  else if(bits_get(instruction, 21, 27) == 0b0001100) {
    // Only words
    for(int i = 0; i < 4; i++)
      set_insert(destination_set, addr + i);
  }
  // A3-22 : Load/store word or unsigned byte
  else if(bits_get(instruction, 26, 27) == 0b01) {
    // A3-22 : B == 1 means byte
    if(bit_isset(instruction, 22)) {
      set_insert(destination_set, addr);
    } else {
      for(int i = 0; i < 4; i++)
        set_insert(destination_set, addr + i);
    }
  }
  // A3-23 : Load/store halfword, double word, or signed byte
  else if(bits_get(instruction, 25, 27) == 0b000) {
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
        printk("%x accessed %x\n", instruction, addr);
        panic("Unexpected LSH combination\n");
    }
  }
  // A3-26 : Load/store multiple
  else if(bits_get(instruction, 25, 27) == 0b100) {
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

  
}


static memory_touch_handler_t memory_touch_handler;

void set_memory_touch_handler(memory_touch_handler_t func) {
  memory_touch_handler = func;
} 

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

  // Compute set of touched bytes
  set_t* touched = set_alloc();
  uint32_t instruction = GET32(pc);
  get_touched_bytes(instruction, addr, touched);

  if (memory_touch_handler) {
    memory_touch_handler(touched, pc); 
  }

  // Store R/W addresses
  if(current_tracker.write && w)
    set_union_inplace(current_tracker.write, touched);
  else if(current_tracker.read && !w)
    set_union_inplace(current_tracker.read, touched);

  // Disable data aborts
  rw_tracker_disarm();
  set_free(touched);
}

void rw_tracker_init(uint32_t enabled) {
  full_except_set_data_abort(rw_tracker_data_abort_handler);
  rw_tracker_disarm();
  set_memory_touch_handler(NULL);
  rw_tracker_enabled = enabled;
}

void rw_tracker_enable() { 
  rw_tracker_enabled = 1;
}
void rw_tracker_disable() {
  rw_tracker_disarm();
  rw_tracker_enabled = 0;
}

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

void find_rw_set(func_ptr exe, set_t* read, set_t* write) {
  // Initialize single stepping & threads
  equiv_init();

  // Disable context switching
  disable_ctx_switch();

  current_tracker = rw_tracker_mk(read, write);

  // Enable the RW tracker
  rw_tracker_enable();

  // Run our function
  eq_th_t* t = equiv_fork(exe, NULL, 0);
  equiv_run();

  // Clean up
  rw_tracker_disable();
  reset_ntids();
}

void find_pc_set(func_ptr exe, set_t* shared_memory, set_t* pcs) {
  // Initialize single stepping & threads
  equiv_init();

  // Disable context switching
  disable_ctx_switch();

  current_tracker = pc_tracker_mk(shared_memory, pcs);

  // Enable the RW tracker
  rw_tracker_enable();

  // Run our function
  eq_th_t* t = equiv_fork(exe, NULL, 0);
  equiv_run();

  // Clean up
  rw_tracker_disable();
  reset_ntids();
}



// Finds
