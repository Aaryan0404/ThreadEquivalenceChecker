#ifndef EQUIV_CHECKER_HELPERS_H
#define EQUIV_CHECKER_HELPERS_H

#define EQUIV_USER __attribute__((section(".user")))

#include "equiv-threads.h"

#define LOCKED 1
#define UNLOCKED 0

typedef uint32_t vibe_check_t;

EQUIV_USER
static inline uint32_t atomic_increment(uint32_t* x) {
  uint32_t result;
  uint32_t tmp;
  asm volatile (
   "1: ldrex %0, [%2]\n"     // Load the value from ptr into result
   "   add %0, %0, #1\n"     // Increment the value in result
   "   strex %1, %0, [%2]\n" // Attempt to store the incremented value back to ptr
   "   teq %1, #0\n"         // Check if the store was successful
   "   bne 1b\n"             // If not, retry (branch to label 1)
   : "=&r" (result), "=&r" (tmp)
   : "r" (x)
   : "memory", "cc"
  );
  return result;
}

EQUIV_USER
static inline uint32_t atomic_compare_and_swap(uint32_t* x, uint32_t old_val, uint32_t new_val) {
  uint32_t result;
  uint32_t status;
  asm volatile (
   "mov %1, #1\n"
   "ldrex %0, [%2]\n"         // Load the value from ptr into result
   "teq %0, %3\n"             // Compare result with old_val
   "strexeq %1, %4, [%2]\n"   // Attempt to store new_val to ptr if equal
   : "=&r" (result), "=&r" (status)
   : "r" (x), "r" (old_val), "r" (new_val)
   : "memory", "cc"
  );
  return status;
}


EQUIV_USER
static inline int vibe_check(vibe_check_t *cur_vibes) {
  if(atomic_compare_and_swap(cur_vibes, UNLOCKED, LOCKED) == 0) return UNLOCKED;
  else return LOCKED;
}

EQUIV_USER
static inline void secure_vibes(vibe_check_t *cur_vibes) {
  while(atomic_compare_and_swap(cur_vibes, UNLOCKED, LOCKED) != 0) {
    sys_equiv_yield();
  }
}

EQUIV_USER
static inline void release_vibes(vibe_check_t *cur_vibes) {
  *cur_vibes = UNLOCKED;
}

// Spin lock functions
static inline void vibe_init(vibe_check_t *cur_vibes) {
    *cur_vibes = UNLOCKED; // 0 indicates that the lock is available
}

#endif
