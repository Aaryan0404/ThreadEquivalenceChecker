#include "rpi-asm.h"

@ all of these are called from <switchto.h>: look there for more
@ help in understanding if need to.

@ drop in your code from last lab to get sp and lr.
@
@ void priv_get_sp_lr_asm(uint32_t mode, uint32_t *sp, uint32_t *lr);
@   - assumes caller is at privileged mode [we could check this]
@   - mode in r0
@   - sp pointer in r1
@   - lr pointer in r2
@
MK_FN(priv_get_sp_lr_asm)
    @ b staff_priv_get_sp_lr_asm
    @ asm_not_reached();
    mrs r3, cpsr
    msr cpsr, r0

    str sp, [r1]
    str lr, [r2]

    msr cpsr, r3

    bx lr
    asm_not_reached();


@ drop in your code from lab 13 [in 1-rfe-asm.S]
@
@ void switchto_user_asm(regs_t *r)  __attribute__((noreturn));
@  - <r> is a 17-entry register entry in r0.
@
@ switch to user mode loading r0-r15, and cpsr from <r>
MK_FN(switchto_user_asm)
    @ b staff_switchto_user_asm
    mov r13, r0
    ldm r13, {r0-r14}^

    add r13, r13, #60
    rfe r13
    asm_not_reached();

@ drop in your code from lab 13
@ 
@ void switchto_priv_asm(regs_t *r)  __attribute__((noreturn));
MK_FN(switchto_priv_asm)
    @ b staff_switchto_priv_asm
    ldr r1, [r0, #64]
    msr cpsr, r1
    prefetch_flush(r2)
    ldm r0, {r0-r15}
    asm_not_reached();

@ you'll have to write this, but it should mirror your other save
@ routines.    
@
@ void cswitchto_priv_asm(regs_t *old, regs_t *next);
@ 
@ voluntary context switch: save the current registers in <old>
@ and call the right switchto on <next>
MK_FN(cswitchto_priv_asm)
    @ initially just forward to ours.
    @ b staff_cswitchto_priv_asm

    @ to implement:
    @ 1. save all the callee regs (just like in your threads library)
    @    into the 17-entry register array we have been using.  note
    @    these have to be at the right offsets.
    @    e.g.:
    @       str r4,  [r0, #(4*4)]  @ r4
    @       str r5,  [r0, #(5*4)]  @ r5
    @       ... etc ...
    @   don't forget the cpsr!

    str r4, [r0, #(4*4)]  @ r4
    str r5, [r0, #(5*4)]  @ r5
    str r6, [r0, #(6*4)]  @ r6
    str r7, [r0, #(7*4)]  @ r7
    str r8, [r0, #(8*4)]  @ r8
    str r9, [r0, #(9*4)]  @ r9
    str r10, [r0, #(10*4)]  @ r10
    str r11, [r0, #(11*4)]  @ r11
    str r12, [r0, #(12*4)]  @ r12
    str r13, [r0, #(13*4)]  @ r13
    str r14, [r0, #(14*4)]  @ r14
    str lr,  [r0, #(15*4)]  @ r15
    
    mrs r2, cpsr
    str r2, [r0, #(16*4)]  @ cpsr

    mov r0, r1

    @ 2. just call the right switch routine: note, we should
    @    *not* reach this location when switching back!
    bl switchto_priv_asm
    asm_not_reached();

@ you'll have to write this, but it should mirror your other save
@ routines.    
@
@ void cswitchto_user_asm(regs_t *old, regs_t *next);
@ 
@ voluntary context switch: save the current registers in <old>
@ and call the right switchto on <next>
MK_FN(cswitchto_user_asm)
    @ initially just forward to ours.
    @ b staff_cswitchto_priv_asm
    str r4,  [r0, #(4*4)]    @ r4
    str r5,  [r0, #(5*4)]    @ r5
    str r6,  [r0, #(6*4)]    @ r6
    str r7,  [r0, #(7*4)]    @ r7
    str r8,  [r0, #(8*4)]    @ r8
    str r9,  [r0, #(9*4)]    @ r9
    str r10, [r0, #(10*4)]  @ r10
    str r11, [r0, #(11*4)]  @ r11
    str r12, [r0, #(12*4)]  @ r12
    str r13, [r0, #(13*4)]  @ r13
    str r14, [r0, #(14*4)]  @ r14
    str lr,  [r0, #(15*4)]  @ r15
    
    mrs r2, cpsr
    str r2, [r0, #(16*4)]  @ cpsr

    mov r0, r1

    @ asm_todo("save the registers: identical to <switchto_priv_asm>")
    bl switchto_user_asm
    asm_not_reached();
