/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
`*****************************************************************************
**
** FILE:
**
**      ix86/mmu_tgt.h
** 
** DESCRIPTION:
** 
**      Target-dependent MMU definitions
**      
** ID : $Id: mmu_tgt.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
*/

#ifndef mmu_tgt_h
#define mmu_tgt_h

/*
 * This is the format of an Intel Nemesis software PTE
 */
struct pte_ctl {
    uint8_t c_valid  : 1,   /* If set, page exists and is readable   */
	    c_rw     : 1,   /* If set, page is writable              */
            c_us     : 1,   /* If set, page is readable in user mode */
            c_mt     : 1,   /* If set, page has global meta rights   */
            c_res1   : 1,   /* must be set to zero */
            c_a      : 1,
            c_d      : 1,
            c_res2   : 1;   /* must be set to zero */
};

typedef struct swpte {
    uint16_t  	 sid; /* Stretch ID */
    union {
	struct pte_ctl flags;
	uint16_t        all;
    }            ctl;
} swpte_t;


/* Hardware page directory entry */
typedef union {
    struct {
	uint32_t c_valid : 1,
	    c_rw   : 1,
	    c_us   : 1,
	    c_pwt  : 1,  /* Write through */
	    c_pcd  : 1,  /* Cache disable */
	    c_a    : 1,  /* Accessed      */
	    c_rsv  : 1,  /* Reserved (set to zero) */
	    c_ps   : 1,  /* page size, if 1 => 4Mb */
	    c_g    : 1,  /* global page (ignored) */
	    c_aug  : 1,  /* pte has had rights augmented from pdom    */
	    c_avl  : 2,
	    c_pta  : 20; /* page address (4Mb) or page table address (4K) */
    } ctl;
    uint32_t bits;
} hwpde_t;


#define PDE_V_VALID 0
#define PDE_M_VALID (1<<PDE_V_VALID)
#define PDE_V_RW    1
#define PDE_M_RW    (1<<PDE_V_RW)
#define PDE_V_US    2
#define PDE_M_US    (1<<PDE_V_US)
#define PDE_V_WT    3       
#define PDE_M_WT    (1<<PDE_V_WT)
#define PDE_V_CD    4
#define PDE_M_CD    (1<<PDE_V_CD)
#define PDE_V_ACS   5
#define PDE_M_ACS   (1<<PDE_V_ACS)
#define PDE_V_PGSZ  7
#define PDE_M_PGSZ  (1<<PDE_V_PGSZ)
#define PDE_V_AUG   9
#define PDE_M_AUG   (1<<PDE_V_AUG)
#define PDE_V_PTA   12
#define PDE_M_PTA   (0xFFFFF<<PDE_V_PTA)


/* Hardware page table entry */
typedef union {
    struct {
	uint32_t c_valid : 1,
	    c_rw   : 1,
	    c_us   : 1,
	    c_pwt  : 1,  /* Write through */
	    c_pcd  : 1,  /* Cache disable */
	    c_a    : 1,  /* Accessed      */
	    c_d    : 1,  /* Dirty         */
	    c_ps   : 1,  /* Reserved (set to zero) */
	    c_g    : 1,  /* global page   */
	    c_aug  : 1,  /* pte has had rights augmented from pdom    */
	    c_nld  : 1,  /* pte cannot currently be unmapped */
	    c_avl  : 1,
	    c_pfa  : 20; /* 'frame' address (pa of frame mapped onto) */
    } ctl;
    uint32_t bits;
} hwpte_t;

#define PTE_V_VALID 0
#define PTE_M_VALID (1<<PTE_V_VALID)
#define PTE_V_RW    1
#define PTE_M_RW    (1<<PTE_V_RW)
#define PTE_V_US    2
#define PTE_M_US    (1<<PTE_V_US)
#define PTE_V_WT    3               /* bit 3 in hwpte is write through */
#define PTE_M_WT    (1<<PTE_V_WT)
#define PTE_V_MT    3               /* bit 3 in swpte is meta */
#define PTE_M_MT    (1<<PTE_V_MT)
#define PTE_V_CD    4
#define PTE_M_CD    (1<<PTE_V_CD)
#define PTE_V_ACS   5
#define PTE_M_ACS   (1<<PTE_V_ACS)
#define PTE_V_DTY   6
#define PTE_M_DTY   (1<<PTE_V_DTY)
#define PTE_V_GBL   8
#define PTE_M_GBL   (1<<PTE_V_GBL)
#define PTE_V_AUG   9
#define PTE_M_AUG   (1<<PTE_V_AUG)
#define PTE_V_NLD   10
#define PTE_M_NLD   (1<<PTE_V_NLD)
#define PTE_V_PFA   12
#define PTE_M_PFA   (0xFFFFF<<PTE_V_PFA)


#define WORD_LEN        32
#define NULL_PFN        0xFFFFF

/* 
** On x86 we have two sizes of page/frame; the normal size [4K] and 
** the 'large' size of 4Mb. We define these here, along with some
** associated defns/macros. 
*/
#define WIDTH_4MB       22
#define WIDTH_4K        12

#define WIDTH_MIN       WIDTH_4K
#define WIDTH_MAX       WIDTH_4MB

#define VALID_WIDTH(_w) ((_w) == WIDTH_4K || (_w) == WIDTH_4MB)



/*
** Protection domains are implemented as arrays of 4-bit elements, 
** indexed by stretch id. 
*/
typedef uint16_t  sid_t;
#define SID_NULL  0xFFFF
#define SID_MAX   16384

struct _pdom {
    uchar_t rights[SID_MAX/2];
};

typedef struct _pdom   pdom_t;

/* 
** A protection domain identifier contains an index in the low 16 bits, 
** and a "generation" value in the top 16 bits 
*/
#define PDIDX(_pdid)   ((_pdid) & 0xFFFF)
#define PDGEN(_pdid)   ((_pdid) >> 16)

/*
** We maintain a (fixed size) table mapping protection domain indices
** onto pdom_t's - the maximum number of pdoms allowed is given below. 
*/
#define PDIDX_MAX       0x80   /* Allow up to 128 protection domains */


/* The following return values are used from the ntsc mmgmt calls */
#define MEMRC_OK                ((word_t)0)
#define MEMRC_BADVA             ((word_t)-1)
#define MEMRC_NOACCESS          ((word_t)-2)
#define MEMRC_BADPA             ((word_t)-3)
#define MEMRC_NAILED            ((word_t)-4)
#define MEMRC_BADPDOM           ((word_t)-5)



#ifndef ntsc_trans

/* 
** We provide an inline version of ntsc_trans() in here since the cost
** of a system call on x86 machines is prohibitive. 
*/

#define L1IDX(_va)    ((word_t)(_va) >> 22)
#define L2IDX(_va)    (((word_t)(_va) >> FRAME_WIDTH) & 0x3FF)

#define ntsc_trans(_vpn) 						\
({									\
    uint32_t l1idx, l2idx; 						\
    word_t  *l2va, va, res;						\
    if(!(INFO_PAGE.mmu_ok)) {						\
	res = 0x123; 							\
    } else {								\
	va    = (_vpn) << PAGE_WIDTH; 					\
	l1idx = L1IDX(va);						\
									\
	if(!(INFO_PAGE.l1_va[l1idx] & PDE_M_VALID)) 			\
	    res = MEMRC_BADVA; 						\
	else if(INFO_PAGE.l1_va[l1idx] & PDE_M_PGSZ)			\
	    res = INFO_PAGE.l1_va[l1idx];				\
	else {								\
	    l2va  = (word_t *)(INFO_PAGE.l2tab[l1idx] & PDE_M_PTA);	\
	    l2idx = L2IDX(va);						\
	    res = l2va[l2idx];						\
	}								\
    }									\
    res;								\
})
#endif  /* ntsc_trans */




/* Now the arch-independent layer */
#define NULL_PTE             ((word_t)-1)
#define PFN_PTE(_pte)        (((_pte) & PTE_M_PFA) >> PTE_V_PFA)
#define WIDTH_PTE(_pte)      ((_pte & PDE_M_PGSZ) ? WIDTH_4MB : WIDTH_4K)
#define IS_VALID_PTE(_pte)   ((_pte) & PTE_M_VALID)
#define IS_DIRTY_PTE(_pte)   ((_pte) & PTE_M_DTY)
#define IS_REFFED_PTE(_pte)  ((_pte) & PTE_M_ACS)
#define IS_NAILED_PTE(_pte)  ((_pte) & PTE_M_NLD)
#define VALID(_pte)          ((_pte) | PTE_M_VALID)
#define MAP(_pte)            ((_pte) | PTE_M_VALID)
#define UNREF(_pte)          ((_pte) & ~(PTE_M_ACS))
#define UNMAP(_pte)          ((_pte) & ~(PTE_M_VALID | PTE_M_DTY | PTE_M_ACS))
#define BITS(_pte)           ((_pte) & 0xFFF)

#endif /* mmu_tgt_h */
