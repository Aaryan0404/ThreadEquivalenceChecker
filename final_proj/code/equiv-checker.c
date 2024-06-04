#include "equiv-checker.h"
#include "equiv-malloc.h"
#include "equiv-mmu.h"
#include "equiv-rw-set.h"
#include "permutations.h"
#include "interleaver.h"
#include "equiv-rw-set.h"
#include "memory.h"

void equiv_copy_user_data() { 

  // copy everything from 100K to 200K 
  // to memory location 0x300000

  for (int i = 0; i < (100 * 1024); i++) {
    if(i % 10000 == 0) printk(".");
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

  // For now just map the kernel
  procmap_t kernel_map = procmap_default_mk(kern_dom, user_dom); 
  vm_pt_t* pt = vm_map_kernel(&kernel_map, 1);
  uint32_t pa;
  vm_pte_t* pte = vm_xlate(&pa, pt, 0x8140);

  printk("Copying user data");
  equiv_copy_user_data();
  printk(" Copy complete.\n");

  // RW tracker
  rw_tracker_init(0);
}


void equiv_checker_run(
  function_exec *executables,
  uint32_t n_func,
  uint32_t ncs,
  init_memory_func init,
  set_t* additional_shared_memory,
  memory_tags_t* tags
) {
  assert(ncs);

  set_t* shared_memory = set_alloc();
  find_shared_memory(executables, n_func, shared_memory);
  if(additional_shared_memory) {
    set_union_inplace(shared_memory, additional_shared_memory);
  }

  const size_t num_perms = factorial(n_func);
  int **itl = get_func_permutations(n_func);

  set_t* valid_hashes = set_alloc();
  find_good_hashes(
    executables, n_func,
    init,
    itl, num_perms,
    shared_memory, valid_hashes
  );


  for(int i = 1; i <= ncs; i++) {
    printk("\nTrying %d context switches...\n", i);
    run_interleavings(
      executables,
      n_func,
      valid_hashes,
      init,
      i,
      shared_memory,
      tags
    );
  }

  set_free(valid_hashes);
  set_free(shared_memory);
}
