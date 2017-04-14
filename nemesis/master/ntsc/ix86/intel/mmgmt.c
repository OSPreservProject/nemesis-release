/*
 *      ntsc/ix86/mmgmt.c
 *      ------------------
 *
 * Copyright (c) 1997 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * This file contains the memory management related NTSC stuff.
 * 
 * ID : $Id: mmgmt.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 */

#include <nemesis.h>
#include <kernel.h>
#include <pip.h>

#include <dcb.h>
#include <io.h>
#include <mmu_tgt.h>
#include <segment.h>
#include <nexus.h>

#include <autoconf/kgdb.h>
#include <autoconf/memsys.h>

#include "inline.h"
#include "../../generic/crash.h"

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif


extern bool_t k_event(dcb_ro_t *rop, Channel_RX rx, Event_Val inc);
extern void k_enter_debugger(context_t *, uint32_t, uint32_t, uint32_t);
extern void load_cr3(addr_t);
extern void k_enable_paging(void);
extern void invlpg(addr_t vaddr);

#define M(_b) ((_b)<<20)
#define K(_b) ((_b)<<10)




/*
** Nasty: for now we keep the memory information in the below 
** structures in the image. We can never free them. 
*/
static Mem_PMemDesc allmem[8];   
static Mem_Mapping  memmap[8];    


/* 
** We keep track of any ptes which need to be 'augmented' (due to 
** additional rights in the protection domain) in the augtab. At 
** most N_AUGS of these will fit (after this we flush the entire 
** table, which is not good for performance)
*/
#define N_AUGS        512
typedef struct {
    hwpte_t *pte; 
    word_t   va; 
} augentry_t; 

static augentry_t augtab[N_AUGS];

#include <RamTab.ih>


void k_sc_map(void);
void k_sc_trans(void);
void k_sc_prot(void);
void k_sc_gprot(void);

void k_psc_wptbr(void);
void k_psc_flushtlb(void);
void k_psc_pginit(void);


#define L1IDX(_va)    ((word_t)(_va) >> 22)
#define L2IDX(_va)    (((word_t)(_va) >> FRAME_WIDTH) & 0x3FF)
#define SHADOW(_va)   ((word_t *)((void *)(_va) + 4096))
#define BAD_PTE       (hwpte_t *)NULL_PTE


static INLINE void dump_pte(hwpte_t *cur, sid_t sid)
{
    k_printf("|    SID   |   PA     |RW|US|WT|CD|AC|DY|GL|AU|NL|VD|\n"); 
    k_printf("|          |          +--+--+--+--+--+--+--+--+--+--+\n"); 
    k_printf("|%x|%x|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|\n", 
	     sid, cur->bits & PTE_M_PFA,
	     (cur->bits & PTE_M_RW)    ? "rw" : "r-",
	     (cur->bits & PTE_M_US)    ? "us" : "-s",
	     (cur->bits & PTE_M_WT)    ? "wt" : "--",
	     (cur->bits & PTE_M_CD)    ? "cd" : "--",
	     (cur->bits & PTE_M_ACS)   ? "ac" : "--",
	     (cur->bits & PTE_M_DTY)   ? "dy" : "--",
	     (cur->bits & PTE_M_GBL)   ? "gb" : "--",
	     (cur->bits & PTE_M_AUG)   ? "au" : "--",
	     (cur->bits & PTE_M_NLD)   ? "nl" : "--",
	     (cur->bits & PTE_M_VALID) ? "vd" : "--"
	);
}

hwpte_t *lookup(word_t va, sid_t *sid)
{
    uint32_t l1idx, l2idx; 
    word_t  *l2va;
    word_t   l1pde;

    l1idx = L1IDX(va);
    l1pde = k_st.l1_va[l1idx];
    if(!(l1pde & PDE_M_VALID)) {
	TRC(k_printf("lookup: invalid va %x\n", va));
	*sid = SID_NULL;
	return BAD_PTE;
    }

    if(l1pde & PDE_M_PGSZ) {
	*sid = (sid_t)(SHADOW(k_st.l1_va))[l1idx]; 
	return ((hwpte_t *)(&k_st.l1_va[l1idx])); 
    }

    l2va  = (word_t *)(k_st.l2tab[l1idx] & PDE_M_PTA);
    l2idx = L2IDX(va);
    *sid  = (sid_t)(SHADOW(l2va))[l2idx];
    return ((hwpte_t *)(&l2va[l2idx]));
}


void k_sc_map(void)
{
    word_t     vpn, pfn, bits, newpte; 
    word_t     va, pa; 
    hwpte_t   *pte;
    sid_t      sid;
    uchar_t    mask;
    uint32_t   rights;
    context_t *cp  = k_st.ccxs;
    dcb_ro_t  *rop = k_st.cur_dcb;  

    vpn  = cp->gpreg[r_eax];
    pfn  = cp->gpreg[r_ebx];
    bits = cp->gpreg[r_ecx];

    TRC(k_printf("ntsc_map called: vpn=%p, pfn=%p, bits=%x\n", 
		 vpn, pfn, bits));
    va = vpn << PAGE_WIDTH;

    /* If we have no domain, we can't validate anything */
    if(!rop) {

	/* Shouldn't really happen, but ignore it since so early */
	k_printf("ntsc_map called before there is a domain!\n");
	k_printf(" => skipping validation check on pfn.\n");

    } else if(bits & PTE_M_VALID) {
	RamTable_t *rtab = (RamTable_t *)rop->ramtab; 

	/* We are mapping something *in* => need to validate the pfn */

	if(pfn >= rop->maxpfn) {
	    k_printf("ntsc_map: pfn %x is out of bounds (maxpfn is %x)\n", 
		     pfn, rop->maxpfn);
	    cp->gpreg[r_eax] = MEMRC_BADPA;
	    return;
	}

	if(rtab[pfn].owner != (uint32_t)rop) {
	    k_printf("ntsc_map: pfn %x not owned by caller %x\n", 
		     pfn, (word_t)rop->id);
	    k_printf("rop is %x, owner in ramtab is %x\n", 
		     rop, rtab[pfn].owner);
	    cp->gpreg[r_eax] = MEMRC_BADPA;
	    return;
	}

	if(rtab[pfn].state == RamTab_State_Mapped) {
	    k_printf("PFN %x is mapped: check if remap\n", pfn);

	    /* XXX do me */

	    cp->gpreg[r_eax] = 999;
	    return;
	}

	if(rtab[pfn].state == RamTab_State_Nailed) {
	    k_printf("PFN %x is nailed down.\n", pfn);
	    cp->gpreg[r_eax] = MEMRC_NAILED; 
	    return;
	}

	if(rtab[pfn].fwidth > FRAME_WIDTH) {
	    k_printf("PFN %x has non-standard frame width %x\n", 
		     rtab[pfn].fwidth);

	    /* XXX do me */

	    cp->gpreg[r_eax] = 666;
	    return;
	}
	
    } 

    pa = pfn << FRAME_WIDTH;
    TRC(k_printf("Ok: got validated va=%p, pa=%p\n", va, pa));

    if(!k_st.mmu_ok) {
	/* user-level mmu code hasn't registered yet */
	k_printf("ntsc_map called before MMU code has registered.\n");
	cp->gpreg[r_eax] = 123;
	return;
    }

    if( (pte = lookup((word_t)va, &sid)) == BAD_PTE) {
	k_printf("ntsc_map: bogus va %x\n", va);
	cp->gpreg[r_eax] = MEMRC_BADVA;
	return;
    }


#ifdef CONFIG_NOPROT
    bits |= PTE_M_RW | PTE_M_US;  /* everyone gets read and write */
#else
    /* Check SID => meta rights */
    mask = sid & 1 ? 0xf0 : 0x0f;       /* Get the sid mask */
    if(k_st.pdom != NULL) {
	rights = k_st.pdom[sid>>1] & mask;
	if (sid & 1) rights >>= 4;
	if(!(rights & 8)) {
	    k_printf("ntsc_map: caller [d%x] has not meta rights! ", 
		     k_st.cur_dcb ? k_st.cur_dcb->id & 0xFF : 0);
	    k_printf("caller rights were RWEM (%s%s%s%s)\n", 
		     rights & 1 ? "R" : "-", 
		     rights & 2 ? "W" : "-", 
		     rights & 4 ? "E" : "-", 
		     rights & 8 ? "M" : "-");
	    cp->gpreg[r_eax] = MEMRC_NOACCESS; 
	    return;
	}
    } else TRC(k_printf("no pdom => skpping check for meta rights.\n"));
#endif

    newpte  = ((word_t)pa & PTE_M_PFA) | bits; /* XXX SMH: keep orig rights! */
    TRC(k_printf("ntsc_map: installing mapping, newpte=%x\n", newpte));
    pte->bits = newpte; 

    cp->gpreg[r_eax] = MEMRC_OK;
    return;
}


#if 0
/* This used to be a syscall, but now done INLINE from user space */
void k_sc_trans(void)
{
    hwpte_t   *pte;
    word_t     va, vpn; 
    sid_t      sid; 
    context_t *cp = k_st.ccxs;

    vpn = cp->gpreg[r_eax];
    va  = vpn << PAGE_WIDTH;
    TRC(k_printf("ntsc_trans called: va=%p\n", va));

    if(!k_st.mmu_ok) {
	/* user-level mmu code hasn't registered yet */
	k_printf("ntsc_trans called before MMU code has registered.\n");
	cp->gpreg[r_eax] = 0x123;
	return;
    }

    if( (pte = lookup((word_t)va, &sid)) == BAD_PTE) {
	k_printf("ntsc_trans: bogus va %x\n", va);
	cp->gpreg[r_eax] = MEMRC_BADVA;
	return;
    }

    TRC(k_printf("ntsc_trans: va=%x maps to pa=%x (sid is %x, pte is %x)\n", 
	     (word_t)va & PTE_M_PFA, pte->bits & PTE_M_PFA, sid, 
	     pte->bits));
    cp->gpreg[r_eax] = pte->bits;
    return;
}
#endif


static bool_t sanity_check(word_t vpn, word_t npages, 
			   hwpte_t **pte, sid_t *sid)
{
    hwpte_t *chk_pte;
    word_t   va, chk_va; 
    sid_t    chk_sid;


    /* First check the starting va is ok */
    va = (vpn << PAGE_WIDTH);
    if((*pte = lookup(va, sid)) == BAD_PTE) {
	k_printf("sanity_check: bogus starting va %x\n", va);
	return False;
    }

    /* Now check the last va include is ok, and has the same sid */
    if(npages > 1) {
	chk_va = (vpn + npages - 1) << PAGE_WIDTH; 
	if((chk_pte = lookup(chk_va, &chk_sid)) == BAD_PTE) {
	    k_printf("sanity_check: bogus finishing va %x\n", va);
	    return False;
	}
	if(chk_sid != *sid) {
	    k_printf("sanity_check: range [%x,%x] spans > 1 stretch.\n", 
		     va, chk_va + PAGE_WIDTH - 1); 
	    return False;
	}
    }

    /* Check the predecessor and successor *don't* have the same sid */
    if(vpn != 0) {
	chk_va = (vpn - 1) << PAGE_WIDTH; 
	if((chk_pte = lookup(chk_va, &chk_sid)) != BAD_PTE) {
	    if(chk_sid == *sid) {
		k_printf("sanity_check: page pre [%x,%x] in same stretch.\n", 
			 va, ((vpn + npages) << PAGE_WIDTH) - 1);
		k_printf("Sid is %x (pte %x); prior sid is %x (pte %x)\n", 
			 *sid, (*pte)->bits, chk_sid, chk_pte->bits);
		k_halt();
		return False;
	    }
	}
    }
    
    chk_va = (vpn + npages) << PAGE_WIDTH; 
    if((chk_pte = lookup(chk_va, &chk_sid)) != BAD_PTE) {
	if(chk_sid == *sid) {
	    k_printf("sanity_check: page post [%x,%x] in same stretch.\n", 
		     va, ((vpn + npages) << PAGE_WIDTH) - 1);
		k_printf("Sid is %x (pte %x); post sid is %x (pte %x)\n", 
			 *sid, (*pte)->bits, chk_sid, chk_pte->bits);
	    k_halt();
	    return False;
	}
    }

    return True;
}


void k_sc_nail(void)
{
    context_t *cp = k_st.ccxs;
    uint32_t   vpn, npages, rights, axs;
    uint16_t   newbits, newsbits;
    uchar_t    mask;
    hwpte_t   *pte;
    word_t     va;
    sid_t      sid, newsid;
    swpte_t   *shadow;

    if(!k_st.mmu_ok) {
	/* user-level mmu code hasn't registered yet */
	k_printf("ntsc_nail called before MMU code has registered.\n");
	return;
    }

    vpn    = (uint32_t)cp->gpreg[r_eax];
    npages = (uint32_t)cp->gpreg[r_ebx];
    axs    = (uint32_t)cp->gpreg[r_ecx];

    if(!npages) {     /* all done */
	cp->gpreg[r_eax] = MEMRC_OK;
	return;
    }

    va     = vpn << PAGE_WIDTH; 

    TRC(k_printf("ntsc_nail called: va=%p, npages=%x, access=%x\n", 
	     va, npages, axs));
    (k_printf("ntsc_nail called: va=%p, npages=%x, access=%x\n", 
	      va, npages, axs));
    k_halt();
#if 0
    newbits = 0;

    if(SET_IN(axs, Stretch_Right_Read) || SET_IN(axs, Stretch_Right_Execute))
	newbits |= PTE_M_US;
    
    if(SET_IN(axs, Stretch_Right_Write))
	newbits |= PTE_M_RW | PTE_M_US;
    
    newsbits = newbits; 
    if(SET_IN(axs, Stretch_Right_Meta))    
	newsbits |= PTE_M_MT;

    TRC(k_printf("newbits = %x, newsbits = %x\n", newbits, newsbits));

    /* Now sort out how these new hw bits affect the existing ones */
    if( (pte = lookup((word_t)va, &sid)) == BAD_PTE) {
	k_printf("ntsc_nail: bogus va %x\n", va);
	cp->gpreg[r_eax] = MEMRC_BADVA;
	return;
    }

    /* check sid for meta */
    mask = sid & 1 ? 0xf0 : 0x0f;       /* Get the sid mask */
    if(k_st.pdom != NULL) {
	rights = k_st.pdom[sid>>1] & mask;
	if (sid & 1) rights >>= 4;
	if(!(rights & 8)) {
	    k_printf("ntsc_nail: caller [d%x] has not meta rights! ", 
		     k_st.cur_dcb ? 0 : k_st.cur_dcb->id & 0xFF);
	    k_printf("caller rights were RWEM (%s%s%s%s)\n", 
		     rights & 1 ? "R" : "-", 
		     rights & 2 ? "W" : "-", 
		     rights & 4 ? "E" : "-", 
		     rights & 8 ? "M" : "-");
	    cp->gpreg[r_eax] = MEMRC_NOACCESS; 
	    return;
	}
    } else TRC(k_printf("no pdom => skpping check for meta rights.\n"));


    newsid = sid; 

    if((pte->bits & (PTE_M_RW | PTE_M_US)) == newbits) {
	TRC(k_printf("ntsc_nail: protections same, so done.\n"));
	cp->gpreg[r_eax] = MEMRC_OK;
	return;
    }

    while(npages--) {
	pte->bits = (pte->bits & ~(PTE_M_RW | PTE_M_US)) | newbits;
	shadow = (swpte_t *)SHADOW(pte);
	shadow->ctl.all = (shadow->ctl.all & 
			   ~(PTE_M_RW|PTE_M_US|PTE_M_MT)) | newsbits;

	if(npages) {
	    if((++vpn & 0x3ff) == 0) {  /* wrapped onto a new l2 ptab */
		
		va = vpn << PAGE_WIDTH; 
		TRC(k_printf("ntsc_nail: wrapped at va=%p\n", va));
		
		if( (pte = lookup((word_t)va, &sid)) == BAD_PTE) {
		    k_printf("ntsc_nail: bogus va %x during prot!!!\n", va);
		    cp->gpreg[r_eax] = MEMRC_BADVA;
		    return;
		}
		
		if(newsid != sid) {
		    k_printf("ntsc_nail: bogus sid %x (should be %x)\n", 
			     newsid, sid);
		    cp->gpreg[r_eax] = MEMRC_BADVA;
		    return;
		}
		
	    } else pte++;
	}
    }
    
    TRC(k_printf("ntsc_nail: done!\n"));
    cp->gpreg[r_eax] = MEMRC_OK;
#endif
    return;
}


void k_sc_prot(void)
{
    context_t *cp = k_st.ccxs;
    uchar_t   *pdom, mask;
    uint32_t   vpn, npages, rights, oldrights; 
    word_t   **pdom_tbl = (word_t **)INFO_PAGE.pd_tab; 
    word_t     pdidx, va;
    hwpte_t   *pte;
    sid_t      sid;
    int        i;

    if(!k_st.mmu_ok) {
	/* user-level mmu code hasn't registered yet */
	k_printf("ntsc_prot called before MMU code has registered.\n");
	cp->gpreg[r_eax] = 0x123; 
	return;
    }

    pdidx  = PDIDX(cp->gpreg[r_eax]); 
    vpn    = (uint32_t)cp->gpreg[r_ebx];
    va     = (word_t)(vpn << PAGE_WIDTH);
    npages = (uint32_t)cp->gpreg[r_ecx];
    rights = (uint32_t)cp->gpreg[r_edx];

    if(!npages) {     /* all done */
	cp->gpreg[r_eax] = MEMRC_OK;
	return;
    }

    if( (pdidx >= PDIDX_MAX) || !(pdom = (uchar_t *)pdom_tbl[pdidx])) {
	k_printf("ntsc_prot: bogus pdom identifier %x\n",
		 cp->gpreg[r_eax]); 
	cp->gpreg[r_eax] = MEMRC_BADPDOM; 
	return;
    }

    if(!sanity_check(vpn, npages, &pte, &sid)) {
	k_printf("ntsc_prot: bogus va range [%x,%x]\n", va, 
		 va + (npages << PAGE_WIDTH) - 1);
	cp->gpreg[r_eax] = MEMRC_BADVA;
	return;
    }

    TRC(k_printf("ntsc_prot called: pdom at %p, sid=%x, rights=%s%s%s%s\n", 
	     pdom, sid, 
	     rights & 1 ? "R" : "-", 
	     rights & 2 ? "W" : "-", 
	     rights & 4 ? "E" : "-", 
	     rights & 8 ? "M" : "-"));

    mask = sid & 1 ? 0xf0 : 0x0f;       /* Get the sid mask */

    if(k_st.pdom != NULL) {
	oldrights = k_st.pdom[sid>>1] & mask;
	if (sid & 1) oldrights >>= 4;
	if(!(oldrights & 8)) {
	    k_printf("ntsc_prot: caller [d%x] has not meta rights! ", 
		     k_st.cur_dcb ? 0 : k_st.cur_dcb->id & 0xFF);
	    k_printf("caller rights were RWEM (%s%s%s%s)\n", 
		     oldrights & 1 ? "R" : "-", 
		     oldrights & 2 ? "W" : "-", 
		     oldrights & 4 ? "E" : "-", 
		     oldrights & 8 ? "M" : "-");
	    cp->gpreg[r_eax] = MEMRC_NOACCESS; 
	    return;
	}
    } else TRC(k_printf("no pdom => skpping check for meta rights.\n"));

    TRC(k_printf("ntsc_prot: updating rights\n"));
    if(sid & 1) rights <<= 4; 
    pdom[sid>>1] = (rights & mask) | (pdom[sid>>1] & ~mask);


#ifdef DEBUG
    {
	Stretch_Rights res;
	mask   = sid & 1 ? 0xf0 : 0x0f;       /* Get the sid mask */
	res    = pdom[sid>>1] & mask;
	if (sid & 1) res >>= 4;

	TRC(k_printf("ntsc_prot: now pdom info is RWEM (%s%s%s%s)\n", 
		 res & 1 ? "R" : "-", 
		 res & 2 ? "W" : "-", 
		 res & 4 ? "E" : "-", 
		 res & 8 ? "M" : "-"));
    }
#endif

    /* And now invalidate all virtual addresses in stretch */
    for(i = 0; i < npages; i++)
	invlpg((void *)((vpn + i) << PAGE_WIDTH));

    cp->gpreg[r_eax] = MEMRC_OK; 
    return;
}

void k_sc_gprot(void)
{
    context_t *cp = k_st.ccxs;
    uint32_t   vpn, npages, rights, axs;
    uint16_t   newbits, newsbits;
    uchar_t    mask;
    hwpte_t   *pte;
    word_t     va;
    sid_t      sid, newsid;
    swpte_t   *shadow;

    if(!k_st.mmu_ok) {
	/* user-level mmu code hasn't registered yet */
	k_printf("ntsc_gprot called before MMU code has registered.\n");
	return;
    }

    vpn    = (uint32_t)cp->gpreg[r_eax];
    npages = (uint32_t)cp->gpreg[r_ebx];
    va     = vpn << PAGE_WIDTH; 
    axs    = (uint32_t)cp->gpreg[r_ecx];

    TRC(k_printf("ntsc_gprot called: va=%p, npages=%x, access=%x\n", 
	     va, npages, axs));

    if(!npages) {     /* all done */
	cp->gpreg[r_eax] = MEMRC_OK;
	return;
    }

    if(!sanity_check(vpn, npages, &pte, &sid)) {
	k_printf("ntsc_prot: bogus va range [%x,%x]\n", va, 
		 va + (npages << PAGE_WIDTH) - 1);
	cp->gpreg[r_eax] = MEMRC_BADVA;
	return;
    }


#ifdef CONFIG_NOPROT
    newbits  = PTE_M_RW | PTE_M_US;  /* everyone gets read and write */
    newsbits = newbits; 
#else

    newbits = 0;

    if(SET_IN(axs, Stretch_Right_Read) || SET_IN(axs, Stretch_Right_Execute))
	newbits |= PTE_M_US;
    
    if(SET_IN(axs, Stretch_Right_Write))
	newbits |= PTE_M_RW | PTE_M_US;
    
    newsbits = newbits; 
    if(SET_IN(axs, Stretch_Right_Meta))    
	newsbits |= PTE_M_MT;
#endif

    TRC(k_printf("newbits = %x, newsbits = %x\n", newbits, newsbits));

#ifndef CONFIG_NOPROT
    /* check sid for meta */
    mask = sid & 1 ? 0xf0 : 0x0f;       /* Get the sid mask */
    if(k_st.pdom != NULL) {
	rights = k_st.pdom[sid>>1] & mask;
	if (sid & 1) rights >>= 4;
	if(!(rights & 8)) {
	    k_printf("ntsc_gprot: caller [d%x] has not meta rights! ", 
		     k_st.cur_dcb ? 0 : k_st.cur_dcb->id & 0xFF);
	    k_printf("caller rights were RWEM (%s%s%s%s)\n", 
		     rights & 1 ? "R" : "-", 
		     rights & 2 ? "W" : "-", 
		     rights & 4 ? "E" : "-", 
		     rights & 8 ? "M" : "-");
	    cp->gpreg[r_eax] = MEMRC_NOACCESS; 
	    return;
	}
    } else TRC(k_printf("no pdom => skpping check for meta rights.\n"));
#endif

    newsid = SID_NULL; 

    /* Now sort out how these new hw bits affect the existing ones */
    if((pte->bits & (PTE_M_RW | PTE_M_US)) == newbits) {
	TRC(k_printf("ntsc_gprot: protections same, so done.\n"));
	cp->gpreg[r_eax] = MEMRC_OK;
	return;
    }

    while(npages--) {
	pte->bits = (pte->bits & ~(PTE_M_RW | PTE_M_US)) | newbits;

	shadow    = (swpte_t *)SHADOW(pte);
	shadow->ctl.all = (shadow->ctl.all & 
			   ~(PTE_M_RW|PTE_M_US|PTE_M_MT)) | newsbits;

	invlpg((addr_t)(vpn << PAGE_WIDTH));

	if(npages) {
	    if((++vpn & 0x3ff) == 0) {  /* wrapped onto a new l2 ptab */
		
		va = vpn << PAGE_WIDTH; 
		TRC(k_printf("ntsc_gprot: wrapped at va=%p\n", va));
		
		if( (pte = lookup((word_t)va, &newsid)) == BAD_PTE) {
		    k_printf("ntsc_gprot: bogus va %x during prot!!!\n", va);
		    cp->gpreg[r_eax] = MEMRC_BADVA;
		    return;
		}
		
		if(newsid != sid) {
		    k_printf("ntsc_gprot: bogus sid %x (should be %x)\n", 
			     newsid, sid);
		    cp->gpreg[r_eax] = MEMRC_BADVA;
		    return;
		}
		
	    } else pte++;
	}
    }
    
    TRC(k_printf("ntsc_gprot: done!\n"));
    cp->gpreg[r_eax] = MEMRC_OK;
    return;
}

void k_psc_wptbr(void)
{
    context_t *cp=k_st.ccxs;

    k_st.l1_va  = (word_t *)cp->gpreg[r_eax];
    k_st.l1_pa  = (word_t *)cp->gpreg[r_ebx];
    k_st.l2tab  = (word_t *)cp->gpreg[r_ecx];
    k_st.augtab = (word_t *)augtab;
    k_st.augidx = 0;

    load_cr3(k_st.l1_pa);
    k_enable_paging();

    k_st.mmu_ok = True;
}

void k_psc_flushtlb(void)
{
    load_cr3(k_st.l1_pa);
}

void k_psc_wrpdom(void)
{
    context_t *cp = k_st.ccxs;
    word_t pdom; 

    pdom = cp->gpreg[r_eax];

    if((word_t)pdom & (PAGE_SIZE-1)) {
	start_crashdump("k_psc_wrpdom");
	k_printf("ntsc_wrpdom: bad [unaligned] pdom %x\n", pdom); 
	end_crashdump();
	k_halt();
    }

    k_st.pdom = (uint8_t *)pdom;

    return;
}

/* ---------------------- Memory Management Faults  ------------------- */


/*
** k_dispatch_fault():
**    send an event on a domain's mmgmt endpoint and reactivate.
**    preconditions: domain has acts on and a valid mmgmt endpoint.
*/
void k_dispatch_fault(dcb_ro_t *rop, Channel_Endpoint mm_ep)
{
    dcb_rw_t *rwp = rop->dcbrw;
    hwpte_t *pte; 
    context_t *cx = k_st.ccxs;
    sid_t sid; 
    static char   *code_name[] = {
	"MM_K_TNV",
	"MM_K_UNA",
	"MM_K_ACV",
	"MM_K_INST",
	"MM_K_FOE",
	"MM_K_FOW",
	"MM_K_FOR",
	"MM_K_PAGE",
	"MM_K_BPT",
    };

    TRC(k_printf("k_dispatch_fault: rop=%x, ep=%x, fstat=%x\n", 
	     rop, mm_ep, rwp->mm_code));
    TRC(k_printf("                  pc=%x, va=%x, sp=%x\n", 
	     cx->eip, rwp->mm_va, cx->esp));

    /* 
    ** XXX SMH: vile vile vile. We have to lookup the bad va 
    ** in order to determine why it faulted!!! The fstat from 
    ** coprocessor does not hold enough info for us to distinguish
    ** between PAGE and TNV. Possibly may be fixed with weird access
    ** stuff and/or 'large' page cheating. 
    ** XXX BTW: if we want to distinguish between read and write 
    ** access causing faults, we need to disassemble the faulting
    ** instruction by hand ala netbsd. YUK.
    */
    if((pte = lookup((word_t)rwp->mm_va, &sid)) == BAD_PTE)
	rwp->mm_code = Mem_Fault_TNV;
    else if(!(pte->bits & PTE_M_VALID))
	rwp->mm_code = Mem_Fault_PAGE;
    else rwp->mm_code = Mem_Fault_ACV;

    /* Now patch up the stretch via sstab */
    rwp->mm_str = (sid == SID_NULL) ? (Stretch_cl *)NULL :
	((Stretch_clp *)INFO_PAGE.ss_tab)[sid]; 
    TRC(k_printf("sid is %x => setting stretch to %x\n", sid, rwp->mm_str));

    if(rwp->mm_str == NULL) {
	start_crashdump("k_dispatch_fault");
	k_printf("k_dispatch_fault: '%s' rop=%x, ep=%x\n", 
	     code_name[rwp->mm_code], rop, mm_ep);
	k_printf("                  pc=%x, va=%x, sp=%x\n", 
	     cx->eip, rwp->mm_va, cx->esp);
	k_printf("sid=%x, pte= BAD\n", sid);
	end_crashdump();
	k_enter_debugger(cx, k_st.cs, 0, rwp->mm_code);
    }

#ifdef CONFIG_KGDB
    /* If above defined, then we enter the kernel debugger for
       any *unsatisfiable* faults. Other faults, such as PAGE, 
       BPT, etc, are still dispatched to user space however. */
    if(rwp->mm_code < MM_K_FOW) 
	k_mmgmt(rwp->mm_code, 0xf00b, k_st.cs);
#endif

    if(!k_event(rop, mm_ep, 1)) 
	goto bogus;
    k_activate(rop, Activation_Reason_Event);

    /* NOT REACHED */
bogus:
    k_panic("Bogosity in k_dispatch_fault.\n");
}



static void INLINE k_augment_pte(hwpte_t *pte, word_t va, sid_t sid) {
    
    uint16_t i, newbits;
    Stretch_Rights res;
    uchar_t mask;

    mask   = sid & 1 ? 0xf0 : 0x0f;       /* Get the sid mask */
    res    = k_st.pdom[sid>>1] & mask;
    if (sid & 1) res >>= 4;

    TRC(k_printf("pdom info is RWEM (%s%s%s%s)\n", 
	     res & 1 ? "R" : "-", 
	     res & 2 ? "W" : "-", 
	     res & 4 ? "E" : "-", 
	     res & 8 ? "M" : "-"));

    newbits = PTE_M_AUG;

    if(SET_IN(res, Stretch_Right_Read) || SET_IN(res, Stretch_Right_Execute)) 
	newbits |= PTE_M_US;
    if(SET_IN(res, Stretch_Right_Write))
	newbits |= PTE_M_RW | PTE_M_US;

    pte->bits |= newbits; 
    
    TRC(k_printf("            : patched up PTE: %x\n", cur->bits));
    /* Store a pointer to this pte for later (in swppdom) */

    if(k_st.augidx == N_AUGS) {

	/* Full augmentation table; need to zap things. */
	k_printf("NTSC warning: flushing augmentation table.\n");
	k_printf("This is poor for performance; consider increasing size.\n");

	/* XXX SMH: for now, just zap them all; yippee */
	for (i=0; i < k_st.augidx; i++) {
	    
	    swpte_t *shadow;
	    pte    = augtab[i].pte;
	    shadow = (swpte_t *)SHADOW(pte);

	    pte->ctl.c_us  = shadow->ctl.flags.c_us;
	    pte->ctl.c_rw  = shadow->ctl.flags.c_rw;
	    pte->ctl.c_aug = False; 
	    
	    invlpg((void *)augtab[i].va); /* Remove from TLB */
	}
	k_st.augidx = 0;
    }

    augtab[k_st.augidx].pte = pte;
    augtab[k_st.augidx].va  = va;
    k_st.augidx++;
}

static bool_t k_augment_va(word_t linear, sid_t oldsid) {
    
    sid_t sid;
    hwpte_t *pte = lookup(linear, &sid);

    if(pte == BAD_PTE) {
	TRC(k_printf("k_augment_va: bad va %p\n", linear));
	return False;
    }

    if(sid != oldsid) {
	TRC(k_printf("k_augment_va: got wrong stretch\n"));
	return False;
    }
    if(pte->bits & PTE_M_AUG) {
	TRC(k_printf("k_augment_va: va %p already augmented\n", linear));
	return False;
    }

    k_augment_pte(pte, linear, sid);
    return True;
}

#ifdef LOG_AUGMENTS
#define AUG_TRC(_x) _x

typedef enum { 
    ILLEGAL_ACT, 
    SWP_DEC_LEFT, SWP_AUG, SWP_EVICT, SWP_INVALID, 
    SWP_INC_LIFE, SWP_HALVE_LIFE, SWP_ENTERED,
    PF_NEW, PF_OLD, PF_ALREADY 
} aug_action;

typedef struct _aug_log {
    dcb_ro_t *rop;
    aug_action action;
    augment_entry entry;
} aug_log;

aug_log alog[1024];

aug_log *logptr = alog;
aug_log *logend = alog + 1024;

void INLINE stamp(aug_action action, dcb_ro_t *rop, augment_entry *entry) 
{
    if(logptr < logend) {
	aug_log *log = logptr++;
	log->rop = rop;
	log->action = action;
	if(entry) log->entry = *entry;
    }
}

#else
#define AUG_TRC(_x)    
#endif


/*
** k_prot_fault() is called when we get a #14 fault from the 
** processor because a memory reference failed due to insufficient 
** privilege. 
** In this case, we check if the faulting domain has additional 
** rights stored in its protection domain. If so, we frob in a new PTE, 
** keeping note of the address for later invalidation.
** If not, we want to bail (i.e. notify the user domain).
*/



bool_t k_prot_fault(uint32_t linear, 
		    uint32_t ebp, uint32_t esi, uint32_t edi,
		    uint32_t eax,
		    uint32_t ebx, uint32_t ecx, uint32_t edx,
		    uint32_t error, uint32_t eip, uint32_t cs,
		    uint32_t flags, uint32_t esp3, uint32_t ss3)
{
    sid_t sid;
    hwpte_t *cur;
    dcb_ro_t *rop = k_st.cur_dcb;
    dcb_rw_t *rwp;

    INFO_PAGE.pheartbeat++;

    if(!(error&1)) /* shouldn't get (here for) page faults */
	k_panic("k_prot_fault: linear=%x caused *page* fault.\n", linear);
    
    TRC(k_printf("k_prot_fault: va=%x\n", linear));
    if((cur = lookup(linear, &sid)) == BAD_PTE) {
	k_printf("k_prot_fault: tnv on linear=%x\n", linear);
	return False;
    }

    if (cur->bits & PTE_M_AUG) { 

	/* 
	** Oops: pte has already been augmented for pdom 
	** => genuine access problem, so act appropriately.  
	*/

	TRC(k_printf("k_prot_fault: va=%x already augmented => death!\n", 
		     linear));
	TRC(dump_pte(cur, sid));
	TRC(k_printf("'Raw' hwpte is %x, Error was %x\n", cur->bits, error));
	AUG_TRC(stamp(PF_ALREADY, k_st.cur_dcb, NULL));
	return False;
    } 

    TRC(k_printf("k_prot_fault: patching va=%x: eip=%x, esp3=%x\n", 
	     linear, eip, esp3));

    if((word_t)k_st.pdom & (PAGE_SIZE - 1)) {
	k_printf("k_prot_fault: pdom at %p is unaligned.\n", k_st.pdom);
	k_printf("k_prot_fault: was patching va=%x: eip=%x, esp3=%x\n", 
		 linear, eip, esp3);
	return False;
    }

    k_augment_pte(cur, linear, sid);
    
    /* Add this page to the table of augmented pages for this dcb */
    rwp = rop->dcbrw;
    if(!rwp->aug_cache_busy) {
	word_t i;
	addr_t linear_base = ((addr_t)(linear & (~(FRAME_SIZE-1))));
	
	/* Search from the far end of the table first, since entries
           there are going to fault more frequently */
	
	for(i = rwp->num_augs; i > 0; i--) {
	    augment_entry *aug = &rwp->augments[i-1];
	    if(aug->linear == linear_base) {

		/* We've found the entry. Increase its lifetime in the
                   dcb's augmentation cache. */

		if(aug->max_augs != 1<<12) {
		    aug->max_augs <<= 1;
		}
		aug->augs_left = aug->max_augs;

		/* Ensure the stretch id is up to date */
		aug->sid = sid;

		AUG_TRC(stamp(PF_OLD, rop, aug));
		break;
	    }
	}

	if(!i) {

	    /* This is a new entry. If the cache isn't full, use the
               next slot at the end, otherwise evict the last entry,
               since it's likely to be a recently added one, and thus
               less proven to be in the working set */

	    augment_entry *aug;

	    if(rwp->num_augs < AUG_CACHE_SIZE) {
		aug = &rwp->augments[rwp->num_augs++];
	    } else {
		aug = &rwp->augments[AUG_CACHE_SIZE - 1];
	    }
	    aug->linear = linear_base;
	    aug->sid = sid;

	    /* The initial value of max_augs and augs_left are
	       probably fairly critical for the performance of this
	       cache.  ln(max_augs) determines how frequently a page
	       needs to be accessed to stay in the cache after its
	       first access, and augs_left determines how many times
	       it will be automatically augmented before we leave it
	       out and wait for a fault again.  */

	    aug->max_augs = 1;
	    aug->augs_left = 1;
	    
	    AUG_TRC(stamp(PF_NEW, rop, aug));
	}
		    
    }

    invlpg((addr_t)linear);

    return True; 
}



/*
** When we do a protection domain switch, we need to invalidate
** all the additional PTEs we inserted while the last protection 
** domain was active.  */

void k_swppdom(dcb_ro_t *rop) 
{
    hwpte_t *cur;
    swpte_t *shadow;
    uint32_t i;
    dcb_rw_t *rwp = rop->dcbrw;
    
    if (k_st.pdom != (uchar_t *)rop->pdbase) {

	k_st.pdom = (uchar_t *)rop->pdbase; 

	/* 
	** pdom has changed, so we need to remove any augmented PTEs
	** we inserted into the page table for the last pdom 
	*/

	for (i=0; i < k_st.augidx; i++) {

#ifdef CONFIG_NOPROT
	    k_printf("error! non zero augtab with config noprot!!\n");
	    k_halt();
#endif

	    cur    = augtab[i].pte;
	    shadow = (swpte_t *)SHADOW(cur);

	    cur->ctl.c_us  = shadow->ctl.flags.c_us;
	    cur->ctl.c_rw  = shadow->ctl.flags.c_rw;
	    cur->ctl.c_aug = False; 

	    invlpg((void *)augtab[i].va); /* Remove from TLB */
	}
	k_st.augidx = 0;

	/* Add augmentation entries for recently used pages -
           these will be in the dcb's augmentation cache */
	if(!rwp->aug_cache_busy) {
	    word_t i;
	    
	    /* 'evicted' indicates how many entries before this one
               have been evicted. As we move through the table, if
               'evicted' is non-zero, we shift entries down by that
               many, to ensure that there are no gaps in the table.  */

	    word_t evicted = 0;

	    for(i = 0; i < rwp->num_augs; i++) {
		
		augment_entry *aug = &rwp->augments[i];

		/* If augs_left is non-zero, then this entry won't be
                   evicted this time round, unless it's invalid in
                   some way. If it's > 1, then this entry will be
                   added to the system augmentation cache at this
                   point. If augs_left is zero, we halve
                   max_augs. When max_augs reaches zero, we evict the
                   entry. */
		
		if(aug->augs_left) {
		    aug->augs_left--;
		    AUG_TRC(stamp(SWP_DEC_LEFT, rop, aug));
		    if(aug->augs_left) {
			if(!k_augment_va((uint32_t)aug->linear, aug->sid)) {
			    /* Evict this entry */
			    AUG_TRC(stamp(SWP_INVALID, rop, aug));
			    evicted++;
			    continue;
			}
			AUG_TRC(stamp(SWP_AUG, rop, aug));
		    }

		    /* If there are evicted entries, shift this entry
                       down the table to fill up the gap */
		    if(evicted) {
			aug[-evicted] = *aug;
		    }
		} else {
		    aug->max_augs >>= 1;
		    AUG_TRC(stamp(SWP_DEC_MAX, rop, aug));

		    if(!aug->max_augs) {
			/* This entry isn't being used any more. Evict it */
			AUG_TRC(stamp(SWP_EVICT, rop, aug));
			evicted++;
			
		    }
		}
	    
	    }
	    
	    rwp->num_augs -= evicted;
	
	}

    }
    return;
}




static void dump_va(bool_t recoverable, word_t badva)
{
    hwpte_t *pte;
    uint16_t sid; 
    uint8_t  info;


    if( (pte = lookup(badva, &sid)) == BAD_PTE) {
	/* TNV? how odd. Ignore it */
	k_printf("Bad linear address was (probably): %p. TNV.\n", badva);
	return;
    }
 
    k_printf("Bad linear address was (probably): %p\n", badva);
    dump_pte(pte, sid);

    if(k_st.pdom) {
	if((word_t)k_st.pdom & (PAGE_SIZE - 1))
	    k_printf("WARNING: pdom is %p (unaligned).\n", k_st.pdom);
	info = k_st.pdom[sid>>1] & (sid & 1 ? 0xf0 : 0x0f);
	if(sid & 1) info >>= 4; 
	k_printf("PDom at %p gives rights RWEM (%s%s%s%s)\n", 
		 k_st.pdom, info & 1 ? "R" : "-", 
		 info & 2 ? "W" : "-", info & 4 ? "E" : "-", 
		 info & 8 ? "M" : "-");
    }
}


/*
** When we enter here, we have a memory fault which we cannot
** punt up to some user domain. The stack contains:
**
**    +------+
**    |  SS  |     - stack seg of faulting domain 
**    +------+
**    |  ESP |     - stack pointer of faulting domain
**    +------+
**    |EFLAGS| 
**    +------+
**    |  CS  |     - code seg of faulting domain (should be ring 2 or 3)
**    +------+
**    |  EIP |     - points to faulting instruction
**    +------+
**    | CODE |     - fault code (currently only ACV or UNA).
**    +------+
**
** We have also saved the faulting context in k_st.ccxs (assuming that
** the fault occurred while in user space). 
** In the case of an ACV, we may wish to grab the linear address too...
** Dumps out some info and bails into the kernel debugger.
*/

extern int stub_debug_flag;
extern void handle_exception(int exceptionVector);

void k_mmgmt (int nr, uint32_t eip, uint32_t stack_cs)
{
    static char   *code_name[] = {
	"MM_K_TNV",
	"MM_K_UNA",
	"MM_K_ACV",
	"MM_K_INST",
	"MM_K_FOE",
	"MM_K_FOW",
	"MM_K_FOR",
	"MM_K_PAGE",
	"MM_K_BPT",
    };
    /* XXX cs_text defined in kernel.c */
    extern char *cs_text(uint32_t cs);

    word_t   badva; 

    context_t *cx = k_st.ccxs;
    
    __asm__ __volatile__ ("movl %%cr2, %0" : "=r" (badva) :);

    if(stub_debug_flag) {
	/* We're already in the kernel debugger, and we've barfed on
           something - bail straight into the debugger and get it to
           send an error packet */
	
	/* XXX This is a kludge really, but it seems to work. The
           kernel setup code and the gdb stub are meant to ensure this
           happens automatically - somewhere along the line it's got
           broken. */

	handle_exception(0);
    }
	
    start_crashdump("k_mmgmt");
    k_printf("=== %s (%s)\n", code_name[nr], cs_text(stack_cs));
    k_printf("eax=%p  ebx=%p  ecx=%p  edx=%p\nesi=%p  edi=%p  ebp=%p "
	     "esp=%p\n",
	     cx->gpreg[r_eax], cx->gpreg[r_ebx], cx->gpreg[r_ecx],
	     cx->gpreg[r_edx], cx->gpreg[r_esi], cx->gpreg[r_edi],
	     cx->gpreg[r_ebp], cx->esp);
    k_printf("eip=%p  flags=%p\n",  cx->eip, cx->eflag);
    
    /* Is the badva near the stack pointer? */
    {
	word_t sp_frame = cx->esp >> FRAME_WIDTH;
	word_t va_frame = badva >> FRAME_WIDTH;

	if((sp_frame >= va_frame - 1) && (sp_frame <= va_frame + 1)) {
	    k_printf("*** This may be a stack overrun ***\n");
	}
    }

    /* For certain faults (e.g. ACV, PAGE, etc) it is possible (probable?) 
       that the faulting virtual address is still lurking in cr2. Hence 
       we can try to fetch it and dump some info about it */
    switch(nr) {
    case MM_K_ACV:
    case MM_K_PAGE:
	dump_va(True, badva);
	break;

    case MM_K_TNV:
    case MM_K_UNA:
	dump_va(False, badva);
	break;
    default:
	break;
    }

    /* if the exn was from user mode, then the k_st will still be
     * valid. Otherwise, all bets are off. */

    if (stack_cs == USER_CS) {
	k_printf("DCBRO=%x  DCBRW=%x  domain=%x\n",k_st.cur_dcb,
		 k_st.cur_dcb->dcbrw, (uint32_t)(k_st.cur_dcb->id&0xffffffff));
	k_printf("Activations were %s; mm_ep was %x\n", 
	     k_st.cur_dcb->dcbrw->mode ? "off" : "on", 
	     k_st.cur_dcb->dcbrw->mm_ep);
    }

    end_crashdump();
    k_enter_debugger(cx, k_st.cs, 0, 0);
}


/* ---------------------- Memory Initialisation Routines ----------------- */

void init_mem(struct nexus_primal *primal, word_t memsize)
{
    
    /* First cluster is the 'standard' 640K at start */
    allmem[0].start_addr  = 0; 
    allmem[0].nframes     = K(640) >> FRAME_WIDTH; 
    allmem[0].frame_width = FRAME_WIDTH;
    allmem[0].attr        = 0;

    /* Second cluster is the weird stuff up to 1 Meg */
    allmem[1].start_addr  = (addr_t)K(640);
    allmem[1].nframes     = (M(1) - K(640)) >> FRAME_WIDTH; 
    allmem[1].frame_width = FRAME_WIDTH;
#ifndef CONFIG_MEMSYS_EXPT 
    /* XXX Grunge! Have to pretend this region is 'normal' with 
       pre-epxt memsys since otherwise we can't easily arrange 
       for it to be mapped in the mmu, and hence die on it. */
   allmem[1].attr        = 0;
#else 
   allmem[1].attr        = SET_ELEM(Mem_Attr_NonMemory);
#endif

    /* Third is the 'extended memory', courtesy of boot loader; the
       value passsed in for 'memsize' is the value returned from the
       BIOS by setup.S which (AFAIK) is "total memory in machine 
       less the first megabyte". */
    allmem[2].start_addr  = (addr_t)M(1); 
    allmem[2].nframes     = memsize >> FRAME_WIDTH; 
    allmem[2].frame_width = FRAME_WIDTH;
    allmem[2].attr        = 0;

    /* And that's it; mark end of array */
    allmem[3].nframes     = 0;
    k_st.allmem           = &(allmem[0]);

    /* 
    ** Setup the pre-installed memory mappings. 
    ** Currently we have only one logical mapping which covers the 
    ** entire image and is mapped 1-to-1 onto physical addresses. 
    */
    memmap[0].vaddr   = (addr_t)primal->firstaddr;
    memmap[0].paddr   = (addr_t)primal->firstaddr; /* XXX 121 mapping */
    memmap[0].nframes = 
	(primal->lastaddr - primal->firstaddr) >> FRAME_WIDTH;
    memmap[0].frame_width = FRAME_WIDTH;
    memmap[0].mode    = 0;                 /* XXX fabricate mode bits here */

    memmap[1].nframes = 0;
    k_st.memmap       = &(memmap[0]);

    return;
}
