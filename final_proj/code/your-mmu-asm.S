@ Must carefully read B2 of the armv6 manual.  You cannot rely on "well it
@ worked on my test case": mistakes cause stale data, which may or may not
@ show up in your test case (likely not) despite being wildly broken.
@
@ Note: Rd is allegedly a read-only parameter to these instructions, but
@ the manual says SBZ (should be zero).  I think (and the linux source seems
@ to think) that this means we have to explicitly set it to 0.  Not setting
@ Rd=0 is an easy mistake to make.  Perhaps is worth building in (Linux does
@ not, but) --- the cost of an extra mov is negligible given our other 
@ overheads.
@
@ Alot of people put these instructions in inline assembly, wrapped up by 
@ function calls (e.g., cs107e's MMU code).  This practice is dangerous. 
@ For example, if you are flushing caches, you cannot be sure that the 
@ function return, etc does not then reload them, esp w.r.t. the BTB.  I 
@ think the only safe approach is to put directly in assembly so you are 
@ guaranteed no branch, indirect jump, load/store happens in between.
@
@ A lot of MMU code out there appears pretty broken b/c they don't do enough
@ flushing operations and/or rely on gcc code generation to not do the above.

#include "rpi-asm.h"
#include "armv6-coprocessor-asm.h"


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ many page numbers are in 
@    armv6-coprocessor-asm.h
@ and the other armv6-*.h files

MK_FN(cp15_ctrl_reg1_rd)
    mrc p15, 0, r0, c1, c0, 0
    bx lr

MK_FN(cp15_ctrl_reg1_wr)
    mcr p15, 0, r0, c1, c0, 0
    bx lr

MK_FN(domain_access_ctrl_get)
    mrc p15, 0, r0, c3, c0, 0
    bx lr

// just read the cache type: use C code to figure out if it is unified
// b6-13, b6-14
MK_FN(get_cache_type)
    asm_not_implemented()
    bx lr

// void reset(void)
// clear the dcache, icache, tlbs
MK_FN(mmu_reset)
    @b staff_mmu_reset

    // 
    // invalidate icache & dcache & btb -> dsb -> prefetch flush
 
    @ Invalidate TLBs. We do this before the DSB to ensure that it commits, and
    @ do it before the prefetch flush to to ensure that it appears for all
    @ further instructions. (B2-22)
    mov r0, #0
    INV_TLB(r0)
    @ Wipe all caches. Do this befoe the DSB to ensure that it commits before
    @ we return from the function. (B2-22) Do it before the prefetch flush to
    @ ensure that the instruction cache invalidation appears for future
    @ instruction fetches.
    mov r0, #0
    INV_ALL_CACHES(r0)

    @ Perform a DSB to ensure that the previous cache invalidations have
    @ completed. Do this before the prefetch flush to ensure that the
    @ invalidations complete before flushing (and thus refilling) the prefetch
    @ buffer. (B2-22)
    mov r0, #0
    DSB(r0)

    @ Flush the prefetch buffer. Do this at the end (after the DSB) to ensure
    @ that all future instructions are affected by the cache invalidations.
    mov r0, #0
    PREFETCH_FLUSH(r0)

    bx lr

// void cp15_domain_ctrl_wr(uint32_t dom_reg);
//  need to do cp15 sync: dsb, prefetch flush
MK_FN(cp15_domain_ctrl_wr)
    //b staff_cp15_domain_ctrl_wr

    // write, dsb, btb, dsb, prefetch 

    @ Begin a write to CP15
    DOMAIN_CTRL_WR(r0)

    @ DSB ensures that the previous cp15 write is guaranteed to have comitted.
    @ (B2-19) We do this immediately after the cp15 write and before the
    @ prefetch flush so that the flush occurs on current registers (in case we
    @ modify our current domain) and that the CP15 write doesn't commit after
    @ the prefetch flush.
    @mov r0, #0
    @DSB(r0) Don't need

    @ Prefetch flush is necessary to ensure that the context altering CP15 write
    @ appears to all later instructions. (B2-19) We execute this after DSB
    @ because the DSB guarantees that the CP15 write has taken effect. (B2-19)
    mov r0, #0
    PREFETCH_FLUSH(r0)
    bx lr

// void cp15_set_procid_ttbr0(uint32_t proc_and_asid, fld_t *pt);
// sequence from b2-25
MK_FN(cp15_set_procid_ttbr0)
    @b staff_cp15_set_procid_ttbr0
 
    @ Impose a DSB to ensure that the prior ASID switch has comitted before
    @ moving on. (B2-24)
    mov r3, #0
    DSB(r3)

    @ Set the ASID to a reserved value. We do this first and foremost to ensure
    @ that non-global page accesses for prefetch aren't made on an uncertain page
    @ table. (B2-25)
    mov r2, #0
    ASID_SET(r2)

    @ Flush the prefetch buffer to ensure that the ASID switch now applies to the
    @ execution stream. (B2-25)
    mov r3, #0
    PREFETCH_FLUSH(r3)

    @ Set the first page table register to the second argument. (B4-41)
    TTBR0_SET(r1)

    @ Set the second page table register to 0. We do this because we've
    @ configured the MMU to use the entirety of the first page table register and
    @ none of the second. (B4-41)
    TTBR1_SET(r2)

    @ Configure the MMU to use only the first page table. (B4-41)
    TTBR_BASE_CTRL_WR(r2)
    
    @ Perform a prefetch flush after modifying the translation table base as
    @ per. (B2-24)
    mov r3, #0
    PREFETCH_FLUSH(r3)

    @ Flush the BTB, necessary as per B2-24 to prevent the branch predictor from
    @ predicting on stale data.
    mov r3, #0
    FLUSH_BTB(r3)

    @ Perform a prefetch flush to ensure that a) the prior BTB flush has
    @ completed (B2-24) and b) new instruction fetches that could be affected by
    @ updated PTEs are affected by updated PTEs (B2-23)
    mov r3, #0
    PREFETCH_FLUSH(r3)

    @ Set the ASID to the target ASID (B2-25)
    ASID_SET(r0)

    @ Flush the prefetch buffer because we've changed the execution context
    @ with the ASID write (B2-24)
    mov r3, #0
    PREFETCH_FLUSH(r3)

    bx lr

// void mmu_disable_set_asm(cp15_ctrl_reg1_t c);
MK_FN(mmu_disable_set_asm)
    @b staff_mmu_disable_set_asm

    @ note: this piece of code is mapped into the sample place in both
    @ MMU-enabled and MMU-disabled so BTB and Prefetch entries work identically
    @ to both MMU-enable and disable.   Without this requirement I believe is
    @ very hard to get right (perhaps impossible?) since we cannot flush
    @ the BTB and prefetch and switch MMU on/off atomically.  thus, would
    @ flush, but instructions could get fetched before we could disable,
    @ defeating the purpose.

    @ Clean the data cache to preserve all cached data. We need to do this
    @ first because the data cache is only accessible when the MMU is enabled.
    @ (6-9)
    mov r1, #0
    CLEAN_INV_DCACHE(r1)

    @ Impose a DSB to ensure tha the data cache maintence operation occurs
    @ before we begin invalidating caches and disabling the MMU. This necessary
    @ as per B2-22.
    mov r1, #0
    DSB(r1)

    @ Disable the MMU (6-9)
    CONTROL_REG1_WR(r0)

    @ Ensure the CP15 write have
    @ completed (B2-21, B2-24 )
    mov r1, #0
    PREFETCH_FLUSH(r1)

    @ Invalidate the instruction cache. The instruction cache could hold now
    @ invalid addresses since we're disabling the MMU.
    mov r1, #0
    INV_ICACHE(r1)
 
    @ Impose a DSB to ensure that the instruction cache invalidation has
    @ comitted. (B2-21)
    mov r1, #0
    DSB(r1)

    @ Flush the branch predictor. This is necessary to ensure that the branch
    @ predictor doesn't act on stale data, as per B2-24. We do this after the
    @ disable and DSB to ensure that the MMU is disabled before flushing the
    @ BTB, also per B2-24.
    mov r1, #0
    FLUSH_BTB(r1)

    @ Flush the prefetch buffer to ensure that the previous BTB flush and CP15
    @ write have completed. (B2-24) 
    mov r1, #0
    PREFETCH_FLUSH(r1)

    bx lr

// void mmu_enable_set_asm(cp15_ctrl_reg1_t c);
MK_FN(mmu_enable_set_asm)
    @ b staff_mmu_enable_set_asm

    @ Disable the icache
    CONTROL_REG1_RD(r1)
    bic r1, r1, #4096 @ 4096 = 1 << 12
    CONTROL_REG1_WR(r1)

    @ Invalidate the instruction cache. The instruction cache could hold now
    @ invalid addresses since we're disabling the MMU.
    mov r1, #0
    INV_ICACHE(r1)

    @ Impose a DSB to ensure the prior contorl register write has fully
    @ completed(B2-19)
    mov r1, #0
    DSB(r1)

    @ Enable the MMU. We do this first because the BTB must be flushed after
    @ the MMU is enabled as per page B2-24 "enabling or disabling the MMU"
    CONTROL_REG1_WR(r0)

    @ Flush the prefetch buffer to ensure that the control register write is
    @ guaranteed to have taken effect. (B2-24)
    mov r1, #0
    PREFETCH_FLUSH(r1)

    @ Flush the BTB. We must flush the BTB after any MMU maintenence operation
    @ to ensure that branch prediction doesn't occur on stale addresses. (B2-24)
    mov r1, #0
    FLUSH_BTB(r1)

    @ Flush the prefetch buffer to ensure that both the CP15 write and the BTB
    @ flush are guaranteed to have taken effect. (B2-24)
    mov r1, #0
    PREFETCH_FLUSH(r1)

    bx lr

MK_FN(mmu_sync_pte_mods)
    @ Clean and invalidate the data cache. We need this to ensure that main
    @ memory holds the updated PTEs for the MMU page walk hardware to observe it.
    mov r0, #0
    CLEAN_INV_DCACHE(r0)

    @ Invalidate the icache. We need to do this to ensure that the instruction
    @ cache doesn't hold stale instruction addresses.
    mov r0, #0
    INV_ICACHE(r0)

    @ Impose a DSB to ensure that the previous data cache flush is visible to
    @ all, not just processor, observers (i.e. MMU page walk hardware) (B2-23)
    mov r0, #0
    DSB(r0)

    @ Invalidate the TLB because we changed PTEs. 
    mov r0, #0
    INV_TLB(r0)

    @ Flush the BTB, necessary as per B2-24 to prevent the branch predictor from
    @ predicting on stale data.
    mov r0, #0
    FLUSH_BTB(r0)

    @ Impose a DSB to ensure that the TLB invalidation completes. We need to do
    @ this so that the prefetch flush has an effect. (B2-23)
    mov r0, #0
    DSB(r0)

    @ Perform a prefetch flush to ensure that a) the prior BTB flush has
    @ completed (B2-24) and b) new instruction fetches that could be affected by
    @ updated PTEs are affected by updated PTEs (B2-23)
    mov r0, #0
    PREFETCH_FLUSH(r0)

    bx lr

