
#include "rpi.h"
#include "pt-vm.h"
#include "mmu.h"       

// from last lab.
#include "switchto.h"
#include "full-except.h"

enum { OneMB = 1024*1024};

enum { 
        dom_kern = 1, // domain id for kernel
        heap_dom = 2,  // domain id for user
    };   


cp_asm(control_reg, 15, 0, c1, c0, 0)

// used to store the illegal address we will write.
static volatile uint32_t illegal_addr;
static volatile uint32_t legal_addr;

// a trivial fault handler that checks that we got the fault
// we expected.
static void data_fault_handler(regs_t *r) {
    // renable domain
    uint32_t d = ((DOM_manager) << (heap_dom*2)) | ((DOM_manager) << (dom_kern*2));
    staff_domain_access_ctrl_set(d);
    uint32_t fault_addr;

    // b4-44
    asm volatile("MRC p15, 0, %0, c6, c0, 0" : "=r" (fault_addr));

    // make sure we faulted on the address that should be accessed.
    trace("SUCCESS!: got a fault on address=%x\n", fault_addr);

    // done with test.

    // print the pc
    trace("pc=%x\n", r->regs[15]);
    // get the dfsr
    uint32_t dfsr;
    asm volatile("MRC p15, 0, %0, c5, c0, 0" : "=r" (dfsr));
    trace("dfsr=%x\n", dfsr);

    
    return; 
}

void p_fault_handler(regs_t *r) {
    // renable domain
    uint32_t d = ((DOM_manager) << (heap_dom*2)) | ((DOM_manager) << (dom_kern*2));
    staff_domain_access_ctrl_set(d);
    uint32_t fault_addr;

    // b4-44
    asm volatile("MRC p15, 0, %0, c6, c0, 0" : "=r" (fault_addr));

    // make sure we faulted on the address that should be accessed.
    trace("SUCCESS on p!: got a fault on address=%x\n", fault_addr);

    // done with test.

    // print the pc
    trace("pc=%x\n", r->regs[15]);
    // get the ifsr
    uint32_t ifsr;
    asm volatile("MRC p15, 0, %0, c6, c0, 1" : "=r" (ifsr));
    trace("ifsr=%x\n", ifsr);

    
    return; 
}

void notmain(void) { 
    assert(!mmu_is_enabled());
    trace("1\n");

    // ******************************************************
    // 1. one-time initialization.
    //   - create an empty page table (to catch errors).
    //   - initialize all the vm hardware
    //   - compute permissions for no-user-access.


    // map the heap: for lab cksums must be at 0x100000.
    kmalloc_init_set_start((void*)OneMB, OneMB);

    // if we are correct this will never get accessed.
    // since all valid entries are pinned.
    vm_pt_t* idx = vm_pt_alloc(4096);
    trace("2\n");

    // initialize everything, after bootup.
    // <mmu.h>
    staff_mmu_init();

    // definitions in <pinned-vm.h>
    //
    // see 3-151 for table, or B4-9
    //    given: APX = 0, AP = 1
    //    the bits are: APX << 2 | AP
    uint32_t AXP = 0;
    uint32_t AP = 1;
    uint32_t no_user = AXP << 2 | 1; // no access user (privileged only)

    trace("3\n");
    // armv6 has 16 different domains with their own privileges.
    // just pick one for the kernel.       

    // ******************************************************
    // 2. setup device memory.
    // 
    // permissions: kernel domain, no user access, 
    // memory rules: strongly ordered, not shared.
    pin_t dev  = pin_mk_global(dom_kern, no_user, MEM_device);
    
    // map all device memory to itself.  ("identity map")
    vm_map_sec(idx, 0x20000000, 0x20000000, dev);   // tlb 0
    vm_map_sec(idx, 0x20100000, 0x20100000, dev);   // tlb 1
    vm_map_sec(idx, 0x20200000, 0x20200000, dev);   // tlb 2

    // ******************************************************
    // 3. setup kernel memory: 

    // protection: same as device.
    // memory rules: uncached access.
    pin_t kern = pin_mk_global(dom_kern, no_user, MEM_uncached);
    trace("4\n");

    // Q1: if you uncomment this, what happens / why?
    // kern = pin_mk_global(dom_kern, perm_ro_priv, MEM_uncached);

    // map first two MB for the kernel (1: code, 2: heap).
    //
    // NOTE: obviously it would be better to not have 0 (null) 
    // mapped, but our code starts at 0x8000 and we are using
    // 1mb sections.  you can fix this as an extension.  
    // very useful!
    vm_map_sec(idx, 0, 0, kern);                    // tlb 3

    // now map kernel stack (or nothing will work)
    uint32_t kern_stack = STACK_ADDR-OneMB;
    vm_map_sec(idx, kern_stack, kern_stack, kern);   // tlb 5
    uint32_t except_stack = INT_STACK_ADDR-OneMB;

    // Q2: if you comment this out, what happens?
    vm_map_sec(idx, except_stack, except_stack, kern);

    // ******************************************************
    // 4. setup vm hardware.
    //  - page table, asid, pid.
    //  - domain permissions.

    // b4-42: give permissions for all domains.

    // Q3: if you set this to ~0, what happens w.r.t. Q1?
    staff_domain_access_ctrl_set((DOM_client << dom_kern * 2) | (DOM_no_access << heap_dom * 2));
    enum { ASID = 1, PID = 128 };
    vm_mmu_switch(idx, PID, ASID);

    // if you want to see the lockdown entries.
    // lockdown_print_entries("about to turn on first time");

    // ******************************************************
    // 5. turn it on/off, checking that it worked.
    trace("about to enable\n");
    for (int i = 0; i < 10; i++)
    {
        staff_mmu_enable();

        if (mmu_is_enabled())
            trace("MMU ON: hello from virtual memory!  cnt=%d\n", i);
        else
            panic("MMU is not on?\n");

        staff_mmu_disable();
        assert(!mmu_is_enabled());
        trace("MMU is off!\n");
    }

    // b4-42: give permissions for all domains.

    // Q3: if you set this to ~0, what happens w.r.t. Q1?

    // set address space id, page table, and pid.
    // note:
    //  - pid never matters, it's just to help the os.
    //  - asid doesn't matter for this test b/c all entries 
    //    are global
    //  - the page table is empty (since pinning) and is
    //    just to catch errors.

    // if you want to see the lockdown entries.
    // lockdown_print_entries("about to turn on first time");

    // ******************************************************
    // 6. setup exception handling and make sure we get a fault.

    
    // just like last lab.  setup a data abort handler.
    full_except_install(0);
    full_except_set_data_abort(data_fault_handler);

    // the address we will write to (2MB) we know this is not mapped.
    legal_addr = OneMB; 

    // non-global entry with user permissions. user attribute: not global.  
    pin_t user1 = pin_mk_global(heap_dom, no_user, MEM_uncached); // 6
    trace("6\n");
    
    // read/write the first mb.
    uint32_t user_addr = OneMB;
    assert((user_addr>>12) % 16 == 0);
    uint32_t phys_addr1 = user_addr;
    PUT32(phys_addr1, 0xdeadbeef);

    // user mapping: do an ident.
    vm_map_sec(idx, user_addr, phys_addr1, user1);
    trace("7\n");

    uint32_t d = ((DOM_no_access) << (heap_dom*2)) | ((DOM_manager) << (dom_kern*2));
    staff_domain_access_ctrl_set(d);
    trace("8\n");

    // turn on the mmu.
    staff_mmu_enable();
    trace("MMU is on and working!\n");
    GET32(user_addr);
    trace("GET32 works now\n");
    trace("9\n");
    

    d = ((DOM_no_access) << (heap_dom*2)) | ((DOM_manager) << (dom_kern*2));
    staff_domain_access_ctrl_set(d);
    PUT32(user_addr, 1);
    trace("PUT32 works now\n");
    trace("10\n");

    PUT32(user_addr, 0xe12fff1e);
    typedef void (*voidfn)(void);
    voidfn f = (voidfn)user_addr;
    
    d = ((DOM_no_access) << (heap_dom*2)) | ((DOM_manager) << (dom_kern*2));
    staff_domain_access_ctrl_set(d);
    BRANCHTO(user_addr);
    //f();

    trace("Finished calling function\n");
}