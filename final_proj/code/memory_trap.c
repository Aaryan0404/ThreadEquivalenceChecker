#include "rpi.h"
#include "pt-vm.h"
#include "mmu.h"
#include "switchto.h"
#include "full-except.h"

enum { OneMB = 1024*1024};
enum { 
        dom_kern = 1, // domain id for kernel
        heap_dom = 2,  // domain id for user
    };   

cp_asm(control_reg, 15, 0, c1, c0, 0)

void disable_heap_domain_access(){
    uint32_t d = ((DOM_no_access) << (heap_dom*2)) | ((DOM_manager) << (dom_kern*2));
    staff_domain_access_ctrl_set(d);
}

void enable_heap_domain_access(){
    uint32_t d = ((DOM_manager) << (heap_dom*2)) | ((DOM_manager) << (dom_kern*2));
    staff_domain_access_ctrl_set(d);
}

static void data_fault_handler(regs_t *r) {
    // renable domain
    trace("data fault pc=%x\n", r->regs[15]);
    enable_heap_domain_access();
    return;
    uint32_t fault_addr;

    // b4-44
    asm volatile("MRC p15, 0, %0, c6, c0, 0" : "=r" (fault_addr));

    // make sure we faulted on the address that should be accessed.
    trace("SUCCESS!: got a fault on address=%x\n", fault_addr);

    // done with test.

    // print the pc
    
    // get the dfsr
    uint32_t dfsr;
    asm volatile("MRC p15, 0, %0, c5, c0, 0" : "=r" (dfsr));
    trace("dfsr=%x\n", dfsr);

    return; 
}

void init_memory_trap(){
    assert(!mmu_is_enabled());

    // map the heap: for lab cksums must be at 0x100000.
    //kmalloc_init_set_start((void*)OneMB, OneMB);
    // kmalloc_init();

    // if we are correct this will never get accessed.
    // since all valid entries are pinned.
    vm_pt_t* idx = vm_pt_alloc(4096);

    // initialize everything, after bootup.
    // <mmu.h>
    staff_mmu_init();

    uint32_t AXP = 0;
    uint32_t AP = 1;
    uint32_t no_user = AXP << 2 | 1; // no access user (privileged only)
    assert(perm_rw_priv == no_user);

    // 2. setup device memory.
    // 
    // permissions: kernel domain, no user access, 
    // memory rules: strongly ordered, not shared.
    pin_t dev  = pin_mk_global(dom_kern, no_user, MEM_device);

    // map all device memory to itself.  ("identity map")
    vm_map_sec(idx, 0x20000000, 0x20000000, dev);   // tlb 0
    vm_map_sec(idx, 0x20100000, 0x20100000, dev);   // tlb 1
    vm_map_sec(idx, 0x20200000, 0x20200000, dev);   // tlb 2

    // 3. setup kernel memory: 
    // protection: same as device.
    // memory rules: uncached access.
    pin_t kern = pin_mk_global(dom_kern, no_user, MEM_uncached);

    vm_map_sec(idx, 0, 0, kern);                    // tlb 3

    // now map kernel stack (or nothing will work)
    uint32_t kern_stack = STACK_ADDR-OneMB;
    vm_map_sec(idx, kern_stack, kern_stack, kern);   // tlb 5
    uint32_t except_stack = INT_STACK_ADDR-OneMB;

    // Q2: if you comment this out, what happens?
    vm_map_sec(idx, except_stack, except_stack, kern);

    // Q3: if you set this to ~0, what happens w.r.t. Q1?
    staff_domain_access_ctrl_set((DOM_client << dom_kern * 2) | (DOM_no_access << heap_dom * 2));
    enum { ASID = 1, PID = 128 };
    vm_mmu_switch(idx, PID, ASID);

    // ******************************************************
    // 5. turn it on/off, checking that it worked.
    
    // setup a data abort handler.
    // full_except_install(0);
    full_except_set_data_abort(data_fault_handler);

    pin_t user1 = pin_mk_global(heap_dom, no_user, MEM_uncached); // 6
    
    uint32_t user_addr = OneMB;
    vm_map_sec(idx, user_addr, user_addr, user1);

    trace("about to enable\n");
    staff_mmu_enable();
    trace("enabled\n");
}