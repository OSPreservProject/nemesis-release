/*
 *      ntsc/ix86/syscalls.c
 *      ------------------
 *
 * Copyright (c) 1997 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * This file contains the NTSC system calls.
 */

#include <nemesis.h>
#include <context.h>
#include <kernel.h>
#include <pip.h>
#include <irq.h>
#include <platform.h>

#include "i386-stub.h"
#include "asm.h"
#include "segment.h"
#include "inline.h"

#include "../../generic/console.h"
#include "../../generic/crash.h"
#include "../../generic/string.h"

#include <dcb.h>
#include <sdom.h>
#include <io.h>
#include <mmu_tgt.h>
#include <domid.h>

#define PARANOIA 0

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif


#define SCTRACE(_x)

/* Externs (from kernel.c) */
extern kernel_st k_st;
extern void k_activate(dcb_ro_t *rop, uint32_t reason);

typedef void syscall(void);

struct syscall_info {
    syscall *c;
    bool_t priv;
    char *name;
};


/* The below seven routines are all entered directly via the IDT */
void k_sc_reboot(void);
void k_sc_dbgstop(void);
void k_sc_rfa (void);
void k_sc_rfa_resume (void);
void k_sc_rfa_block (void);
void k_sc_block (void);
void k_sc_yield (void);
uint32_t k_dsc_send(Channel_TX tx, Event_Val val);
static void k_sc_putcons(void);

static void k_sc_send(void); /* XXX not used */
static void k_sc_bad(void);


/* memory management system calls */
extern void k_sc_map(void);
extern void k_sc_nail(void);
extern void k_sc_prot(void);
extern void k_sc_gprot(void);
extern void k_psc_wrpdom(void);
extern void k_psc_wptbr(void);
extern void k_psc_flushtlb(void);


/* These routines are dispatched via the k_syscall routine */
static void k_psc_swpipl(void);
static void k_psc_unmask_irq(void);
static void k_psc_kevent(void);
static void k_psc_activate(void);
static void k_psc_mask_irq(void);
static void k_psc_setdbgreg(void);

void k_psc_entkern(void);
void k_psc_leavekern(void);

static void k_psc_chain(void);
static void k_psc_evsel(void);
static void k_psc_bad(void);



/* 
 * Notify the scheduler that "dom" has received events
 */
static void k_kick (dcb_ro_t *rop)
{
    kernel_st    *st       = &k_st;
    uint32_t      new_head = (st->domq.head + 1 ) % CFG_UNBLOCK_QUEUE_SIZE;
    
     if (rop->schst == DCBST_K_WAKING)
    {
	/* If domain has already been kicked, don't kick it again */
	return;
    }

   /* XXX - I'd rather move dom to the right queue here, but that
       would probably break the scheduler code below.  */
    
    if (new_head == st->domq.tail)
    {
	k_printf ("k_kick: lost wakeup for %x\n", rop);
	return; /* XXX - we lose; fix this bogosity */
    }
    
    st->domq.head = new_head;
    st->domq.q[st->domq.head] = rop;
    rop->schst= DCBST_K_WAKING;
    
    k_st.reschedule = True;
}


static bool_t k_send(dcb_ro_t *rop, Channel_RX rx, Event_Val val, bool_t inc)
{
    bool_t	 res;
    uint32_t	 n;
    
    if (! rop ||
	(((word_t) rop) & (FRAME_SIZE - 1)) ||/* aligned wrong	*/
	! rop->epros ||			/* no endpoints		*/
	! rop->eprws || /* XXX XXX This is a wild guess */
	! (n = rop->num_eps) || /* XXX bleurgh */
	! (rx < n))
    {
	k_printf("k_send: bad dom or rx: rop=%x, rx=%x\n", rop, rx);
	breakpoint();
	res = False;
    }
    else
    {
	ep_ro_t *rxrop = DCB_EPRO (rop, rx);
	ep_rw_t *rxrwp = DCB_EPRW (rop, rx);
	dcb_rw_t  *rwp = DCB_RO2RW (rop);
	
	if ( rxrop->val == rxrwp->ack ) {
	    EP_FIFO_PUT (rop, rwp, rx);    
	}

	if(inc) {
	    rxrop->val += val;
	} else { 
	    rxrop->val = val;
	}

	/* Only kick a domain if it is currently blocked. Otherwise we
           either waste time (if the domain is already runnable) or
           risk shifting a stopped/dying domain into the waking state */

	if(rop->schst == DCBST_K_BLCKD) {
	    k_kick (rop);
	}
	res = True;
    }
    
    return res;
}

bool_t k_event(dcb_ro_t *rop, Channel_RX rx, Event_Val inc)
{
    return k_send(rop, rx, inc, True);
}





static const struct syscall_info syscalls[]={
    /* 'Public' system calls */
    {k_sc_reboot,      False, "reboot"},                     /* 0x00 */
    {k_sc_rfa,         False, "rfa"},
    {k_sc_rfa_resume,  False, "rfa_resume"},                 /* 0x02 */
    {k_sc_rfa_block,   False, "rfa_block"},
    {k_sc_block,       False, "block"},                      /* 0x04 */
    {k_sc_yield,       False, "yield"},
    {k_sc_send,        False, "send"},                       /* 0x06 */
    {k_sc_map,         False, "map"},
    {k_sc_nail,        False, "nail"},                       /* 0x08 */
    {k_sc_prot,        False, "prot"},
    {k_sc_gprot,       False, "gprot"},                      /* 0x0A */
    {k_sc_putcons,     False, "putcons"},
    {k_sc_dbgstop,     False, "dbgstop"},                    /* 0x0C */
    {k_sc_bad,         False, "unknown_syscall"},
    {k_sc_bad,         False, "unknown_syscall"},            /* 0x0E */
    {k_sc_bad,         False, "unknown_syscall"},


    /* 'Private' system calls */
    {k_psc_swpipl,     True,  "swpipl"},                     /* 0x10 */
    {k_psc_unmask_irq, True,  "unmask_irq"},                
    {k_psc_kevent,     True,  "kevent"},		     /* 0x12 */
    {k_psc_activate,   True,  "actdom"},                    
    {k_psc_wptbr,      True,  "wrptbr"},		     /* 0x14 */
    {k_psc_flushtlb,   True,  "flushtlb"},                   
    {k_psc_wrpdom,     True,  "wrpdom"},		     /* 0x16 */
    {k_psc_mask_irq,   True,  "mask_irq"},                  
    {k_psc_setdbgreg,  True,  "set_debug_registers"},	     /* 0x18 */
    {k_psc_entkern,    False, "entkern"},                   
    {k_psc_leavekern,  True,  "leavekern"},		     /* 0x1A */
    {k_psc_bad,        True,  "callpriv"},                  
    {k_psc_chain,      True,  "chain"},                      /* 0x1C */
    {k_psc_evsel,      True,  "evsel"},	    
    {k_psc_bad,        True,  "unknown_syscall"},            /* 0x1E */
    {k_psc_bad,        True,  "unknown_syscall"},
};

#define SYSCALL_MASK 0x1f

void k_syscall(uint32_t nr)
{
    context_t *cx;
    const struct syscall_info *sc;
    uint32_t cs;
    
    cx=k_st.ccxs;
    cs=k_st.cs;

    INFO_PAGE.nheartbeat++;

#if PARANOIA
    if (cs==KERNEL_CS) {
	start_crashdump("k_syscall");
	k_printf("k_syscall: system call from within kernel\n");
	end_crashdump();
	k_enter_debugger(cx, cs, 0, 0);
    }
#endif /* PARANOIA */

    nr &= SYSCALL_MASK;

    sc=syscalls+nr;

    if (sc->priv && cs!=PRIV_CS) {
	start_crashdump("k_syscall");
	k_printf("Unprivileged domain attempted privileged system call '%s'\n",
		 sc->name);
	end_crashdump();
	k_enter_debugger(cx, cs, 0, 0);
    }

    sc->c();

    k_presume(cx, cs, DS(cs));
}



/*
 * System calls
 */


/* Reboot the system */
void k_sc_reboot(void)
{
    k_printf("\nSystem rebooting...\n\n");
    k_reboot();
}


/* Halt the system and enter the debugger */
void k_sc_dbgstop(void)
{
    start_crashdump("System halt");
    end_crashdump();
    k_printf("System halted by domain rop=%x. Press Ctrl-D to reboot.\n",
	     k_st.cur_dcb);
    k_enter_debugger(k_st.ccxs, k_st.cs, DS(k_st.cs), 3);
}


/* Enable activations, reactivate if pending events, else return to caller */
void k_sc_rfa (void)
{
    dcb_ro_t	*rop = k_st.cur_dcb;
    dcb_rw_t	*rwp = rop->dcbrw;

    SCTRACE(k_printf("RFA called\n"));
    rwp->mode = 0; /* enable activations */
    
    if (! EP_FIFO_EMPTY (rwp))
    {
	k_activate (rop, Activation_Reason_Event);
    }
}

/* Enable activations, reactivate if pending events, else restore context */
void k_sc_rfa_resume (void)
{
    dcb_ro_t	*rop = k_st.cur_dcb;
    dcb_rw_t	*rwp = rop->dcbrw;
    context_t	*cp  = k_st.ccxs;
    VP_ContextSlot cx;
   
    SCTRACE(k_printf("RFA resume called\n"));
    cx = cp->gpreg[r_eax];

    rwp->mode = 0; /* enable activations */
  
    if (! EP_FIFO_EMPTY (rwp) || ! (cx < rop->num_ctxts)) {

	k_activate (rop, Activation_Reason_Event);

    } else {

	cp = &(rop->ctxts[cx]);

#if 0
	/* A fp context switch may be needed */
	if (cx != fp_save_cx) {
	    fp_restore_cx=cp;
	    disable_fpu();
	} else {
	    enable_fpu();
	}
#endif /* 0 */
	k_resume(cp, k_st.cs, DS(k_st.cs));
    }
}

/* Enable activations, activate if pending events, else block */
void k_sc_rfa_block (void)
{
    dcb_ro_t	*rop = k_st.cur_dcb;
    dcb_rw_t	*rwp = rop->dcbrw;
    context_t   *cp=k_st.ccxs;
    Time_ns until;

    SCTRACE(k_printf("RFA block called\n"));
    until=cp->gpreg[r_eax];
    *((long *)(&until)+1)=cp->gpreg[r_ebx];

    rwp->mode = 0; /* enable activations */
  
    if (! EP_FIFO_EMPTY (rwp))
    {
	SCTRACE(k_printf("RFA block; calling k_activate"));
	k_activate (rop, Activation_Reason_Event);
    }
    else
    {
	SCTRACE(k_printf("RFA block; preparing rop\n"));
	rop->schst = DCBST_K_BLOCK;
	rop->sdomptr->timeout = until;
	SCTRACE(k_printf("RFA block; calling ksched\n"));
	ksched_scheduler (&k_st, rop, k_st.ccxs);
    }
    SCTRACE(k_printf("NOTREACHED: RFA block finished\n"));
}

/* Wait for an event. */
void k_sc_block (void)
{
    dcb_ro_t	*rop = k_st.cur_dcb;
    dcb_rw_t	*rwp = rop->dcbrw;
    context_t	*cp  = k_st.ccxs;
    Time_ns     until;
    
    SCTRACE(k_printf("BLOCK called\n"));
    until=cp->gpreg[r_eax];
    *((long *)(&until)+1)=cp->gpreg[r_ebx];

    if (! EP_FIFO_EMPTY (rwp))
    {
	return;
    }
    else
    {
	rop->schst = DCBST_K_BLOCK;
	rop->sdomptr->timeout = until;
	ksched_scheduler (&k_st, rop, k_st.ccxs);
    }
}

/* Give up the processor until next resource allocation */
void k_sc_yield (void)
{
    SCTRACE(k_printf("YIELD called\n"));
    k_st.cur_dcb->schst = DCBST_K_YIELD;
    ksched_scheduler (&k_st, k_st.cur_dcb, k_st.ccxs);
}

/* Transmit val over the channel whose TX EP is tx in the current
 * domain. */
uint32_t k_dsc_send(Channel_TX tx, Event_Val val)
{
    dcb_ro_t	*rop	= k_st.cur_dcb;
    ntsc_rc_t	 rc;
    
    SCTRACE(k_printf("Send called\n"));

    if (! (rop->epros && rop->eprws && tx < rop->num_eps))
	rc = rc_ch_invalid;
    else
    {
	ep_ro_t *eprop = DCB_EPRO (rop, tx);
	ep_rw_t *eprwp = DCB_EPRW (rop, tx);
	
	if (eprop->type != Channel_EPType_TX)
	{
	    rc = rc_ch_invalid;
	}
	else if (eprwp->state != Channel_State_Connected) /* XXX right place
							     to look? */
	{
	    rc = rc_ch_bad_state;
	}
	else if (k_send (eprop->peer_dcb, eprop->peer_ep, val, False))
	{
	    eprop->val = val; /* remember the tx'd value; might be
				 handy someday */
	    rc = rc_success;
	}
	else
	    rc = rc_ch_bad_state;
    }
    return rc;
}

static void k_sc_send(void)
{
    context_t	*cp	= k_st.ccxs;
    Channel_TX   tx;
    Event_Val    val;

    tx=cp->gpreg[r_eax];
    val=cp->gpreg[r_ebx];
    
    cp->gpreg[r_eax]=k_dsc_send(tx, val);
}


/*
 * Architecture specific system calls
 */

/* Set interrupt state, return old state */
static void k_psc_swpipl(void)
{
    context_t *cp=k_st.ccxs;
    int rc;
    bool_t newipl;

    newipl=cp->gpreg[r_eax];

    /* paranoia: this syscall has same name as Alpha one. We don't
     * want to get called from Alpha code with newipl not a bool_t. */
    if (newipl!=0 && newipl!=1)
    {
	start_crashdump("psc_swpipl");
	k_printf("k_psc_swpipl() with newipl=%x, shouldn't happen", newipl);
	end_crashdump();
	k_enter_debugger(cp, k_st.cs, DS(k_st.cs), 0);
    }

    rc=(cp->eflag & 0x200)==0x200;
    cp->eflag=(cp->eflag &(~0x200)) | (newipl << 9);
    cp->gpreg[r_eax]=rc;
}

static void k_psc_unmask_irq(void)
{
    context_t *cp=k_st.ccxs;
    int nr;

    nr=cp->gpreg[r_eax];

    if (nr<0 || nr>=N_IRQS || nr==2)
    {
	k_printf("k_psc_unmask_irq(): irq %x is out of range", nr);
	cp->gpreg[r_eax] = -1; /* error */
	return;
    }

    /* XXX ownership check goes here, eventually */

    int_mask &= ~(1<<nr);
    if (nr < IRQ_V_PIC2)
	outb((int_mask & IRQ_M_PIC1), PIC1_OCW1);
    else
	outb((int_mask & IRQ_M_PIC2) >> IRQ_V_PIC2, PIC2_OCW1);

    cp->gpreg[r_eax] = 0; /* success */
}

static void k_psc_kevent(void)
{
    context_t *cp=k_st.ccxs;

    k_event((void *)cp->gpreg[r_eax], cp->gpreg[r_ebx],
	    cp->gpreg[r_ecx]);
}

static void k_psc_activate(void)
{
    context_t *cp=k_st.ccxs;

    DCB_RO2RW((dcb_ro_t *)cp->gpreg[r_eax])->mode=0;
    k_activate((void *)cp->gpreg[r_eax], cp->gpreg[r_ebx]);
    k_panic("k_psc_activate\n");
}

static void k_psc_mask_irq(void)
{
    context_t *cp=k_st.ccxs;
    int nr;

    nr=cp->gpreg[r_eax];

    if (nr<0 || nr>=N_IRQS || nr==2)
    {
	k_printf("k_psc_mask_irq(): irq %x is out of range", nr);
	cp->gpreg[r_eax] = -1; /* error */
	return;
    }

    /* XXX ownership check goes here, eventually */

    int_mask |= (1<<nr);
    if (nr < IRQ_V_PIC2)
	outb((int_mask & IRQ_M_PIC1), PIC1_OCW1);
    else
	outb((int_mask & IRQ_M_PIC2) >> IRQ_V_PIC2, PIC2_OCW1);

    cp->gpreg[r_eax] = 0; /* success */
}

static void k_psc_setdbgreg(void)
{
    context_t *cp = k_st.ccxs;

    k_set_debug_registers((void *)cp->gpreg[r_eax]);
}

void k_psc_entkern(void)
{
    context_t *cp = k_st.ccxs;
    dcb_ro_t *rop = k_st.cur_dcb;

    if (!rop->psmask & PSMASK(PS_KERNEL_PRIV)) {
	start_crashdump("psc_entkern");
	k_printf("ntsc: unprivileged domain tried to enable kernel privs\n");
	end_crashdump();
	k_enter_debugger(cp, k_st.cs, 0, 0);
	/* NOTREACHED */
    }

    rop->ps |= PSMASK(PS_KERNEL_PRIV);
    k_presume(cp, PRIV_CS, PRIV_DS);
}

void k_psc_leavekern(void)
{
    context_t *cp = k_st.ccxs;
    dcb_ro_t *rop = k_st.cur_dcb;

    if (rop->id!=DOM_NEMESIS) {
	rop->ps &= ~PSMASK(PS_KERNEL_PRIV);
	k_presume(cp, USER_CS, USER_DS);
    } else {
	k_presume(cp, PRIV_CS, PRIV_DS);
    }
}    

static void k_sc_putcons(void)
{
    context_t *cp=k_st.ccxs;

    console_putbyte(cp->gpreg[r_eax]);
}

#define PARAM 0x90000
#define EXT_MEM_K (*(unsigned short *) (PARAM+2))
#define CMDLINE_SIGNATURE (*(unsigned short *) (PARAM+0x20))
#define CMDLINE_OFFSET (*(unsigned short *) (PARAM+0x22))

static void k_psc_chain(void)
{
    context_t *cp=k_st.ccxs;
    uint32_t base, len;
    uint32_t *i, *d;
    char *cmdline;

    base=cp->gpreg[r_eax];
    len=cp->gpreg[r_ebx];
    cmdline=(char *)cp->gpreg[r_ecx];

    d=(uint32_t *)0x90200;

    /* Trash the current Nemesis image immediately and start a new one */
    /* Some setup: disable paging */

    k_disable_paging();

    /* Put the memory size in the appropriate place */
    EXT_MEM_K = k_st.memory >> 10;
    CMDLINE_SIGNATURE=0xa33f;
    strcpy(0x90030,cmdline);
    CMDLINE_OFFSET=0x30;

    /* Now we need to copy the relocation and chaining routine to a safe area
       of memory (somewhere above 0x90000), patch in the addresses and
       call it */

    /* Convert len into words */
    len = (len/4) + 1;

    *d++=base;
    *d++=len;

    for (i=&chain_relocator; i<=&chain_relocator_end; i++)
    {
	*d++=*i;
    }

    /* Copy the GDT and its descriptor */
    d=(uint32_t *)0x80000;  /* XXX find somewhere to put this properly */
    for (i=&gdt_desc; i<=&tss_end; i++) {
	*d++=*i;
    }
    *(uint32_t *)0x80002=0x80006;  /* Base address of gdt */

    /* Jump at it */
    __asm__ __volatile__ ("jmp *%0" : : "r" (0x90208) );
    k_printf("Bogosity: notreached in chain()\n");
}

static void k_psc_evsel(void) 
{
    context_t *cp = k_st.ccxs;
    uint32_t   ev0, ev1; 
    word_t     mode; 

    /* If we don't have a PPro or above, spit an error */
    if (INFO_PAGE.processor.family < 6) {
	k_printf("ntsc_selpmctr called but processor not PPro or better.\n");
	cp->gpreg[r_eax] = -1; /* success */
	return; 
    }

    ev0  = (uint32_t)cp->gpreg[r_eax];
    ev1  = (uint32_t)cp->gpreg[r_ebx];
    mode = (word_t)cp->gpreg[r_ecx];   

    if(mode & 2) {
	/* This is evsel1  */
	__asm__ __volatile__ (
	    "movl $0x187, %%ecx; movl %0, %%eax; wrmsr;" 
	    :                     /* no outputs */
	    : "a" (ev1)           /* inputs */
	    : "eax", "ecx"        /* clobbers */);
    }

    if(mode & 1) { 
	/* This is evsel0: or in 'EN' at top.  */
	ev0 |= 0x400000; 
	__asm__ __volatile__ (
	    "movl $0x186, %%ecx; movl %0, %%eax; wrmsr;" 
	    :                     /* no outputs */
	    : "a" (ev0)           /* inputs */
	    : "eax", "ecx"        /* clobbers */);
    } 

    
    cp->gpreg[r_eax] = 0; /* success */
    return;
}

static void k_sc_bad(void)
{
    context_t *cp=k_st.ccxs;

    start_crashdump("bad syscall");
    k_printf("\nUnknown unpriv system call called\n");
    end_crashdump();
    k_enter_debugger(cp, k_st.cs, 0, 0);
}



static void k_psc_bad(void)
{
    context_t *cp=k_st.ccxs;

    start_crashdump("bad privileged syscall");
    k_printf("\nUnknown priv system call called\n");
    end_crashdump();
    k_enter_debugger(cp, k_st.cs, 0, 0);
}


