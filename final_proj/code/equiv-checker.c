#include "equiv-checker.h"
#include "procmap.h"
#include "mem-attr.h"
#include "pt-vm.h"
#include "equiv-malloc.h"

void equiv_copy_user_data() { 

  // copy everything from 100K to 200K 
  // to memory location 0x300000

  for (int i = 0; i < (100 * 1024); i++) {
    char* src = (char*)(i + (100 * 1024)); 
    char* dst = (char*)(0x300000 + i);

    *dst = *src;
  }

}

void equiv_checker_init() {
  // Initialize the general heap
  enum { MB = 1024 * 1024 };
  kmalloc_init_set_start((void*)MB, MB);

  // Initialize the equiv checker heap
  uint32_t heap_size = 1024 * 512;
  void* heap_start = kmalloc(heap_size);
  equiv_malloc_init(heap_start, heap_size);

  equiv_copy_user_data();

  // For now just map the kernel
  procmap_t kernel_map = procmap_default_mk(kern_dom, user_dom); 
  vm_pt_t* pt = vm_map_kernel(&kernel_map, 1);
  uint32_t pa;
  vm_pte_t* pte = vm_xlate(&pa, pt, 0x8140);
  printk("%x\n", pa);

  domain_acl_print();
  output("MMU enabled\n");
}
