/*============================================================================
* Lock-Free Ring Buffer (LFRB) for embedded systems
* GitHub: https://github.com/QuantumLeaps/lock-free-ring-buffer
*
*                    Q u a n t u m  L e a P s
*                    ------------------------
*                    Modern Embedded Software
*
* Copyright (C) 2005 Quantum Leaps, <state-machine.com>.
*
* SPDX-License-Identifier: MIT
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
============================================================================*/
#include <stdint.h>
#include <stdbool.h>
#include "rpi.h"
#include "equiv-checker.h"


/*! Ring buffer counter/index
*
* @attention
* The following RingBufCtr type is assumed to be "atomic" in a target CPU,
* meaning that the CPU needs to write the whole RingBufCtr in one machine
* instruction. An example of violating this assumption would be an 8-bit
* CPU, which writes uint16_t (2-bytes) in 2 machine instructions. For such
* 8-bit CPU, the maximum size of RingBufCtr would be uint8_t (1-byte).
*
* Another case of violating the "atomic" writes to RingBufCtr type would
* be misalignment of a RingBufCtr variable in memory, which could cause
* the compiler to generate multiple instructions to write a RingBufCtr value.
* Therefore, it is further assumed that all RingBufCtr variables in the
* following RingBuf struct *are* correctly aligned for "atomic" access.
* In practice, most C compilers should provide such natural alignment
* (by inserting some padding into the ::RingBuf struct, if necessary).
*/
typedef uint16_t RingBufCtr;

/*! Ring buffer element type
*
* @details
* The type of the ring buffer elements is not critical for the lock-free
* operation and does not need to be "atomic". For example, it can be
* an integer type (uint16_t, uint32_t, uint64_t), a pointer type,
* or even a struct type. However, the bigger the type the more RAM is
* needed for the ring buffer and more CPU cycles are needed to copy it
* into and out of the buffer memory.
*/
typedef uint8_t RingBufElement;

/*! Ring buffer struct */
typedef struct {
    RingBufElement *buf; /*!< pointer to the start of the ring buffer */
    RingBufCtr end;  /*!< offset of the end of the ring buffer */
    RingBufCtr head; /*!< offset to where next el. will be inserted */
    RingBufCtr tail; /*!< offset of where next el. will be removed */
} RingBuf;

/*! Ring buffer callback function for RingBuf_process_all()
*
* @details
* The callback processes one element and runs in the context of
* RingBuf_process_all().
*/
typedef void (*RingBufHandler)(RingBufElement const el);

/*..........................................................................*/
void RingBuf_ctor(RingBuf * const me,
                  RingBufElement sto[], RingBufCtr sto_len) {
    me->buf  = &sto[0];
    me->end  = sto_len;
    me->head = 0U;
    me->tail = 0U;
}
/*..........................................................................*/
EQUIV_USER
bool RingBuf_put(RingBuf * const me, RingBufElement const el) {
    RingBufCtr head = me->head + 1U;
    if (head == me->end) {
        head = 0U;
    }
    if (head != me->tail) { /* buffer NOT full? */
        me->buf[me->head] = el; /* copy the element into the buffer */
        me->head = head; /* update the head to a *valid* index */
        return true;  /* element placed in the buffer */
    }
    else {
        return false; /* element NOT placed in the buffer */
    }
}
/*..........................................................................*/
EQUIV_USER
bool RingBuf_get(RingBuf * const me, RingBufElement *pel) {
    RingBufCtr tail = me->tail;
    if (me->head != tail) { /* ring buffer NOT empty? */
        *pel = me->buf[tail];
        ++tail;
        if (tail == me->end) {
            tail = 0U;
        }
        me->tail = tail; /* update the tail to a *valid* index */
        return true;
    }
    else {
        return false;
    }
}
/*..........................................................................*/
EQUIV_USER
RingBufCtr RingBuf_num_free(RingBuf * const me) {
    RingBufCtr head = me->head;
    RingBufCtr tail = me->tail;
    if (head == tail) { /* buffer empty? */
        return (RingBufCtr)(me->end - 1U);
    }
    else if (head < tail) {
        return (RingBufCtr)(tail - head - 1U);
    }
    else {
        return (RingBufCtr)(me->end + tail - head - 1U);
    }
}

/*..........................................................................*/
EQUIV_USER
void RingBuf_process_all(RingBuf * const me, RingBufHandler handler) {
    RingBufCtr tail = me->tail;
    while (me->head != tail) { /* ring buffer NOT empty? */
        (*handler)(me->buf[tail]);
        ++tail;
        if (tail == me->end) {
            tail = 0U;
        }
        me->tail = tail; /* update the tail to a *valid* index */
    }
}

RingBufCtr rb_size;
RingBufElement buf[8];
RingBuf rb;

#define N 2

EQUIV_USER
void func_producer(void** arg) {
  gcc_mb();
  for(int i = 0; i < N; i++)
    while(!RingBuf_put(&rb, i)) gcc_mb();
  gcc_mb();
}

EQUIV_USER
void func_consumer(void** arg) {
  RingBufElement el;
  while(RingBuf_num_free(&rb) < rb_size) {
    gcc_mb();
    RingBuf_get(&rb, &el);
    gcc_mb();
  }
}

void init_memory() {
  for(int i = 0; i < 8; i++) buf[i] = 0;
  RingBuf_ctor(&rb, buf, sizeof(buf));
  rb_size = RingBuf_num_free(&rb);
}

#define NUM_FUNCS 2
#define NUM_CTX 2

void notmain() {    
    equiv_checker_init();
    set_verbosity(3);

    init_memory();

    function_exec *executables = kmalloc(NUM_FUNCS * sizeof(function_exec));
    executables[0].func_addr = (func_ptr)func_producer;
    executables[1].func_addr = (func_ptr)func_consumer;

    /* 
     * We need a shared memory hint because of the consumer branch. It's ok if
     * you include too much in this as long as you initialize properly and your
     * functions are deterministic.
     */
    set_t* shared_mem_hint = set_alloc();
    add_mem(shared_mem_hint, &buf, sizeof(buf));
    add_mem(shared_mem_hint, &rb.head, sizeof(rb.head));
    add_mem(shared_mem_hint, &rb.tail, sizeof(rb.tail));
    //set_t* shared_mem_hint = NULL;


    memory_tags_t t = mk_tags(10);
    add_tag(&t, &rb.head, "rb.head");
    add_tag(&t, &rb.tail, "rb.tail");
    add_tag(&t, &buf[0], "buf[0]");
    add_tag(&t, &buf[1], "buf[1]");
    add_tag(&t, &buf[2], "buf[2]");
    add_tag(&t, &buf[3], "buf[3]");
    add_tag(&t, &buf[4], "buf[4]");
    add_tag(&t, &buf[5], "buf[5]");
    add_tag(&t, &buf[6], "buf[6]");
    add_tag(&t, &buf[7], "buf[7]");

    equiv_checker_run(executables, NUM_FUNCS, NUM_CTX, init_memory, shared_mem_hint, &t);
}
