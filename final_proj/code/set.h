#ifndef __SET_H
#define __SET_H

/*
 * Set with 32-bit valued items.
 */

/* Naive list implementation */
typedef struct {
  uint32_t* array;
  uint32_t array_size;
} set_t;

/*
 * Allocates an empty set.
 */
set_t* set_alloc();

/*
 * Initializes an empty set.
 */
void set_mk(set_t* s);

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

