/*
 *      ntsc/ix86/kernel.c
 *      ------------------
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * This file contains most of the NTSC.
 *
 * $Id: kernel.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 */

#include <autoconf/memsys.h>
#include <autoconf/timer_rdtsc.h>
#include <nemesis.h>
#include <context.h>
#include <kernel.h>
#include <nexus.h>
#include <pip.h>
#include <Timer.ih>
#include <irq.h>
#include <platform.h>

#include <Stretch.ih>
#include <ProtectionDomain.ih>

#include "i386-stub.h"
#include "asm.h"
#include "segment.h"
#include "inline.h"
#include "../../generic/string.h"
#include "../../generic/console.h"
#include "../../generic/crash.h"

#include <dcb.h>
#include <io.h>
#include <mmu_tgt.h>

#include <multiboot.h>

#define PARANOIA 0

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef ACT_TRACE
#define ATRC(_x) _x
#else
#define ATRC(_x)
#endif

#define SCTRACE(_x)

/* The kernel state is a nice global variable */
kernel_st k_st;

/* Context slots */
context_t  bad_cx;               /* Context used when no valid slot in DCB */

static callpriv_desc callpriv_dispatch[N_CALLPRIVS];


void k_enter_debugger(context_t *, uint32_t, uint32_t, uint32_t);
    
/*
 * Internal functions
 */

/* Activate or resume a domain */
void k_activate(dcb_ro_t *rop, uint32_t reason) 
{
    word_t *stack;
    dcb_rw_t *rwp = DCB_RO2RW (rop);
    uint32_t cs;
    context_t *cx;
    
#if PARANOIA
    if (rop != k_st.cur_dcb) 
	k_printf("activate: bogosity\n");
#endif

    INFO_PAGE.pvsptr = &rwp->pvs;

    ATRC(k_printf("|"));

    /* We always resume in the context slot indicated by resctx, even
       if activations are on. Think about it. */
    cx = &(rop->ctxts[rwp->resctx&rop->ctxts_mask]);

    /* Read cycle counter if we have one */
    rop->pcc = ((INFO_PAGE.processor.features & PROCESSOR_FEATURE_TSC)?
		rpcc() : 0);

    cs = (rop->ps & PSMASK(PS_KERNEL_PRIV)) ? PRIV_CS : USER_CS;

    /* XXX this is a hack, because with alignment checks enabled doubles
       and long longs must be aligned to 8 bytes. This would break
       lots of things */
    cx->eflag &= ~EFLAG_AC; /* Disable alignment checks */
    /* XXX all domains are now fp domains. */


    if (rwp->mode == 0)
    {
	rwp->mode = 1; /* disable activations */
	
	cx->eip = (uint32_t) rwp->activ_op;

	/* Platform-specific magic to pass the activation code its DCB
	   address and activation reason. This code sets up a C stack
	   frame with a faked return address */
	stack      = (word_t *)((uint32_t)rwp + DCBRW_W_STKSIZE);
	*(--stack) = reason;
	*(--stack) = (uint32_t) &(rop->vp_cl);
	*(--stack) = (uint32_t) rwp->activ_clp;
	*(--stack) = 0; /* Fake return address */
	
	cx->esp = (uint32_t)stack;
	
	/* We should totally reset the flags before playing with them */
	cx->eflag = 0x200; /* Always activate with interrupts enabled */
	if (rop->ps & PSMASK(PS_IO_PRIV))
	    cx->eflag |= 0x3000; /* Set IOPL 3 for privileged tasks */
	else
	    cx->eflag &= ~0x3000; /* Otherwise IOPL 0 */

#if PARANOIA
	if (rop->ps&PSMASK(PS_KERNEL_PRIV) && rop->id!=0xd000000000000000LL) {
	    k_printf("ntsc: Oops: about to activate %x in kernel mode\n",
		     rop->id & 0xffffffff);
	}	
#endif /* PARANOIA */

	k_activ_go(cx, cs, DS(cs));
	k_panic("k_activate(activate): NOTREACHED\n");
    }
    /* XXX BEFORE we do this we should sanity check the flags; it will be
       possible for domains to gain privileges if they are able to fiddle
       with the flag bits */

    k_resume(cx, cs, DS(cs));

    k_panic ("k_activate(resume): NOTREACHED\n");
}


/* ------------------------- IRQ dispatching -------------------------- */


/* For speed, keep an in-ram copy of the interrupt mask register */
/* bits 0-7 are for irqs 0-7, 8-15 for irq 8-15. */
int_mask_reg_t int_mask = 0xfffb; /* initally all but cascade masked */

extern bool_t k_event(dcb_ro_t *rop, Channel_RX rx, Event_Val inc);

/* Static kinfo structure passed into the stubs */
static const kinfo_t stub_kinfo = {
    &k_event,
    &int_mask,
    &(k_st.reschedule)
};


/* default IRQ handler  */
static void k_resvd_irq(void *state, const kinfo_t *kinfo)
{
    /* This shouldn't be called: the interrupt should be handled in
     * the switch earlier */
    k_panic("k_resvd_irq: kernel reserved irq called through stub!\n");
}


static irq_desc irq_dispatch[N_IRQS] = {
    /* set some defaults up */
    {NULL, BADPTR},		/*  0  - timer, reserved, special stub */
    {NULL, BADPTR},		/*  1 */
    {k_resvd_irq, BADPTR},	/*  2  - cascade reserved */
    {NULL, BADPTR},		/*  3 */
    {NULL, BADPTR},		/*  4 */
    {NULL, BADPTR},		/*  5 */
    {NULL, BADPTR},		/*  6 */
    {NULL, BADPTR},		/*  7 */
    {NULL, BADPTR},	        /*  8 */
    {NULL, BADPTR},		/*  9 */
    {NULL, BADPTR},		/* 10 */
    {NULL, BADPTR},		/* 11 */
    {NULL, BADPTR},		/* 12 */
    {NULL, BADPTR},		/* 13 */
    {NULL, BADPTR},		/* 14 */
    {NULL, BADPTR}		/* 15 */
};

void k_irq(uint32_t nr)
{
    context_t *cx;
    uint32_t cs;

    cx=k_st.ccxs;
    cs=k_st.cs;

    INFO_PAGE.iheartbeat++;

#if PARANOIA
    if (k_st.cur_dcb && cs == PRIV_CS && k_st.cur_dcb->dcbrw->mode == 0 &&
	(k_st.cur_dcb->id & 0xffffffff)) {
	start_crashdump("k_irq");
	k_printf("k_irq: call %x from priv dom with activ on\n",nr);
	end_crashdump();
	k_enter_debugger(cx, cs, 0, 0);
    }

    /* XXX Are we sane? */
    if (cs == 0) {
	start_crashdump("k_irq");
	k_printf("k_irq: irq in domain with bad context slot\n");
	end_crashdump();
	k_enter_debugger(cx, cs, 0 /* ss */, 0 /* exception */);
    }
#endif /* PARANOIA */

    k_st.reschedule = False;
    
    if (nr < N_IRQS)
    {
	/* mask the interrupt in the PIC's IMR if this isn't the ticker */
	int_mask |= (1<<nr);
	if (nr < IRQ_V_PIC2)
	    outb((int_mask & IRQ_M_PIC1), PIC1_OCW1);
	else
	    outb((int_mask & IRQ_M_PIC2) >> IRQ_V_PIC2, PIC2_OCW1);

	/* Acknowledge the interrupt, but leave it masked in the
	 * IMR. This should clear the bit in the ISR */
	ack_irq(nr);
	
	/* run the stub, if there is one */
	if (irq_dispatch[nr].stub)
	    irq_dispatch[nr].stub(irq_dispatch[nr].state, &stub_kinfo);
	else {
#if PARANOIA
	    k_printf("k_irq: got int %x, but no stub present\n", nr);
#endif /* PARANOIA */
	}
	    
	if (k_st.reschedule) {
	    ksched_scheduler(&k_st, k_st.cur_dcb, cx);
	    k_panic("k_int: NOTREACHED after ksched_scheduler()\n");
	}
	
	if (cs == KERNEL_CS)
	    k_idle();
	k_presume(cx, cs, DS(cs));
	k_panic("k_int: NOTREACHED after k_presume()\n");
    }

    k_panic("NOTREACHED at end of k_irq() (nr out of range)\n");
}

/* handler for invalid callpriv calls */
void k_bad_callpriv()
{
    context_t *cp = k_st.ccxs;
    
    int vec = cp->gpreg[r_eax];

    start_crashdump("bad_callpriv");

    if(vec >= N_CALLPRIVS) {
	k_printf("\nntsc: callpriv vector number %x out of range\n", vec);
    } else {
	k_printf("\nntsc: callpriv vector number %x not in use\n", vec);
    }
    end_crashdump();

    k_enter_debugger(cp, k_st.cs, 0, 0);
    /* NOTREACHED */
}


static char *exc_names[] = {
    "Division by zero",
    "Single step exception",
    "NMI",
    "Breakpoint",
    "Integer overflow exception into",
    "Bound check error",
    "Invalid Instruction",
    "Coprocessor not available",
    "Double fault exception",
    "CoProcessor segment overrun (should never happen)",
    "Invalid TSS",
    "Segment not present",
    "Invalid stack access",
    "General protection fault",
    "Page fault",
    "int 15",
    "Floating point error",
    "Unaligned data access",
    "P5 Machine check exception",
}; 

static char *cs_vals[] = {
    "User mode",
    "Privileged mode",
    "Kernel mode",
    "Bad context slot",
    "Unknown CS"
};

char *cs_text(uint32_t cs)
{
    switch(cs) {
    case USER_CS: 
	return(cs_vals[0]);

    case PRIV_CS: 
	return(cs_vals[1]);

    case KERNEL_CS: 
	return(cs_vals[2]);

    case 0: 
	return(cs_vals[3]);
    }
    
    return(cs_vals[4]);
}



void k_exception (int nr, int error)
{
    context_t *cx = k_st.ccxs;

    start_crashdump("exception");
    k_printf("\n\n=== %s (%s)\n",exc_names[nr], cs_text(k_st.cs));
    k_printf("eax=%p  ebx=%p  ecx=%p  edx=%p\nesi=%p  edi=%p  ebp=%p "
	     "esp=%p\n",
	     cx->gpreg[r_eax], cx->gpreg[r_ebx], cx->gpreg[r_ecx],
	     cx->gpreg[r_edx], cx->gpreg[r_esi], cx->gpreg[r_edi],
	     cx->gpreg[r_ebp], cx->esp);
    k_printf("error=%p  eip=%p  flags=%p\n",
	     error, cx->eip, cx->eflag);

    /* if the exn was from user mode, then the k_st will still be
     * valid. Otherwise, all bets are off. */
    if (k_st.cs == USER_CS)
	k_printf("DCBRO=%x  DCBRW=%x  domain=%x\n",k_st.cur_dcb,
		 k_st.cur_dcb->dcbrw, (uint32_t)(k_st.cur_dcb->id&0xffffffff));
    end_crashdump();
    
    k_enter_debugger(cx, k_st.cs, 0, error);
}


void k_enter_debugger(context_t *cx, uint32_t cs, uint32_t ss,
			   uint32_t exception)
{
    /* Fill debugger state */
    registers[EAX] = cx->gpreg[r_eax];
    registers[EBX] = cx->gpreg[r_ebx];
    registers[ECX] = cx->gpreg[r_ecx];
    registers[EDX] = cx->gpreg[r_edx];
    registers[ESP] = cx->esp;
    registers[EBP] = cx->gpreg[r_ebp];
    registers[ESI] = cx->gpreg[r_esi];
    registers[EDI] = cx->gpreg[r_edi];
    registers[PC]  = cx->eip;
    registers[PS]  = cx->eflag;
    registers[CS]  = cs;
    registers[SS]  = ss;
    registers[DS]  = fetch_ds();
    registers[ES]  = fetch_es();
    registers[FS]  = fetch_fs();
    registers[GS]  = fetch_gs();

    handle_exception(exception);
}





/* --------------------- Initialisation / Termination Routines ------------ */

extern void init_serial();
extern void init_console();
extern void init_debug();
extern void init_mem(struct nexus_primal *, word_t);

/* Initialise the interrupt controllers */
static void k_init_pic(void)
{
    outb(0x11,0x20); /* Init PIC1 */
    outb(0x11,0xa0); /* Init PIC2 */
    outb(0x20,0x21); /* PIC1 first IRQ 0x20 */
    outb(0x28,0xa1); /* PIC2 first IRQ 0x28 */
    outb(0x04,0x21); /* PIC1 is master */
    outb(0x02,0xa1); /* PIC2 is slave */
    outb(0x01,0x21); /* 8086 mode */
    outb(0x01,0xa1);
    outb(0xff,0xa1); /* Mask all */
    outb(0xfb,0x21); /* Mask all except cascade */
}
    
/* This function is called as soon as it is possible to run C code */
void k_init(uint32_t multiboot_magic, struct multiboot_info *mb_info)
{
    extern void init_cache(void);
    extern int callpriv_unused_vector(void *, dcb_ro_t *, void *, 
				      void *, void *);
#ifdef CONFIG_TIMER_RDTSC
    extern Timer_clp Timer_rdtsc_init(void);
#endif
    extern Timer_clp Timer_init(void);

    /* Use bad_cx allocated earlier for primal context */
    unsigned int memory_size;
    struct nexus_primal *primal;
    extern const char *k_ident;
    processor_t *p;
    int i;

    k_init_pic();
    /* Default options for output, before we have a chance to parse the
       command line */
    k_st.crash.local=True;
    k_st.crash.serial=True;
    k_st.serial.baseaddr=0x3f8; /* First serial port */
    k_st.local_vga.baseaddr=0xb8000;
    k_st.local_vga.width=80;
    k_st.local_vga.height=25;
    console_init();
    init_serial();
    init_debug();

    /* Keep things as much like the old world as possible for now.
     * We may change the defaults later on. */
    {
	k_st.crash.serial=True;
	k_st.console.buffer_output=False;
	k_st.console.local_output=False;
	k_st.console.serial_output=True;
    }

    get_nexus_ident();
    k_printf("Nemesis: The Kernel  %s\n", k_ident);
    TRC(k_printf("Out out!! You demons of stupidity!!\n\n"));
#define M(b) ((b)<<20)
#define K(b) ((b)<<10)
#define PARAM 0x90000
#define EXT_MEM_K (*(unsigned short *) (PARAM+2))
#define CMDLINE_SIGNATURE (*(unsigned short *) (PARAM+0x20))
#define CMDLINE_OFFSET (*(unsigned short *) (PARAM+0x22))

    if (multiboot_magic == MULTIBOOT_LOADER_MAGIC) {
	TRC(k_printf("ntsc: multiboot information present\n"));

	if (!(mb_info->flags & MULTIBOOT_FLAG_MEMORY)) {
	    k_panic("ntsc: no memory size specified in multiboot info\n");
	}

	memory_size = K(mb_info->mem_upper);
	TRC(k_printf("Upper memory size: %x\n", memory_size));

	/* Skip boot device... */

	if (mb_info->flags & MULTIBOOT_FLAG_CMDLINE) {
	    k_st.command_line = mb_info->cmdline;
	} else {
	    k_st.command_line = NULL;
	}

    } else {
	TRC(k_printf("ntsc: Linux-style boot\n"));

	memory_size = K((EXT_MEM_K));
	
	/* Save a pointer to the kernel command line */
	k_st.command_line = NULL;
	if (CMDLINE_SIGNATURE == 0xa33f) {
	    k_st.command_line = (char *)(PARAM+CMDLINE_OFFSET);
	}
	
	/* See if memory size is set explicitly */
	if (k_st.command_line) {
	    char *c;
	    
	    for (c=k_st.command_line; *c; c++) {
		if (*c == ' ' && *(const unsigned long *)(c+1) ==
		    *(const unsigned long *)"mem=") {
		    memory_size = simple_strtoul(c+5, &c, 0);
		    if (*c == 'K' || *c == 'k') {
			memory_size = K(memory_size);
		    } else if (*c == 'M' || *c == 'm') {
			memory_size = M(memory_size);
		    }
		    k_printf("Memory size explicitly set to %x\n",memory_size);
		}
	    }
	}
    }

    if (k_st.command_line) {
	char *c;
	TRC(k_printf("Cmdline: %s\n",k_st.command_line));

	for (c=k_st.command_line; *c; c++) {
	    if (*c == ' ' && *(const unsigned long *)(c+1) ==
		*(const unsigned long *)"cras" &&
		*(const unsigned short *)(c+5) ==
		*(const unsigned short *)"h=") {
		char *o=c+7;
		k_printf("Crash command line option found\n");
		k_st.crash.local=False;
		k_st.crash.serial=False;

		while (*o && *o!=' ') {
		    if (*o==',') {
			o++;
			continue;
		    }
		    if (memcmp(o,"local",5)==0) {
			k_printf("Crashdump to local display\n");
			k_st.crash.local=True;
			o+=5;
			continue;
		    }
		    if (memcmp(o,"serial0",7)==0) {
			k_printf("Crashdump to serial0\n");
			k_st.crash.serial=True;
			k_st.serial.baseaddr=0x3f8;
			o+=7;
			continue;
		    }
		    if (memcmp(o,"serial1",7)==0) {
			k_printf("Crashdump to serial1\n");
			k_st.crash.serial=True;
			k_st.serial.baseaddr=0x2f8;
			o+=7;
			continue;
		    }
		}
		if (k_st.crash.serial) init_serial();
	    }
	}
	for (c=k_st.command_line; *c; c++) {
	    if (*c == ' ' && *(const unsigned long *)(c+1) ==
		*(const unsigned long *)"trac" &&
		*(const unsigned char *)(c+5) ==
		*(const unsigned char *)"e") {
		k_st.crash.serial=True;
		k_st.console.buffer_output=False;
		k_st.console.local_output=False;
		k_st.console.serial_output=True;
		k_printf("\nTrace command line option found: console"
			 "trace to serial active\n");
	    }
	}
    }
    
    /* Save this for later (XXX SMH: only used by ntsc_chain currently) */
    k_st.memory = memory_size;
    
    /* setup the kernel's status struct */
    k_st.irq_dispatch = irq_dispatch;
    k_st.cptable      = callpriv_dispatch;

    for (i=0; i<N_CALLPRIVS; i++) {
	callpriv_dispatch[i].cpfun = callpriv_unused_vector;
	callpriv_dispatch[i].state = (void *)i;
    }
#ifdef CONFIG_NTSC_SCHED_ACCOUNT
    k_st.account = NULL;
#endif
    /* XXX we don't really want to export this
       to the world */

    p=&(INFO_PAGE.processor);

    if (processor_cpuid_available()) {
	uint32_t out[4];
	uint32_t max_cpuid;

	/* Find out how much CPUID information is available */
	read_cpuid(0, out);

	max_cpuid=out[0];
	*(uint32_t *)&p->vendor[0]=out[1];
	*(uint32_t *)&p->vendor[4]=out[3];
	*(uint32_t *)&p->vendor[8]=out[2];
	p->vendor[12]=0;

	if (max_cpuid>=1) {
	    read_cpuid(1, out);
	    p->stepping=out[0]&0xf;
	    p->model=(out[0]>>4)&0xf;
	    p->family=(out[0]>>8)&0xf;
	    p->features=out[3];
	} else {
	    p->stepping=0;
	    p->model=0;
	    p->family=0;
	    p->features=0;
	}

	if (max_cpuid>=2) {
	    read_cpuid(2, out);
	    /* We don't store this information yet */
	}

	switch (p->family) {
	case 4: p->type=processor_type_486; break;
	case 5: p->type=processor_type_pentium; break;
	case 6: p->type=processor_type_ppro; break;
	default: p->type=processor_type_unknown; break;
	}
    } else {
	p->type=processor_type_unknown;
	strcpy(p->vendor,"Unknown");
	p->stepping=0;
	p->model=0;
	p->family=4;
	p->features=0;
    }

    /* Tell them what they probably already know */
#ifdef DEBUG
    {
	char *processors[]={"Unknown","i486","Pentium","Pentium Pro"};
	char *features[]={"fpu","vme","de","pse","tsc","msr","pae",
			  "mce","cx8","apic","10","11","mtrr","pge",
			  "mca","cmov","16","17","18","19","20","21",
			  "22","23","24","25","26","27","28","29",
			  "30","31"};
	char *numbers[]={"0","1","2","3","4","5","6","7",
			 "8","9","10","11","12","13","14","15"};
	uint32_t i;
	k_printf("Memory size:        %x\n",memory_size);
	k_printf("Processor type:     %s\n",processors[p->type]);
	k_printf(" +        family:   %s\n",numbers[p->family]);
	k_printf(" +        model:    %s\n",numbers[p->model]);
	k_printf(" +        stepping: %s\n",numbers[p->stepping]);
	k_printf(" +        vendor:   %s\n",p->vendor);
	k_printf(" +        features:");
	if (p->features) {
	    for (i=0; i<32; i++)
		if (p->features & (1<<i)) k_printf(" %s",features[i]);
	} else
	    k_printf(" none");
	k_printf("\n\n");
    }
#endif /* DEBUG */

    /* If we have 4Mb pages available then enable them */
    if (INFO_PAGE.processor.features & PROCESSOR_FEATURE_PSE) {
	TRC(k_printf("Enabling page size extension\n"));
	set_cr4_bit(CR4_PSE);
    }

    /* If we can enable PGE, then do */
    if (INFO_PAGE.processor.features & PROCESSOR_FEATURE_PGE) {
	TRC(k_printf("Enabling global pages\n"));
	set_cr4_bit(CR4_PGE);
    }

    /* If we have a 486 or above enable alignment checking */
    if (INFO_PAGE.processor.family >= 4) {
	TRC(k_printf("Enabling alignment checking\n"));
	set_cr0_bit(CR0_AM);
    }

    /* If we have a Pentium or above enable the cache */
    if (INFO_PAGE.processor.family >= 5) {
	TRC(k_printf("Enabling cache\n"));
	init_cache();
    }

    /* If we have a PPro or above, enable user-level reading of PMCTRs */
    if (INFO_PAGE.processor.family >= 6) {
	TRC(k_printf("Enabling performance counters\n"));
	init_pmctr();         /* setup to read DCstall cycles + inst retired */
	set_cr4_bit(CR4_PCE); /* enable read from user land */
    }


    /* Clear out the information page */
    INFO_PAGE.pvsptr     = NULL;
    INFO_PAGE.sheartbeat = 0;      /* scheduler passes */
    INFO_PAGE.iheartbeat = 0;      /* irqs             */
    INFO_PAGE.nheartbeat = 0;      /* ntsc calls       */
    INFO_PAGE.pheartbeat = 0;      /* prot faults      */

#ifdef CONFIG_TIMER_RDTSC
    if (INFO_PAGE.processor.features & PROCESSOR_FEATURE_TSC) {
	k_st.t = (Timer_clp)Timer_rdtsc_init();
    }
    else
    {
	k_st.t = (Timer_clp)Timer_init();
    }
#else
    k_st.t = (Timer_clp)Timer_init();
#endif

    Timer$Enable(k_st.t, 0);


    primal = get_primal_info();

    /* Setup the memory map information */
    init_mem(primal, memory_size);

    /* Enable the fpu */
    enable_fpu();

    /* Set up registers for NemesisPrimal program */
    bad_cx.esp = 0x80000; /* NemesisPrimal must fiddle the stack once
			       it has created a DCB for its own use */
    bad_cx.eip = primal->entry;
    bad_cx.gpreg[r_eax] = (uint32_t)primal;
    bad_cx.gpreg[r_ebx] = (int) (&k_st); /* XXX- urgh */
    bad_cx.gpreg[r_ecx] = (int) k_ident;
    bad_cx.eflag = 0x03002; /* Flag settings for NemesisPrimal:
				 Interrupts _disabled_,
				 IOPL=3 (program can use IO ports) */

    TRC(k_printf("Primal(%x,%x,%x,%x)\n\n", bad_cx.gpreg[r_eax], 
		 bad_cx.gpreg[r_ebx], bad_cx.gpreg[r_ecx],
		 bad_cx.gpreg[r_edx]));

    k_presume(&bad_cx, PRIV_CS, PRIV_DS);
    k_panic("k_init: NOTREACHED\n");
}


void k_halt(void)
{
    breakpoint();
    re_enter_rom();
}

void k_reboot(void)
{
    int i,j;

    *(uint8_t *)0 = 7;
    *((unsigned short *)0x472) = 0x1234;
    for (;;) {
	for (i=0; i<100; i++) {
	    for (j=0; j<0x10000; j++)
		if ((inb_p(0x64) & 0x02) == 0)
		    break;
	    for(j = 0; j < 100000 ; j++)
		/* nothing */;
	    outb(0xfe,0x64);	 /* pulse reset low */
	    for(j = 0; j < 10000000 ; j++)
		/* nothing */;
	}
	re_enter_rom();
    }
}


