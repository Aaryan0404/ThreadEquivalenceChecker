#ifndef __SET_H
#define __SET_H

#include "rpi.h"

#define MAX_OFFSET 30

/*
 * Set with 32-bit valued items.
 */

typedef struct set_t {
  uint32_t mask;
  // The number of number of bits between the LSB of the index bits and the LSB
  // of the item
  uint32_t offset;
  struct set_t* children[32];
} set_t;

/*
 * Allocates an empty set.
 */
set_t* set_alloc(uint32_t offset);

/*
 * Frees a set
 */
void set_free();

/*
 * Initializes an empty set.
 */
void set_mk(set_t* s, uint32_t offset);

/*
 * Prints a set in-order
 */
void set_print(const char* msg, set_t* s);

/*
 * Copies a set. Makes no assumptions about destination.
 */
void set_copy(set_t* dest, set_t* src);

/* 
 * Inserts v into s. Returns 1 if set did not previously have value, otherwise
 * returns 0.
 */
uint32_t set_insert(set_t* s, uint32_t v);

/*
 * Looks up v in s. Returns 1 if set contains v, 0 otherwise.
 */
uint32_t set_lookup(set_t* s, uint32_t v);

void set_union(set_t* z, set_t* x, set_t* y);
void set_union_inplace(set_t* y, set_t* x);

void set_intersection(set_t* z, set_t* x, set_t* y);
void set_intersection_inplace(set_t* y, set_t* x);

#endif
