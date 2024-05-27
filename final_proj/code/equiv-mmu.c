// hardware mmu code: mostly they do error checking and call into
// assembly (your-asm.S) which you will write next vm lab.
#include "rpi.h"
#include "rpi-constants.h"
#include "rpi-interrupts.h"
#include "equiv-mmu.h"
#include "libc/helper-macros.h"
#include "asm-helpers.h"

#define USE_STAFF 0

/*
 * Helpers
 */
#include "fast-hash32.h"

#if 0
#define print_field(x, field) do {                   \
        printk("\t0b%b\t= %s\n", (x)->field, #field);   \
        if(((x)->field) > 8)                            \
            printk("\t%x\n", (x)->field);             \
} while(0)
#endif

static int quiet_p = 0;
void mmu_be_quiet(void) { quiet_p = 1; }

void hash_print(const char *msg, const void *data, unsigned n) {
    if(quiet_p)
        return;
    printk("HASH:%s: hash=%x,nbytes=%d\n", msg, fast_hash32(data,n),n);
}

/*************************************************************************
 * domain helper
 */
void domain_acl_print(void) {
    if(quiet_p)
        return;
    printk("domain access control:\n");
    printk("\t%b\n", domain_access_ctrl_get());
}

/*************************************************************************
 * control reg1 helpers.
 */
static void control_reg1_check_offsets(void) {
    AssertNow(sizeof(struct control_reg1) == 4);
    check_bitfield(struct control_reg1, MMU_enabled,        0,      1);
    check_bitfield(struct control_reg1, A_alignment,        1,      1);
    check_bitfield(struct control_reg1, C_unified_enable,   2,      1);
    check_bitfield(struct control_reg1, W_write_buf,        3,      1);
    check_bitfield(struct control_reg1, B_endian,           7,      1);
    check_bitfield(struct control_reg1, S_prot,             8,      1);
    check_bitfield(struct control_reg1, R_rom_prot,         9,      1);
    check_bitfield(struct control_reg1, F,                  10,      1);
    check_bitfield(struct control_reg1, Z_branch_pred,      11,      1);
    check_bitfield(struct control_reg1, I_icache_enable,    12,      1);
    check_bitfield(struct control_reg1, V_high_except_v,    13,      1);
    check_bitfield(struct control_reg1, RR_cache_rep,       14,      1);
    check_bitfield(struct control_reg1, L4,                 15,      1);
    check_bitfield(struct control_reg1, F1,                 21,      1);
    check_bitfield(struct control_reg1, U_unaligned,        22,      1);
    check_bitfield(struct control_reg1, XP_pt,              23,      1);
    check_bitfield(struct control_reg1, VE_vect_int,        24,      1);
    check_bitfield(struct control_reg1, EE,                 25,      1);
    check_bitfield(struct control_reg1, L2_enabled,         26,      1);
    check_bitfield(struct control_reg1, TR_tex_remap,       28,      1);
    check_bitfield(struct control_reg1, FA_force_ap,        29,      1);
}

static void control_reg1_sanity_check(struct control_reg1 *r) {
    // SBO: should be 1
    assert(r->_unused1 == 0b111);
    assert(r->_dt == 1);
    assert(r->_it == 1);

    // SBZ: should be 0
    assert(r->_sbz0 == 0);
    assert(r->_sbz1 == 0);
    assert(r->_st == 0);

    // unknown
    // assert(r->_reserved == 0);

    // allow non-back compat.  
    // assert(r->XP_pt == 1);

    // we don't enable cache yet.
    assert(!r->L2_enabled);
    assert(!r->I_icache_enable);
    assert(!r->C_unified_enable);
}

void control_reg1_print(struct control_reg1 *r) {
    control_reg1_sanity_check(r);

    printk("system control reg=\n");
    print_field(r, FA_force_ap);
    print_field(r, TR_tex_remap);
    print_field(r, L2_enabled);
    print_field(r, EE);
    print_field(r, VE_vect_int);
    print_field(r, XP_pt);
    print_field(r, U_unaligned);
    print_field(r, F1);
    print_field(r, RR_cache_rep);
    print_field(r, I_icache_enable);
    print_field(r, Z_branch_pred);
    print_field(r, R_rom_prot);
    print_field(r, S_prot);
    print_field(r, B_endian);
    print_field(r, W_write_buf);
    print_field(r, C_unified_enable);
    print_field(r, A_alignment);
    print_field(r, MMU_enabled);
}


/*************************************************************************
 * tlb config helpers: b4-39
 */
static void tlb_config_check_offsets(void) {
    AssertNow(sizeof(struct tlb_config) == 4);
    check_bitfield(struct tlb_config, unified_p,    0,      1);
    check_bitfield(struct tlb_config, n_d_lock,     8,      8);
    check_bitfield(struct tlb_config, n_i_lock,    16,      8);
}
void tlb_config_print(struct tlb_config *c) {
    printk("TLB config:\n");
    printk("\tunified=%d\n", c->unified_p);
    printk("\tlockable data ent=%d\n", c->n_d_lock);
    printk("\tlockable inst ent=%d\n", c->n_i_lock);
}

/*************************************************************************
 * first level pt descriptor
 */
static void fld_check_valid(fld_t *f) {
    assert(f->_sbz1 == 0);
    assert(f->tag == 0b10);
    assert(f->S == 0);
    assert(f->IMP == 0);
#if 0
    assert(f->TEX == 0);
    assert(f->C == 0);
    assert(f->B == 0);
#endif
    assert(f->super == 0);
}


fld_t fld_mk(void) {
    // all unused fields can have 0 as default.
    return (fld_t){ .tag = 0b10 };
}

static void fld_check_offsets(void) {
    fld_t f = fld_mk();
    fld_check_valid(&f);

    assert(sizeof f == 4);

    //                    field     offset  nbits
    check_bitfield(fld_t, tag,      0,      2);
    check_bitfield(fld_t, B,        2,      1);
    check_bitfield(fld_t, C,        3,      1);
    check_bitfield(fld_t, XN,       4,      1);
    check_bitfield(fld_t, domain,   5,      4);
    check_bitfield(fld_t, IMP,      9,      1);
    check_bitfield(fld_t, AP,       10,     2);
    check_bitfield(fld_t, TEX,      12,     3);
    check_bitfield(fld_t, APX,      15,     1);
    check_bitfield(fld_t, S,        16,     1);
    check_bitfield(fld_t, nG,       17,     1);
    check_bitfield(fld_t, super,       18,     1);
    check_bitfield(fld_t, sec_base_addr, 20,     12);
}

void vm_pte_print(vm_pt_t *pt, vm_pte_t *pte) {
    if(quiet_p)
        return;
    printk("------------------------------\n");
    printk("printing pte entry [index=%d = addr=%p]:\n", pte-pt, pte);
    hash_print("\tPTE crc:", pte, sizeof *pte);
    print_field(pte, sec_base_addr);
    printk("\t  --> va=%x\n", pte->sec_base_addr<<20);
    printk("\t          76543210 [digit position]\n");

    print_field(pte, nG);
    print_field(pte, S);
    print_field(pte, APX);
    print_field(pte, TEX);
    print_field(pte, AP);
    print_field(pte, IMP);
    print_field(pte, domain);
    print_field(pte, XN);
    print_field(pte, C);
    print_field(pte, B);
    print_field(pte, tag);

    fld_check_valid(pte);
}

/************************************************************************
 * the checking harness, such as it is.
 */

void check_vm_structs(void) {
    control_reg1_check_offsets();
    tlb_config_check_offsets();
    fld_check_offsets();
}

// given.

int mmu_is_enabled(void) {
    return cp15_ctrl_reg1_rd().MMU_enabled != 0;
}

// disable the mmu by setting control register 1
// to <c:32>.
// 
// we use a C veneer over the assembly (mmu_disable_set_asm)
// so we can easily do assertions: the real work is 
// done by the asm code (you'll write this next time).
void mmu_disable_set(cp15_ctrl_reg1_t c) {
    assert(!c.MMU_enabled);

#if USE_STAFF
    staff_mmu_disable_set_asm(c);
    return;
#endif
    
    // record if dcache on.
    uint32_t cache_on_p = c.C_unified_enable;

    mmu_disable_set_asm(c);
    

    // re-enable if it was on.
    if(cache_on_p) {
        c.C_unified_enable = 1;
        cp15_ctrl_reg1_wr(c);
    }
}

// disable the MMU by flipping the enable bit.   we 
// use a C vener so we can do assertions and then call
// out to assembly to do the real work (you'll write this
// next time).
void mmu_disable(void) {
    cp15_ctrl_reg1_t c = cp15_ctrl_reg1_rd();
    assert(c.MMU_enabled);
    c.MMU_enabled=0;
    mmu_disable_set(c);
}

// enable the mmu by setting control reg 1 to
// <c>.   we start in C so we can do assertions
// and then call out to the assembly for the 
// real work (you'll write this code next time).
void mmu_enable_set(cp15_ctrl_reg1_t c) {
    assert(c.MMU_enabled);
#if USE_STAFF
    staff_mmu_enable_set_asm(c);
#else
    mmu_enable_set_asm(c);
#endif
}

// enable mmu by flipping enable bit.
void mmu_enable(void) {
    cp15_ctrl_reg1_t c = cp15_ctrl_reg1_rd();
    assert(!c.MMU_enabled);
    c.MMU_enabled = 1;
    mmu_enable_set(c);
}

// C end of this: does sanity checking then calls asm.
void set_procid_ttbr0(unsigned pid, unsigned asid, fld_t *pt) {
    assert((pid >> 24) == 0);
    assert(pid > 64);
    assert(asid < 64 && asid);
    cp15_set_procid_ttbr0(pid << 8 | asid, pt);
}


// set so that we use armv6 memory.
// this should be wrapped up neater.  broken down so can replace 
// one by one.
//  1. the fields are defined in vm.h.
//  2. specify armv6 (no subpages).
//  3. check that the coprocessor write succeeded.
void mmu_init(void) { 
    //mmu_reset();
#if USE_STAFF
    staff_mmu_init();
    return;
#endif

    // reset the MMU state: you will implement next lab
    //staff_mmu_reset();
    mmu_reset();

    // trivial: RMW the xp bit in control reg 1.
    // leave mmu disabled.
    struct control_reg1 c1 = cp15_ctrl_reg1_rd();
    c1.XP_pt = 1;
    cp15_ctrl_reg1_wr(c1);

    // make sure write succeeded.
    c1 = cp15_ctrl_reg1_rd();
    assert(c1.XP_pt);
    assert(!c1.MMU_enabled);
}

// read and return the domain access control register

// b4-42
// set domain access control register to <r>
void domain_access_ctrl_set(uint32_t r) {
    
#if USE_STAFF
    staff_domain_access_ctrl_set(r);
#else
    cp15_domain_ctrl_wr(r);
#endif
    //assert(domain_access_ctrl_get() == r);
}

/*void staff_domain_access_ctrl_set(uint32_t d) {
  domain_access_ctrl_set(d);
}

void staff_set_procid_ttbr0(unsigned pid, unsigned asid, void *pt) {
  set_procid_ttbr0(pid, asid, (fld_t*)pt);
}*/
