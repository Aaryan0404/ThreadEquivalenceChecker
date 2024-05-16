#include "rpi.h"
#include "pt-vm.h"
#include "helper-macros.h"
#include "procmap.h"

// turn this off if you don't want all the debug output.
enum { verbose_p = 0 };
enum { OneMB = 1024*1024 };

#define N_PHYSICAL_PAGE 4096
static uint8_t taken_list[N_PHYSICAL_PAGE];

void vm_taken_list_print(void) {
  for(int i = 0; i < N_PHYSICAL_PAGE; i++) {
    if(taken_list[i]) {
      printk("physical page %d (%x to %x) is taken\n", i, i * OneMB, (i+1)*OneMB);
    }
  }
}

uint32_t get_first_free_page(void) {
  for(uint32_t i = 0; i < N_PHYSICAL_PAGE; i++) {
    if(!taken_list[i]) return i;
  }
  panic("OOM\n");
}

vm_pt_t *vm_pt_alloc(unsigned n) {
    demand(n == 4096, we only handling a fully-populated page table right now);

    vm_pt_t *pt = 0;
    unsigned nbytes = 4096 * sizeof *pt;

    pt = (vm_pt_t*)kmalloc_aligned(nbytes, 1 << 14);

    // allocate pt with n entries.
    //pt = staff_vm_pt_alloc(n);

    demand(is_aligned_ptr(pt, 1<<14), must be 14-bit aligned!);
    return pt;
}

// allocate new page table and copy pt.
vm_pt_t *vm_dup(vm_pt_t *pt1) {
    vm_pt_t *pt2 = vm_pt_alloc(PT_LEVEL1_N);
    memcpy(pt2,pt1,PT_LEVEL1_N * sizeof *pt1);
    return pt2;
}

// same as pinned version: probably should check that
// the page table is set.
void vm_mmu_enable(void) {
    assert(!mmu_is_enabled());
    mmu_enable();
    assert(mmu_is_enabled());
}

// same as pinned version.
void vm_mmu_disable(void) {
    assert(mmu_is_enabled());
    mmu_disable();
    assert(!mmu_is_enabled());
}

// - set <pt,pid,asid> for an address space.
// - must be done before you switch into it!
// - mmu can be off or on.
void vm_mmu_switch(vm_pt_t *pt, uint32_t pid, uint32_t asid) {
    assert(pt);
    mmu_set_ctx(pid, asid, pt);
}

// just like pinned.
void vm_mmu_init(uint32_t domain_reg) {
    // initialize everything, after bootup.
    mmu_init();
    domain_access_ctrl_set(domain_reg);
}

// map the 1mb section starting at <va> to <pa>
// with memory attribute <attr>.
vm_pte_t *
vm_map_sec(vm_pt_t *pt, uint32_t va, uint32_t pa, pin_t attr) 
{
    assert(aligned(va, OneMB));
    assert(aligned(pa, OneMB));

    // today we just use 1mb.
    assert(attr.pagesize == PAGE_1MB);

    // Flag physical page as taken
    uint32_t phys_segment = pa >> 20;
    demand(phys_segment < N_PHYSICAL_PAGE, "physical segment too big: %d\n", phys_segment);
    taken_list[phys_segment] = 1;

    unsigned index = va >> 20;
    assert(index < PT_LEVEL1_N);

    //vm_pte_t *pte = 0;
    //return staff_vm_map_sec(pt,va,pa,attr);
    vm_pte_t *pte = &pt[index];
    vm_pte_t entry = {
      // B4-29
      .tag = 0b10,

      .B = mem_attr_B(attr.mem_attr),
      .C = mem_attr_C(attr.mem_attr),
      
      // B4-25
      .XN = 0,
      
      .domain = attr.dom,
      
      // B4-28
      .IMP = 0,
      
      .AP = attr.AP_perm & 0b11,
      .TEX = mem_attr_TEX(attr.mem_attr),
      .APX = attr.AP_perm >> 2,
      
      // B4-9
      .S = 0,

      .nG = ~attr.G,
      
      // We assert pagesize == PAGE_1MB
      .super = 0,


      // To be safe
      ._sbz1 = 0,


      // B4-29
      .sec_base_addr = pa >> 20,
    };
    *pte = entry;


    if(verbose_p)
        vm_pte_print(pt,pte);
    assert(pte);
    return pte;
}

// lookup 32-bit address va in pt and return the pte
// if it exists, 0 otherwise.
vm_pte_t * vm_lookup(vm_pt_t *pt, uint32_t va) {
    vm_pte_t pte = pt[va >> 20];
    if(pte.tag == 0) return 0;
    return &pt[va >> 20];
}

// manually translate <va> in page table <pt>
// - if doesn't exist, returns 0.
// - if does exist returns the corresponding physical
//   address in <pa>
//
// NOTE: we can't just return the result b/c page 0 could be mapped.
vm_pte_t *vm_xlate(uint32_t *pa, vm_pt_t *pt, uint32_t va) {
    vm_pte_t pte = pt[va >> 20];
    if(pte.tag == 0) return 0;
    uint32_t section_base = pte.sec_base_addr;
    *pa = pte.sec_base_addr << 20 | (va & 0xfffff);
    return &pt[va >> 20];
}

// compute the default attribute for each type.
static inline pin_t attr_mk(pr_ent_t *e) {
    switch(e->type) {
    case MEM_DEVICE: 
        return pin_mk_device(e->dom);
    // kernel: currently everything is uncached.
    case MEM_RW:
        return pin_mk_global(e->dom, perm_rw_priv, MEM_uncached);
   case MEM_RO: 
        panic("not handling\n");
   default: 
        panic("unknown type: %d\n", e->type);
    }
}

// setup the initial kernel mapping.  This will mirror
//  static inline void procmap_pin_on(procmap_t *p) 
// in <15-pinned-vm/code/procmap.h>  but will call
// your vm_ routines, not pinned routines.
//
// if <enable_p>=1, should enable the MMU.  make sure
// you setup the page table and asid. use  
// kern_asid, and kern_pid.
vm_pt_t *vm_map_kernel(procmap_t *p, int enable_p) {
    // the asid and pid we start with.  
    //    shouldn't matter since kernel is global.
    enum { kern_asid = 1, kern_pid = 0x140e };

    vm_pt_t *pt = vm_pt_alloc(4096);

    //return staff_vm_map_kernel(p,enable_p);

    vm_mmu_switch(pt, kern_pid, kern_asid);

    for(int i = 0; i < p->n; i++) {
      pr_ent_t *e = &p->map[i];
       demand(e->nbytes == MB, "nbytes=%d\n", e->nbytes);

       pin_t g;
       switch(e->type) {
       case MEM_DEVICE:
         g = pin_mk_device(e->dom);
         break;
       case MEM_RW:
         // currently everything is uncached.
         g = pin_mk_global(e->dom, perm_rw_priv, MEM_uncached);
         break;
       case MEM_RO: panic("not handling\n");
       default: panic("unknown type: %d\n", e->type);
       }
       vm_map_sec(pt, e->addr, e->addr, g);
    }

    uint32_t d = dom_perm(p->dom_ids, DOM_client);
    vm_mmu_init(d);

    if(enable_p) {
      vm_mmu_enable();
    }

    assert(pt);
    return pt;
}
