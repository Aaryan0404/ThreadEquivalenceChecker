#ifndef EQUIV_MMU_H__
#define EQUIV_MMU_H__

/*
 * Equivalence checker MMU definitions.
 */

/*
 * ARMv6 coprocessor definitions
 */

/******************************************************************************
 * b4-39 / 3-60: tlb read configuration 
 */
typedef struct tlb_config {
    unsigned
        unified_p:1,    // 0:1 0 = unified, 1 = seperate I/D
        _sbz0:7,        // 1-7:7
        n_d_lock:8,       // 8-15:8  number of unified/data lockable entries
        n_i_lock:8,        //16-23:8 number of instruction lockable entries
        _sbz1:8;
} cp15_tlb_config_t;

_Static_assert(sizeof(cp15_tlb_config_t) == 4, "invalid size for struct tlb_config!");

cp15_tlb_config_t cp15_read_tlb_config(void);

// vm-helpers: print <c>
void tlb_config_print(struct tlb_config *c);

/******************************************************************************
 * b3-12: the massive machine configuration.
 * A more full description on 3-45 in arm1176.pdf  
 * 
 * things to use for speed:
 *  - C_cache = l1 data cache
 *  - W_write_buf
 *  - Z_branch_pred
 *  - I_icache_enable
 *  - L2_enabled
 *  - C_unified_enable [note: we don't have unified I/D]
 *
 * the c1 aux register enabled branch prediction and return
 * stack prediction 3-49
 * 
 * SBO = should be 1.
 */
typedef struct control_reg1 {
    unsigned
        MMU_enabled:1,      // 0:1,   0 = MMU disabled, 1 = enabled,
        A_alignment:1,      // 1:1    0 = alignment check disabled.
        C_unified_enable:1, // 2:1    if unified used, this is enable/disable
                            // on arm1176: level 1 data cache
                            //        otherwise is enable/disable for dcache
        W_write_buf:1,      // 3:1    0 = write buffer disabled
        _unused1:3,         // 4:3
        B_endian:1,         // 7:1    0 = little 
        S_prot:1,           // 8:1   - deprecated b4-8
        R_rom_prot:1,       // 9:1   - deprecated b4-8
        F:1,                // 10:1  impl defined
        Z_branch_pred:1,    // 11:1  branch predict 0 = disabled, 1 = enabled
        I_icache_enable:1,  // 12:1  if seperate i/d, disables(0) or enables(1)
        V_high_except_v:1,  // 13:1  location of exception vec.  0 = normal
        RR_cache_rep:1,     // 14:1 cache replacement 0 = normal, 1 = different.
        L4:1,               // 15:1 inhibits arm internworking.
        _dt:1,              // 16:1, SBO
        _sbz0:1,            // 17:1 sbz
        _it:1,              // 18
        _sbz1:1,            // 19
        _st:1,              // 20
        F1:1,               // 21   fast interrupt.  [dunno]
        U_unaligned:1,      // 22 : unaligned
        XP_pt:1,            // 23:1,  1 = vmsav6, subpages disabled.
                            //    must be enabled for XN.
        VE_vect_int:1,      // 24: no.  0
        EE:1,               // 25 endient exception a2-34 
        L2_enabled:1,       // 26,
        _reserved0:1,       // 27
        TR_tex_remap:1,     // 28
        FA_force_ap:1,      // 29  0 = disabled, 1 = force ap enabled      
        _reserved1:2;
} cp15_ctrl_reg1_t;

cp15_ctrl_reg1_t cp15_ctrl_reg1_rd(void);
uint32_t cp15_ctrl_reg1_get(void);
void cp15_ctrl_reg1_wr(cp15_ctrl_reg1_t r);

/******************************************************************************
 * set page table pointers.
 *
 * C, S, IMP, should be 0 and the offset can dynamically change based on N,
 * so we just do this manually.
 */
typedef struct {
#if 0
    unsigned 
        c:1,    // 0:1  inner cacheabled = 1, non=0
        s:1,    // 1:1, pt walk is shareable 1 or non-sharable (0)
        IMP:1,  // 2:1,
        RGN:2,  // 3:2 // B4-42: is it cacheable: 
                       // 0b00: outer non-cacheaable.  
                       // 0b10 outer write through
                       // 0b11 outer write back
        // this can change depending on what N is.  not sure the cleanest.
        base:
#endif
    unsigned base;
} cp15_tlb_reg_t;

cp15_tlb_reg_t cp15_ttbr0_rd(void);
void cp15_ttbr0_wr(cp15_tlb_reg_t r);

cp15_tlb_reg_t cp15_ttbr1_rd(void);
void cp15_ttbr1_wr(cp15_tlb_reg_t r);

// read the N that divides the address range.  0 = just use ttbr0
uint32_t cp15_ttbr_ctrl_rd(void);
// set the N that divides the address range b4-41
void cp15_ttbr_ctrl_wr(uint32_t N);

// must set both at once, AFAIK or hardware state too iffy.
struct first_level_descriptor;
// void cp15_set_procid_ttbr0(uint32_t procid, struct first_level_descriptor *pt);

uint32_t cp15_procid_rd(void);

void cp15_tlbr_print(void);
void cp15_domain_print(void);


/**************************************************************************
 * engler, cs140e: structures for armv6 virtual memory.
 *
 * ./docs/README.md  gives pages numbers for (some) key things if you can't 
 * find them.
 *
 * page table operations: we only handle mapping 1MB sections.
 */

// need to add citations to the various things, especially for the 
// different routines.

/*
    -----------------------------------------------------------------
    b4-26 first level descriptor  
        bits0:1 =
            - 0b00: unmapped, 
            - 0b10: section descriptor, 
            - 0b01: second level coarse pt.
            - 0b11: reserved

    b4-27 section:
        31-20: section base address: must be aligned.
        19: sbz: "should be zero"
        18: 0
        17: nG:  b4-25, global bit=0 implies global mapping, g=1, process 
                 specific.
        -16: S: 0 deprecated.
        -15: APX  0b11 r/w [see below]
        -14-12: TEX: table on b4-12
        -11-10: AP 
        9: IMP: 0: should be set to 0 unless the implementation defined
                   functionality is required.

        -8-5: Domain: b4-10: 0b11 = manager (no perm check), 0b01 (checked perm)
        -4: XN: 1 = execute never,  0 = can execute.
        3:C: set 0 see below
        2:B: set 0
        1: 1
        0: 0

  APX, S, R: 
    b4-8: permissions, table on b4-9
    use of S/R deprecated.

    S  R  APX   AP[1:0]   privileged permissions   user permissions
    x  x    0   0b11       r/w                       r/w
    x  x    0   0b10       r/w                       r/o
    x  x    0   0b01       r/w                       no access
    0  0    0      0       no access                no access
    
  ap,apx,domain:
     b4-8

  c,b,tex:
   b4-11, b4-12 

    TEX   C  B 
    0b000 0  0 strongly ordered.   
    0b001 0  0 non-cacheable

  
   on pi: organized from bit 0 to high.
*/

typedef struct first_level_descriptor {
    unsigned
        tag:2,      // 0-1:2    should be 0b10
        B:1,        // 2:1      just like pinned.
        C:1,        // 3:1      just like pinned
        XN:1,       // 4:1      1 = execute never, 0 = can execute
                    // needs to have XP=1 in ctrl-1.

        domain:4,   // 5-8:4    b4-10: 0b11 = manager, 0b01 checked perms
        IMP:1,      // 9:1      should be set to 0 unless imp-defined 
                    //          functionality is needed.

        AP:2,       // 10-11:2  permissions, see b4-8/9
        TEX:3,      // 12-14:3
        APX:1,      // 15:1     
        S:1,        // 16:1     set=0, deprecated.
        nG:1,       // 17:1     nG=0 ==> global mapping, =1 ==> process specific
        super:1,    // 18:1     selects between section (0) and supersection (1)
        _sbz1:1,    // 19:1     sbz
        sec_base_addr:12; // 20-31.  must be aligned.
} fld_t;
_Static_assert(sizeof(fld_t) == 4, "invalid size for fld_t!");

// B4-9: AP field:  no/access=0b00, r/o=0b10, rw=0b11
enum {
    // read-write access
    AP_rw = 0b11,
    // no access either privileged or user
    AP_no_access = 0b00,
    // privileged: read only.
    AP_rd_only = 0b10
};

// arm-vm-helpers: print <f>
// void fld_print(fld_t *f);


/***********************************************************************
 * utility routines [in arm-vm-helpers.c]
 */

// hash [data, data+n) and print it with <msg>
void hash_print(const char *msg, const void *data, unsigned n);

// vm-helpers.c: one-time sanity check of the offsets in the structures above.
void check_vm_structs(void);

// domain access control  b4-42
void domain_acl_print(void);


/*
 * Process (proc) map
 */

// physical address of our kernel: used to map.
// probably should tag with the domain?
// i think just use the page table entry itself.  give
// some simple routines to make cached, uncached, device
// memory, readonly, etc
typedef struct {
    uint32_t addr, nbytes;
    // need to have privileged.
    enum { MEM_DEVICE, MEM_RW, MEM_RO } type;
    unsigned dom;
} pr_ent_t;
static inline pr_ent_t
pr_ent_mk(uint32_t addr, uint32_t nbytes, int type, unsigned dom) {
    demand(nbytes == 1024*1024, only doing sections first);
    demand(dom < 16, "illegal domain %d\n", dom);
    return (pr_ent_t) {
        .addr = addr,
        .nbytes = nbytes,
        .type = type,
        .dom = dom
    };
}

typedef struct {
#   define MAX_ENT 8
    unsigned n;
    unsigned dom_ids;       // all the domain ids in use.
    pr_ent_t map[MAX_ENT];
} procmap_t;
// add entry <e> to procmap <p>
static inline void procmap_push(procmap_t *p, pr_ent_t e) {
    assert(p->n < MAX_ENT);
    assert(e.dom < 16);
    p->dom_ids |= 1 << e.dom;
    p->map[p->n++] = e;
}

// given a bitvector of which domains are in use,
// compute the domain permission bits to use
static inline uint32_t 
dom_perm(uint32_t doms, unsigned perm) {
    assert(doms);
    assert(doms >> 16 == 0);
    assert(perm <= 0b11);
    unsigned mask = 0;
    for(unsigned i = 0; i < 16; i++) {
        if(doms & (1 <<  i))
            mask |= perm << i*2;
    }
    return mask;
}

static inline pr_ent_t *
procmap_lookup(procmap_t *pmap, const void *addr) {
    for(int i = 0; i < pmap->n; i++) {
        pr_ent_t *p = &pmap->map[i];

        void *s = (void*)p->addr;
        void *e = s + p->nbytes;
        if(addr >= s && addr < e)
            return p;
    }
    return 0;
}

// memory map of address space.  each chunk of memory should have an entry.
// must be aligned to the size of the memory region.
//
// we don't seem able to search in the TLB if MMU is off.  
// 
// limitations in overall scheme:
//   - currently just tag everything with the same domain.
//   - should actually track what sections are allocated.
//   - basic map of our address space: should add sanity checking that ranges
//     are not overlapping.   
//   - our overall vm scheme is janky since still has assumptions encoded.
//   - in real life would want more control over region attributes.
//
// mapping static sections.
#include "memmap.h"
static inline procmap_t procmap_default_mk(unsigned dom, unsigned dom2) {
    enum { MB = 1024 * 1024 };

    procmap_t p = {};
    procmap_push(&p, pr_ent_mk(0x20000000, MB, MEM_DEVICE, dom));
    procmap_push(&p, pr_ent_mk(0x20100000, MB, MEM_DEVICE, dom));
    procmap_push(&p, pr_ent_mk(0x20200000, MB, MEM_DEVICE, dom));
    procmap_push(&p, pr_ent_mk(0x300000,   MB, MEM_RW,     dom2));

    // make sure entire program fits on 1 page: when done, will split
    // this onto two pages and mark the code as RO.
#if 0
    extern char __prog_end__[];
#endif
    assert((uint32_t)__prog_end__ <= MB);
    procmap_push(&p, pr_ent_mk(0x00000000, MB, MEM_RW, dom));

    // heap
    char *start = kmalloc_heap_start();
    char *end = kmalloc_heap_end();
    unsigned nbytes = end - start;

    // for today: check that heap starts at 1MB.
    if(start != (void*)MB)
        panic("sanity check that heap starts at 1mb failed: %p\n", start);
    assert(start == (void*)MB);
    // and that its 1MB big.
    if(nbytes != MB)
        panic("nbytes = %d\n", nbytes);

    procmap_push(&p, pr_ent_mk(0x00100000, MB, MEM_RW, dom));

    // the two hardcoded stacks we use.
    procmap_push(&p, pr_ent_mk(INT_STACK_ADDR-MB, MB, MEM_RW, dom));
    procmap_push(&p, pr_ent_mk(STACK_ADDR-MB, MB, MEM_RW, dom));

    return p;
}

/*
 * Core MMU
 */

// get the different cp15 data structures.
// #include "armv6-cp15.h"
// get the different cp15 data structures.
// #include "armv6-vm.h"

/* one time initialation of caches, tlb, etc */
void mmu_init(void);
void staff_mmu_init(void);

// install trap handlers: should make more fine-grained.
void mmu_install_handlers(void);

// allocate page table and initialize.  handles alignment.
//
// Naive: we assume a single page table, which forces it to be 4096 entries.
// Why 4096:
//      - ARM = 32-bit addresses.
//      - 2^32 = 4GB
//      - we use 1MB sections (i.e., "page size" = 1MB).
//      - there are 4096 1MB sections in 4GB (4GB = 1MB * 4096).
//      - therefore page table must have 4096 entries.
//
// Note: If you want 4k pages, need to use the ARM 2-level page table format.
// These also map 1MB (otherwise hard to mix 1MB sections and 4k pages).
fld_t *mmu_pt_alloc(unsigned n_entries);

// initialize a preallocated chunk of memory
fld_t *mmu_pt_init(void *addr, unsigned nbytes);


// map a 1mb section starting at <va> to <pa>
fld_t *mmu_map_section(fld_t *pt, uint32_t va, uint32_t pa, uint32_t dom);
fld_t *staff_mmu_map_section(fld_t *pt, uint32_t va, uint32_t pa, uint32_t dom);
// map <nsec> 1mb sections starting at <va> to <pa>
void mmu_map_sections(fld_t *pt, unsigned va, unsigned pa, unsigned nsec, uint32_t dom);
void staff_mmu_map_sections(fld_t *pt, unsigned va, unsigned pa, unsigned nsec, uint32_t dom);


// lookup section <va> in page table <pt>
fld_t *mmu_lookup(fld_t *pt, uint32_t va);

// called to sync after a set of pte modifications: flushes everything.
void mmu_sync_pte_mods(void);
void staff_mmu_sync_pte_mods(void);

// *<pte> = e.   more precisely flushes state.
void mmu_sync_pte_mod(fld_t *pte, fld_t e);


/*
 * enable, disable init.
 */

// turn on/off mmu: handles all the cache flushing, barriers, etc.
void mmu_enable(void);
void mmu_disable(void);

// same as disable/enable except client gives the control reg to use --- 
// this allows messing with cache state, etc.
void mmu_disable_set(cp15_ctrl_reg1_t c);
void mmu_enable_set(cp15_ctrl_reg1_t c);

// <pid>= process id.
// <asid>= address space identifier, can't be 0, must be < 64
// pt is the page table address
void cp15_set_procid_ttbr0(unsigned pid, fld_t* pt);
void set_procid_ttbr0(unsigned pid, unsigned asid, fld_t *pt);

// set the 16 2-bit access control fields and do any needed coherence.
void domain_access_ctrl_set(uint32_t d);
void staff_domain_access_ctrl_set(uint32_t d);

// return the domain access control register.
uint32_t domain_access_ctrl_get(void);
uint32_t staff_domain_access_ctrl_get(void);

// set protection for [va,va+nsection) to <perm>
void mmu_mprotect(fld_t *pt, unsigned va, unsigned nsec, unsigned perm);
void staff_mmu_mprotect(fld_t *pt, unsigned va, unsigned nsec, unsigned perm);

// helpers
// mark [va, va+ nsec * 1MB) with the indicated permissions.
static inline void 
mmu_mark_sec_no_access(fld_t *pt, unsigned va, unsigned nsec) 
    { mmu_mprotect(pt,va,nsec, AP_no_access); }
static inline void 
mmu_mark_sec_readonly(fld_t *pt, unsigned va, unsigned nsec) 
    { mmu_mprotect(pt,va,nsec, AP_rd_only); }
static inline void 
mmu_mark_sec_rw(fld_t *pt, unsigned va, unsigned nsec) 
    { mmu_mprotect(pt,va,nsec, AP_rw); }

/*****************************************************************
 * mmu assembly routines: these are mainly what you will implement in
 * lab 15 (today).
 *
 *
 * sort-of organized from easiest to hardest.
 */

// write the values for the domain control reg and do any needed coherence.
void cp15_domain_ctrl_wr(uint32_t dom_reg);
void staff_cp15_domain_ctrl_wr(uint32_t dom_reg);


// one time initialization of the machine state.  cache/tlb should not be active yet
// so just invalidate.  
void mmu_reset(void);
void staff_mmu_reset(void);

// sets both the procid and the ttbr0 register: does any needed coherence.
// i *think* we absolutely must do both of these at once.
//void cp15_set_procid_ttbr0(uint32_t proc_and_asid, fld_t *pt);
void staff_cp15_set_procid_ttbr0(uint32_t proc_and_asid, fld_t *pt);

// sets the control register to <c> and does any needed coherence operations.
void mmu_disable_set_asm(cp15_ctrl_reg1_t c);
void mmu_enable_set_asm(cp15_ctrl_reg1_t c);

void staff_mmu_disable_set_asm(cp15_ctrl_reg1_t c);
void staff_mmu_enable_set_asm(cp15_ctrl_reg1_t c);

/******************************************************************
 * simple helpers.
 */

// is MMU on?
int mmu_is_enabled(void);
int staff_mmu_is_enabled(void);

// b4-20
enum {
    SECTION_XLATE_FAULT = 0b00101,
    SECTION_PERM_FAULT = 0b1101,
};

// strip off the offset bits, leaving the section bits only.
static inline unsigned mmu_sec_bits_only(unsigned u) { 
    return u & ~((1<<21)-1); 
}
// extract the (virt/phys) section number.
static inline unsigned mmu_sec_num(unsigned u) { 
    return u >> 20; 
}

// is <x> divisible by 1<<n?
static inline uint32_t mod_pow2(uint32_t x, uint32_t n) {
    assert(n<32);
    uint32_t rem = x % (1<<n);

    if(rem)
        panic("cannot divide %x by 2^%d : remainder=%x\n", x, n, rem);
    return 1;
}
// is <x> divisible by 1<<n?
static inline uint32_t mod_pow2_ptr(void *x, uint32_t n) {
    return mod_pow2((uint32_t)x,n);
}

// turn off all the output from mmu helpers
void mmu_be_quiet(void);

typedef void (*mmu_handler_t)(uint32_t fault_addr, uint32_t reason);
// set handler for data_abort: return previous.
mmu_handler_t mmu_set_data_abort(mmu_handler_t h);
// set handler for prefetch_abort: return previous.
mmu_handler_t mmu_set_prefetch_abort(mmu_handler_t h);

// setup pid, asid and pt in hardware.
static inline void
mmu_set_ctx(uint32_t pid, uint32_t asid, void *pt) {
    assert(asid!=0);
    assert(asid<64);
    assert(pid>64);
    set_procid_ttbr0(pid, asid, pt);
}

/*
 * Memory attributes
 */

#include "libc/bit-support.h"

// you can flip these back and forth if you want debug output.
#if 0
    // change <output> to <debug> if you want file/line/func
#   define pin_debug(args...) output("PIN_VM:" args)
#else
#   define pin_debug(args...) do { } while(0)
#endif

// kinda iffy to have this here...
enum { MB = 1024 * 1024 };

// kernel domain = 1.  we leave 0 free so can sanity check.
enum { kern_dom = 1 };
enum { user_dom = 2 }; 


// this enum flag is a three bit value
//      AXP:1 << 2 | AP:2 
// so that you can just bitwise or it into the 
// position for AP (which is adjacent to AXP).
//
// if _priv access = kernel only.
// if _user access, implies kernel access (but
// not sure if this should be default).
//
// see: 3-151 for table or B4-9 for more 
// discussion.
typedef enum {
    perm_rw_user = 0b011, // read-write user 
    perm_ro_user = 0b010, // read-only user
    perm_na_user = 0b001, // no access user

    // kernel only, user no access
    perm_ro_priv = 0b101,
    // perm_rw_priv = perm_na_user,
    perm_rw_priv = perm_na_user,
    perm_na_priv = 0b000,
} mem_perm_t;

static inline int mem_perm_islegal(mem_perm_t p) {
    switch(p) {
    case perm_rw_user:
    case perm_ro_user:
    // case perm_na_user:
    case perm_ro_priv:
    case perm_rw_priv:
    case perm_na_priv:
        return 1;
    default:
        // for today just die.
        panic("illegal permission: %b\n", p);
    }
}

// domain permisson enums: see b4-10
enum {
    DOM_no_access   = 0b00, // any access = fault.
    // client accesses check against permission bits in tlb
    DOM_client      = 0b01,
    // don't use.
    DOM_reserved    = 0b10,
    // TLB access bits are ignored.
    DOM_manager     = 0b11,
};

// caching is controlled by the TEX, C, and B bits.
// these are laid out contiguously:
//      TEX:3 | C:1 << 1 | B:1
// we construct an enum with a value that maps to
// the description so that we don't have to pass
// a ton of parameters.
//
// from table 6-2 on 6-15:
#define TEX_C_B(tex,c,b)  ((tex) << 2 | (c) << 1 | (b))
typedef enum { 
    //                              TEX   C  B 
    // strongly ordered
    // not shared.
    MEM_device     =  TEX_C_B(    0b000,  0, 0),  
    // normal, non-cached
    MEM_uncached   =  TEX_C_B(    0b001,  0, 0),  

    // write back no alloc
    MEM_wb_noalloc =  TEX_C_B(    0b000,  1, 1),  
    // write through no alloc
    MEM_wt_noalloc =  TEX_C_B(    0b000,  1, 0),  

    // NOTE: missing a lot!
} mem_attr_t;

#define mem_attr_TEX(m) bits_get(m,2,4)
#define mem_attr_B(m) bit_get(m,0)
#define mem_attr_C(m) bit_get(m,1)

// attributes: these get inserted into the TLB.
typedef struct {
    // for today we only handle 1MB sections.
    uint32_t G,         // is this a global entry?
             asid,      // don't think this should actually be in this.
             dom,       // domain id
             pagesize;  // can be 1MB or 16MB

    // permissions for needed to access page.
    //
    // see mem_perm_t above: is the bit merge of 
    // APX and AP so can or into AP position.
    mem_perm_t  AP_perm;

    // caching policy for this memory.
    // 
    // see mem_cache_t enum above.
    // see table on 6-16 b4-8/9
    // for today everything is uncacheable.
    mem_attr_t mem_attr;
} pin_t;

// default: 
//  - 1mb section
static inline pin_t
pin_mk(uint32_t G,
            uint32_t dom,
            uint32_t asid,
            mem_perm_t perm, 
            mem_attr_t mem_attr) {
    demand(mem_perm_islegal(perm), "invalid permission: %b\n", perm);
    demand(dom <= 16, illegal domain id);
    if(G)
        demand(!asid, "should not have a non-zero asid: %d", asid);
    else {
        demand(asid, should have non-zero asid);
        demand(asid > 0 && asid < 64, invalid asid);
    }

    return (pin_t) {
            .dom = dom,
            .asid = asid,
            .G = G,
            // default: 1MB section.
            .pagesize = 0b11,
            // default: uncacheable
            // .mem_attr = MEM_uncached, // mem_attr,
            .mem_attr = mem_attr,
            .AP_perm = perm };
}

// pattern for overriding values.
static inline pin_t
pin_mem_attr_set(pin_t e, mem_attr_t mem) {
    e.mem_attr = mem;
    return e;
}

// encoding for different page sizes.
enum {
    PAGE_4K     = 0b01,
    PAGE_64K    = 0b10,
    PAGE_1MB    = 0b11,
    PAGE_16MB   = 0b00
};

// override default <p>
static inline pin_t pin_16mb(pin_t p) {
    p.pagesize = PAGE_16MB;
    return p;
}
static inline pin_t pin_64k(pin_t p) {
    p.pagesize = PAGE_64K;
    return p;
}

// possibly we should just multiply these.
enum {
    _4k     = 4*1024, 
    _64k    = 64*1024,
    _1mb    = 1024*1024,
    _16mb   = 16*_1mb
};

// return the number of bytes for <attr>
static inline unsigned
pin_nbytes(pin_t attr) {
    switch(attr.pagesize) {
    case 0b01: return _4k;
    case 0b10: return _64k;
    case 0b11: return _1mb;
    case 0b00: return _16mb;
    default: panic("invalid pagesize\n");
    }
}

// check page alignment. as is typical, arm1176
// requires 1mb pages to be aligned to 1mb; 16mb 
// aligned to 16mb, etc.
static inline unsigned 
pin_aligned(uint32_t va, pin_t attr) {
    unsigned n = pin_nbytes(attr);
    switch(n) {
    case _4k:   return va % _4k == 0;
    case _64k:  return va % _64k == 0;
    case _1mb:  return va % _1mb == 0;
    case _16mb: return va % _16mb == 0;
    default: panic("invalid size=%u\n", n);
    }
}

// make a dev entry: 
//    - global bit set.
//    - non-cacheable
//    - kernel R/W, user no-access
static inline pin_t pin_mk_device(uint32_t dom) {
    return pin_mk(1, dom, 0, perm_rw_priv, MEM_device);
}


// make a global entry: 
//  - global bit set
//  - asid = 0
//  - default: 1mb section.
static inline pin_t
pin_mk_global(uint32_t dom, mem_perm_t perm, mem_attr_t attr) {
    // G=1, asid=0.
    return pin_mk(1, dom, 0, perm, attr);
}

// must have:
//  - global=0
//  - asid != 0
//  - domain should not be kernel domain (we don't check)
static inline pin_t
pin_mk_user(uint32_t dom, uint32_t asid, mem_perm_t perm, mem_attr_t attr) {
    return pin_mk(0, dom, asid, perm, attr);
}

/*
 * Page table definitions.
 */

#include "switchto.h"
#include "full-except.h"

typedef fld_t vm_pt_t;

// page table entry (confusingly same type since its an array)
typedef fld_t vm_pte_t;

static inline vm_pte_t vm_pte_mk(void) {
    // all unused fields can have 0 as default.
    return (vm_pte_t){ .tag = 0b10 };
}


// 4gb / 1mb = 4096 entries fully populated for first level
// of page table.
enum { PT_LEVEL1_N = 4096 };


// allocate zero-filled page table with correct alignment.
//  - for today's lab: assume fully populated 
//    (nentries=PT_LEVEL1_N)
vm_pt_t *vm_pt_alloc(unsigned nentries);
vm_pt_t *staff_vm_pt_alloc(unsigned nentries);

vm_pte_t *vm_map_sec(vm_pt_t *pt, uint32_t va, uint32_t pa, pin_t attr);
vm_pte_t *staff_vm_map_sec(vm_pt_t *pt, uint32_t va, uint32_t pa, pin_t attr);

// these don't do much today: exactly the same as pinned
// versions.
void vm_mmu_enable(void);
void vm_mmu_disable(void);

// mmu can be off or on.
void vm_mmu_switch(vm_pt_t *pt, uint32_t pid, uint32_t asid);

// initialize the hardware mmu and set the 
// domain reg [not sure that makes sense to do
// but we keep it b/c we did for pinned]
enum { dom_all_access = ~0, dom_no_access = 0 };

void vm_mmu_init(uint32_t domain_reg);


// lookup va in page table.
vm_pte_t * vm_lookup(vm_pt_t *pt, uint32_t va);
vm_pte_t * staff_vm_lookup(vm_pt_t *pt, uint32_t va);

// manually translate virtual address <va> to its 
// associated physical address --- keep the same
// offset.
vm_pte_t *vm_xlate(uint32_t *pa, vm_pt_t *pt, uint32_t va);
vm_pte_t *staff_vm_xlate(uint32_t *pa, vm_pt_t *pt, uint32_t va);

// allocate new page table and copy pt
vm_pt_t *vm_dup(vm_pt_t *pt);

// do an identy map for the kernel.  if enable_p=1 will
// turn on vm.
vm_pt_t *vm_map_kernel(procmap_t *p, int enable_p);
vm_pt_t *staff_vm_map_kernel(procmap_t *p, int enable_p);

// arm-vm-helpers: print <f>
void vm_pte_print(vm_pt_t *pt, vm_pte_t *pte);

// set the <AP> permissions in <pt> for the range [va, va+1MB*<nsec>) in 
// page table <pt> to <perm>
void vm_mprotect(vm_pt_t *pt, unsigned va, unsigned nsec, pin_t perm);

void vm_taken_list_print();
uint32_t get_first_free_page();


#endif
