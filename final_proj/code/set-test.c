#include "rpi.h"
#include "set.h"
#include "equiv-malloc.h"

void notmain() {
  enum { MB = 1024 * 1024 };

  kmalloc_init_set_start((void*)MB, MB + 1024);
  uint32_t heap_size = MB;
  void* heap_start = kmalloc(heap_size);
  equiv_malloc_init(heap_start, heap_size);

  //caches_enable();

  set_t* s = set_alloc_offset(MAX_OFFSET);

  set_insert(s, 0x1);
  set_insert(s, 0x2);
  set_insert(s, 0x400);

  printk("Lookup:\n");
  printk("set[0x1]: %d\n", set_lookup(s, 0x1));
  printk("set[0x400]: %d\n", set_lookup(s, 0x400));
  printk("set[0x3]: %d\n", set_lookup(s, 0x3));

  set_print("Print set:\n", s);

  printk("\nTesting unions....\n");

  set_t* x = set_alloc_offset(MAX_OFFSET);
  set_insert(x, 0x1);
  set_insert(x, 0x2);
  set_print("Set X\n", x);

  printk("\n");

  set_t* y = set_alloc_offset(MAX_OFFSET);
  set_insert(y, 0x10001);
  set_insert(y, 0x1000002);
  set_print("Set Y\n", y);

  printk("\n");

  set_t* z = set_alloc_offset(MAX_OFFSET);
  set_union(z, x, y);
  set_print("X U Y\n", z);

  printk("\nTesting inplace unions....\n");

  set_t* a = set_alloc_offset(MAX_OFFSET);
  set_insert(a, 0x1);
  set_insert(a, 0x2);
  set_print("Set A\n", a);

  printk("\n");

  set_t* b = set_alloc_offset(MAX_OFFSET);
  set_insert(b, 0x10001);
  set_insert(b, 0x1000002);
  set_print("Set B\n", b);

  printk("\n");

  set_union_inplace(a, b);
  set_print("A U B\n", a);

  printk("\nTesting intersection....\n");

  a = set_alloc_offset(MAX_OFFSET);
  set_insert(a, 0x1);
  set_insert(a, 0x2);
  set_print("Set A\n", a);

  printk("\n");

  b = set_alloc_offset(MAX_OFFSET);
  set_insert(b, 0x2);
  set_insert(b, 0x3);
  set_print("Set B\n", b);

  printk("\n");

  set_t* c = set_alloc_offset(MAX_OFFSET);
  set_intersection(c, a, b);
  set_print("A & B\n", c);

  printk("\nTesting inplace intersection....\n");

  a = set_alloc_offset(MAX_OFFSET);
  set_insert(a, 0x1);
  set_insert(a, 0x2);
  set_print("Set A\n", a);

  printk("\n");

  b = set_alloc_offset(MAX_OFFSET);
  set_insert(b, 0x2);
  set_insert(b, 0x3);
  set_print("Set B\n", b);

  printk("\n");

  set_intersection_inplace(a, b);
  set_print("A & B\n", a);

}
