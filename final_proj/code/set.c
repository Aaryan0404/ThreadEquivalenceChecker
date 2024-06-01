#include "set.h"
#include "rpi.h"
#include "equiv-malloc.h"

void set_mk(set_t* s, uint32_t offset) {
  s->mask = 0;
  s->offset = offset;
  for(int i = 0; i < 32; i++) s->children[i] = NULL;
}

set_t* set_alloc(uint32_t offset) {
  set_t* s = (set_t*)equiv_malloc(sizeof(set_t));
  if(s == NULL) panic("set allocation failed!");
  set_mk(s, offset);
  return s;
}

/*
 * Returns >0 if mask has bit
 */
static inline uint32_t mask_has(uint32_t mask, uint32_t bit) {
  return mask & (1 << bit);
}

void set_print_recurse(set_t* s, uint32_t prefix) {
  if(s->offset > 0) {
    for(int i = 0; i < 32; i++) {
      if(mask_has(s->mask, i)) {
        set_print_recurse(s->children[i], (prefix << 5) | i);
      }
    }
  } else {
    for(int i = 0; i < 32; i++) {
      if(mask_has(s->mask, i)) {
        uint32_t v = (prefix << 5) | i;
        printk("\t%x\n", v);
      }
    }
  }
}

void set_print(const char* msg, set_t* s) {
  if(msg) printk(msg);

  set_print_recurse(s, 0);
}

void set_free(set_t* s) {
  for(int i = 0; i < 32; i++) {
    if(s->children[i] != NULL) set_free(s->children[i]);
  }
  equiv_free(s);
}

void set_copy(set_t* dst, set_t* src) {
  dst->offset = src->offset;
  dst->mask = src->mask;
  if(src->offset > 0) {
    for(int i = 0; i < 32; i++) {
      uint32_t bit = 0x1 << i;
      if(src->mask & bit) {
        dst->children[i] = set_alloc(src->offset - 5);
        set_copy(dst->children[i], src->children[i]);
      }
    }
  }
}

uint32_t set_insert(set_t* s, uint32_t v) {
  uint32_t index = (v >> s->offset) & 0x1F;

  uint32_t bit = 0x1 << index;
  uint32_t present = s->mask & bit;

  s->mask |= bit;

  // If this isn't a leaf node, we need to recurse
  if(s->offset > 0) {
    // If the child doesn't exist, make it
    if(!present) {
      s->children[index] = set_alloc(s->offset - 5);
    }
    return set_insert(s->children[index], v);
  }

  return present;
}

uint32_t set_lookup(set_t* s, uint32_t v) {
  uint32_t index = (v >> s->offset) & 0x1F;

  uint32_t bit = 0x1 << index;
  uint32_t present = s->mask & bit;

  // If the prefix is not present in the set, give up
  if(!present) return 0;

  // If the prefix is present and we are a leaf node, return the present bit
  if(s->offset == 0) return present >> index;

  // Otherwise recurse
  return set_lookup(s->children[index], v);
}

void set_union(set_t* z, set_t* x, set_t* y) {
  assert(z->offset == x->offset && x->offset == y->offset);

  z->mask = x->mask | y->mask;

  if(z->offset == 0) return;

  uint32_t both_present = x->mask & y->mask;
  uint32_t at_least_one_present = x->mask | y->mask;
  for(int i = 0; i < 32; i++) {
    if(mask_has(both_present, i)) {
      // Make the child
      z->children[i] = set_alloc(z->offset - 5);

      // Recursively call union
      set_union(z->children[i], x->children[i], y->children[i]);
    } else if(mask_has(at_least_one_present, i)) {
      // Make the child
      z->children[i] = set_alloc(z->offset - 5);

      // Just take the one that is present
      if(mask_has(x->mask, i)) {
        set_copy(z->children[i], x->children[i]);
      } else if(mask_has(y->mask, i)) {
        set_copy(z->children[i], y->children[i]);
      }
    }
  }
}

void set_union_inplace(set_t* y, set_t* x) {
  assert(y->offset == x->offset);

  uint32_t both_present = y->mask & x->mask;
  y->mask |= x->mask;

  if(y->offset == 0) return;
  
  for(int i = 0; i < 32; i++) {
    if(mask_has(both_present, i)) {
      // Recursively call union
      set_union_inplace(y->children[i], x->children[i]);
    } else if(mask_has(x->mask, i)) {
      // Y must not have the bit
      y->children[i] = set_alloc(x->offset - 5);
      set_copy(y->children[i], x->children[i]);
    }
  }
}

void set_intersection(set_t* z, set_t* x, set_t* y) {
  assert(z->offset == x->offset && x->offset == y->offset);
  
  uint32_t both_present = y->mask & x->mask;
  z->mask = both_present;

  if(z->offset == 0) return;

  for(int i = 0; i < 32; i++) {
    if(mask_has(both_present, i)) {
      z->children[i] = set_alloc(x->offset - 5);
      set_intersection(z->children[i], x->children[i], y->children[i]);
    }
  }
}

void set_intersection_inplace(set_t* y, set_t* x) {
  assert(y->offset == x->offset);

  uint32_t both_present = y->mask & x->mask;
  uint32_t only_y = y->mask & ~x->mask;
  uint32_t only_x = x->mask & ~y->mask;

  y->mask &= x->mask;
  if(y->offset == 0) return;

  for(int i = 0; i < 32; i++) {
    // Free children present in y but not in x
    if(mask_has(only_y, i)) {
      set_free(y->children[i]);
      y->children[i] = NULL;
    // Copy children present in x but not in y
    } else if(mask_has(only_x, i)) {
      y->children[i] = set_alloc(y->offset - 5);
      set_copy(y->children[i], x->children[i]);
    // Recurse on shared children
    } else if(mask_has(both_present, i)) {
      set_intersection_inplace(y->children[i], x->children[i]);
    }
  }
}
