#include "rpi.h"

static inline void atomic_increment(volatile int *ptr) {
    int temp, fail;
    unsigned int timeout = 1000; // Set a timeout value
    asm volatile (
        "1:     ldrex   %0, [%2]    \n" // Load the value at ptr into temp
        "       add     %0, %0, #1  \n" // Increment temp
        "       strex   %1, %0, [%2] \n" // Store temp back to ptr, set fail if failed
        "       teq     %1, #0      \n" // Test if store succeeded
        "       bne     2f          \n" // Skip to the end if store succeeded
        "       subs    %3, %3, #1  \n" // Decrement timeout
        "       bne     1b          \n" // Retry if timeout not reached
        "2:                         \n"
        : "=&r" (temp), "=&r" (fail)
        : "r" (ptr), "r" (timeout)
        : "cc", "memory"
    );
    printk("Incrementing counter\n");
}

static inline void atomic_decrement(volatile int *ptr) {
    int temp, fail;
    unsigned int timeout = 1000; // Set a timeout value
    asm volatile (
        "1:     ldrex   %0, [%2]    \n" // Load the value at ptr into temp
        "       sub     %0, %0, #1  \n" // Decrement temp
        "       strex   %1, %0, [%2] \n" // Store temp back to ptr, set fail if failed
        "       teq     %1, #0      \n" // Test if store succeeded
        "       bne     2f          \n" // Skip to the end if store succeeded
        "       subs    %3, %3, #1  \n" // Decrement timeout
        "       bne     1b          \n" // Retry if timeout not reached
        "2:                         \n"
        : "=&r" (temp), "=&r" (fail)
        : "r" (ptr), "r" (timeout)
        : "cc", "memory"
    );
    printk("Decrementing counter\n");
}

void notmain() {
    printk("Testing atomic increment and decrement\n");
    volatile int counter = 0;
    
    atomic_increment(&counter);
    printk("Counter after increment: %d\n", counter);
    
    atomic_decrement(&counter);
    printk("Counter after decrement: %d\n", counter);
}
