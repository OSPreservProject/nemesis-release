/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      sys/nemesis
** 
** FUNCTIONAL DESCRIPTION:
** 
**      The Nemesis domain is responsible for allocating basic kernel
**      provided resources.  It communicates with the kernel via the
**      shared kernel state and privileged NTSC calls.
**    
**      A number of system services execute as servers within this domain,
**      including DomainMgr, Binder, the memory system, Interrupt,
**      CallPriv, Security, Console
** 
** ENVIRONMENT: 
**
**	Activated at the end of NemesisPrimal which has already created
**      most of the basic system services.  All we have to do now is 
** 	IDC_EXPORT them!
**
**      ID :     $Id: Nemesis.c 1.2 Thu, 18 Feb 1999 14:18:08 +0000 dr10009 $
**
*/

#include <autoconf/callpriv.h>
#include <autoconf/memsys.h>
#include <autoconf/qosctl.h>
#include <autoconf/start_of_day_tracing.h>
#include <autoconf/trader_outside_kernel.h>
#include <autoconf/rc_outside_kernel.h>
#include <autoconf/module_offset_data.h>
#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <ntsc.h>
#include <domid.h>
#include <salloc.h>
#include <stdio.h>
#include <time.h>
#include <ecs.h>

#include <IDCMacros.h>
#include <VPMacros.h>

#include <Activation.ih>
#include <BootDomain.ih>
#include <Builder.ih>
#include <ThreadsPackage.ih>
#include <VP.ih>
#include <TypeSystemF.ih>
#include <NTSC.ih>

#include "Console.h"
#include "klogd.h"
#include "misc.h"

#include <ConsoleWr.ih>

#ifndef CONFIG_RC_OUTSIDE_KERNEL
#include <RdWrMod.ih>
#include <ClangerMod.ih>
#include <Clanger.ih>
#include <GatekeeperMod.ih>
#endif

#include <Security.ih>
#include <DomainMgrMod.ih>
#ifdef CONFIG_CALLPRIV
#include <CallPriv.ih>
#endif
#ifdef CONFIG_MODULE_OFFSET_DATA
#include <ModData.ih>
#endif
#ifdef AXP3000
#include <TCOpt.ih>
#include <platform.h>
#include <irq.h>
#include <tc.h>
static const TCOpt_Slot slots[]= {
    { (addr_t)(IO_TCOPT0 | IO_SPARSE), 0, IR_M_OPTION0, IRQ_V_TC_OPT0 }, 
    { (addr_t)(IO_TCOPT1 | IO_SPARSE), 1, IR_M_OPTION1, IRQ_V_TC_OPT1 }, 
    { (addr_t)(IO_TCOPT2 | IO_SPARSE), 2, IR_M_OPTION2, IRQ_V_TC_OPT2 }, 
    { (addr_t)(IO_TCOPT3 | IO_SPARSE), 3, IR_M_OPTION3, IRQ_V_TC_OPT3 }, 
    { (addr_t)(IO_TCOPT4 | IO_SPARSE), 4, IR_M_OPTION4, IRQ_V_TC_OPT4 }, 
    { (addr_t)(IO_TCOPT5 | IO_SPARSE), 5, IR_M_OPTION5, IRQ_V_TC_OPT5 },
};
#endif

#if defined(CONFIG_MEMSYS_STD) || defined(CONFIG_MEMSYS_EXPT)
#include <MMEntryMod.ih>
#include <MMNotifyMod.ih>
#endif

#ifdef CONFIG_MEMSYS_EXPT
#include <SDriverMod.ih>
#include <StretchTbl.ih>
#include <frames.h>         /* pull in flist_t */
#endif

#ifdef CONFIG_QOSCTL
#include <QoSCtl.ih>
extern bool_t sched_ctl_init(void);
#endif

#ifdef CONFIG_START_OF_DAY_TRACING_SUBHEADINGS
#define DEBUG
#endif

#ifdef DEBUG
#define TRC(_x) _x
#define DB(_x) _x
#else
#define TRC(_x)
#define DB(_x)
#endif /* DEBUG */



/* This is the state that the Nemesis domain shares with the kernel */
#include <kernel_st.h>		

/* Useful macros */
#define K(k) ((k)<<10)
#define S(s) SECONDS(s)
#define M(m) MILLISECS(m)
#define U(u) MICROSECS(u)


#define DUMP_PVS()							\
    printf("\nDumping pervasives: \n");				\
    if(Pvs(vp)) printf("   vp = %p\n",Pvs(vp)); 			\
    if(Pvs(heap)) printf(" heap = %p\n",Pvs(heap)); 			\
    if(Pvs(types)) printf("types = %p\n",Pvs(types)); 			\
    if(Pvs(root)) printf(" root = %p\n",Pvs(root)); 			\
    if(Pvs(exns)) printf(" exns = %p\n",Pvs(exns)); 			\
    if(Pvs(time)) printf(" time = %p\n",Pvs(time)); 			\
    if(Pvs(evs)) printf("  evs = %p\n",Pvs(evs)); 			\
    if(Pvs(thd)) printf("  thd = %p\n",Pvs(thd)); 			\
    if(Pvs(thds)) printf(" thds = %p\n",Pvs(thds)); 			\
    if(Pvs(srcth)) printf("srcth = %p\n",Pvs(srcth)); 			\
    if(Pvs(in)) printf("   in = %p\n",Pvs(in));   			\
    if(Pvs(out)) printf("  out = %p\n",Pvs(out)); 			\
    if(Pvs(err)) printf("  err = %p\n",Pvs(err)); 			\
    if(Pvs(console)) printf(" cons = %p\n",Pvs(console));  		\
    if(Pvs(bndr)) printf(" bndr = %p\n",Pvs(bndr)); 			\
    if(Pvs(objs)) printf(" objs = %p\n",Pvs(objs)); 			\
    if(Pvs(gkpr)) printf(" gkpr = %p\n",Pvs(gkpr));			\
    if(Pvs(libc)) printf(" libc = %p\n",Pvs(libc));			\
    printf("End of pervasives dump: non-listed means NULL...\n");


static Activation_Go_fn Go;
static Activation_op np_op = { Go };
static const Activation_cl np_cl = { &np_op, NULL };

CL_EXPORT(Activation, Nemesis, np_cl);

static Closure_Apply_fn SystemDomain;
static Closure_op sys_op = { SystemDomain };
static const Closure_cl sys_cl = { &sys_op, NULL };

static void NemesisMainThread();
extern Wr_cl triv_wr_cl;

/*
** The Nemesis domain is special - it gets passed 
** only half a comment
** and its Pvs are special.
*/
void Go(Activation_cl *self, 
	VP_clp vp /* IN */,
	Activation_Reason ar /* IN */ ) 
{
    kernel_st      *kst = (kernel_st *)self->st;
    dcb_rw_t       *rwp = vp->st;
    dcb_ro_t       *rop = DCB_RW2RO(rwp);
    ActivationF_cl *actf;

    TRY {

#ifdef __IX86__
	ntsc_entkern(); /* There is an NTSC hack that prevents this
			   from being undone. */
#endif /* __IX86__ */

	/* Direct all output from Nemesis domain through NTSC (or,
	   on Alpha, direct to the hardware) */
	Pvs(err)=Pvs(out)=Pvs(console)=&triv_wr_cl;

	TRC(printf("Nemesis domain entered.\n")); 
	TRC(printf(" + DCBRO at %p\n", rop));
	TRC(printf("      psmask=%qx\n", rop->psmask));
#ifdef __ALPHA__
	TRC(printf("      ps=%qx\n", ntsc_rdps()));
#endif
	TRC(printf(" + DCBRW at %p\n", rwp));
	TRC(printf(" + Activation clp at %p\n", self));
	TRC(printf(" + VP clp at %p\n", vp));
	TRC(printf(" + activationsOn=%d\n", VP$ActivationMode(vp)));
	TRC(printf(" + Activation Reason %d\n", ar));
	TRC(printf(" + PVS        = %p\n", PVS()));
	
	TRC(DUMP_PVS());
	
	{
	    StretchAllocator_clp salloc;
	    StretchAllocator_SizeSeq *sizes;
	    StretchAllocator_StretchSeq *stretches;
	    ThreadsPackage_Stack protoStack;
	    Stretch_clp userStretch;
	    BootDomain_Info *nem_info;
	    ThreadsPackage_clp tp;
	    ThreadF_clp thdf;
#ifdef CONFIG_MEMSYS_EXPT
	    SDriverMod_cl *sdmod;
	    StretchTbl_cl *strtab;
#define MAX_DESCS 5
	    Mem_PMemDesc pmem[MAX_DESCS + 1];
	    Type_Any fany; 
	    flink_t *lk;
	    flist_t *cur; 
	    int i = 0; 

	    /* Create a new stretch driver (physical one for now) */
	    TRC(printf("Creating initial stretch driver and stretch tab.\n"));
	    sdmod  = NAME_FIND("modules>SDriverMod", SDriverMod_clp);
	    strtab = NAME_FIND("sys>StretchTable", StretchTbl_clp);

	    /* Grab the pmem which has been allocated for us by dmgr */
	    for(lk = rop->finfo.next; lk != &rop->finfo; lk = lk->next) {
		cur = (flist_t *)lk; 
		pmem[i].start_addr  = cur->base; 
		pmem[i].frame_width = cur->fwidth; 
		pmem[i].nframes     = 
		    cur->npf >> (cur->fwidth - FRAME_WIDTH); 
		pmem[i].attr        = 0;
		if(++i == MAX_DESCS) 
		    break;
	    }
	    pmem[i].nframes = 0; /* terminate array */

	    ANY_INIT(&fany, Frames_clp, rop->frames);
	    Pvs(sdriver) = SDriverMod$NewPhysical(sdmod, vp, Pvs(heap), 
						  strtab, pmem, &fany);
#endif

	    /* Add our personal frames closure into the sys context */
	    /* XXX SDE: I'm not convinced that this is a good idea; the
	       Nemesis domain (and all other domains) should have a bit of
	       private namespace for this. I was bitten once by a domain
	       using the Nemesis domain's Frames closure directly because
	       its own hadn't overridden it in the namespace. */
	    CX_ADD("sys>Frames", rop->frames, Frames_clp);
	    
	    TRC(eprintf (" + Finding Nemesis StretchAllocator\n"));
	    salloc = NAME_FIND("sys>StretchAllocator", StretchAllocator_clp);
	    TRC(eprintf (" + StretchAlloc = %p\n", salloc));
	    
	    TRC(eprintf (" + Finding parameters\n"));
	    nem_info= NAME_FIND("progs>Nemesis", BootDomain_InfoP);

	    sizes = SEQ_NEW(StretchAllocator_SizeSeq, 3, Pvs(heap));
	    SEQ_ELEM(sizes, 0) = FRAME_SIZE; /* for guard page */
	    SEQ_ELEM(sizes, 1) = nem_info->stackWords*sizeof(word_t);
	    SEQ_ELEM(sizes, 2) = nem_info->pHeapWords*sizeof(word_t);
	    
	    TRC(eprintf (" + Allocating Stretches\n"));
	    stretches = STR_NEWLIST_SALLOC(salloc, sizes);
	    
	    protoStack.guard   = SEQ_ELEM(stretches, 0);
	    protoStack.stretch = SEQ_ELEM(stretches, 1);
	    userStretch        = SEQ_ELEM(stretches, 2);
	    
	    TRC(eprintf (" + Freeing SEQs\n"));
	    SEQ_FREE(sizes);
	    SEQ_FREE(stretches);


#ifdef CONFIG_MEMSYS_EXPT
	    /* Bind and map the stack */
	    {
		Stretch_Size sz; 
		addr_t stackva; 
		int npages; 

		TRC(printf("+ Binding and mapping stack.\n"));
		stackva = STR_RANGE(protoStack.stretch, &sz);
		npages  = sz >> PAGE_WIDTH;
		while(npages--) {
		    StretchDriver$Map(Pvs(sdriver), 
				      protoStack.stretch, stackva);
		    stackva += PAGE_SIZE;
		}

	    }
#endif

	    TRC(printf(" + Setting protection\n"));
	    SALLOC_SETGLOBAL(salloc, protoStack.guard, 0);
	    
	    /* XXX PRB Global write is a bit dodgy to say the least! */
	    SALLOC_SETGLOBAL(salloc, protoStack.stretch, 
			     SET_ELEM(Stretch_Right_Read)    |
			     SET_ELEM(Stretch_Right_Write)   );
	    SALLOC_SETGLOBAL(salloc, userStretch,
			     SET_ELEM(Stretch_Right_Read)    |
			     SET_ELEM(Stretch_Right_Write)   );

	    TRC(printf(" + Finding ThreadsPackage\n"));
	    tp = NAME_FIND("modules>NonPreemptiveThreads",ThreadsPackage_clp); 
	    TRC(printf(" + ThreadsPackage = %p\n", tp));
	    if(!(thdf = ThreadsPackage$New(tp, (addr_t)&NemesisMainThread,
					   (addr_t)kst, &protoStack,
					   userStretch,
					   nem_info->stackWords*sizeof(word_t), 
					   (Pervasives_Init *)PVS(), &actf))) {
		TRC(printf("Nemesis: can't get threads.\n"));
		ntsc_halt();
	    } 
	    /* Set ourselves up to handle memory faults */
	    
	    /* first get/set an event channel for fault notification */
	    rwp->mm_ep = VP$AllocChannel(Pvs(vp));
#if defined(CONFIG_MEMSYS_STD) || defined(CONFIG_MEMSYS_EXPT)
	    {
		MMNotifyMod_cl *mmnmod;
		MMNotify_cl *mmnfy;
		StretchTbl_clp strtable; 

#ifdef CONFIG_MEMSYS_EXPT
		/* get the stretch table */
		TRC(eprintf (" + Finding StretchTable\n"));
		strtable = NAME_FIND("sys>StretchTable", StretchTbl_clp);
#else
		strtable = (StretchTbl_cl *)NULL;
#endif
		mmnmod = NAME_FIND("modules>MMNotifyMod", MMNotifyMod_clp);
		mmnfy = MMNotifyMod$New(mmnmod, vp, Pvs(heap), strtable); 
		CX_ADD("sys>MMNotify", mmnfy, MMNotify_clp);
		ActivationF$Attach(actf, (ChannelNotify_cl *)mmnfy, 
				   rwp->mm_ep);
		
	    }
#endif      /* for non expt, we do all this with a mmentry in main thd */
	}
	TRC(printf(" + yielding to main thread.\n"));

	VP$ActivationsOn(Pvs(vp));
	VP$Yield(Pvs(vp));
	
    } CATCH_Context$NotFound(name) {
	printf("Caught execption Context$NotFound(%s)\n", name);
    } CATCH_ALL {
	printf("Caught exception %s\n", exn_ctx.cur_exception);
    } ENDTRY;
    
    /* XXX we are not happy here; for now, just halt. */
    printf("\nNemesis: Much bogosity!!! halting.\n");
    ntsc_dbgstop();
}


/*
 * This is the main thread for the Nemesis domain. It is 
 * reponsible for looking after basic system services and 
 * creating the initial set of domains.
 */

static void NemesisMainThread(kernel_st *kst)
{
    DomainMgrMod_clp	dmm;
    DomainMgr_clp	dm;

    TRC(printf("NemesisMainThread: entered.\n"));
    
    TRC(printf( " + Finding DomainMgr...\n"));
    dmm = NAME_FIND("modules>DomainMgrMod",DomainMgrMod_clp);
    dm  = NAME_FIND("sys>DomainMgr",DomainMgr_clp);
    
    TRC(printf( " + Completing Domain Manager initialisation.\n"));
    DomainMgrMod$Done (dmm, dm);

    /* Get a tag for ourselves and tell the world about it */
    TRC(printf( " + Creating a Security.Tag for the system.\n"));
    {
	Security_Tag tag;
	Security_clp sec;
	Type_Any any;

	sec=IDC_OPEN("sys>SecurityOffer", Security_clp);
	/* XXX will be local, so we don't need to worry about dropping
	   it on the floor */
	if (Security$CreateTag(sec, &tag)) {
	    ANY_INIT(&any, Security_Tag, tag);
	    Context$Add (Pvs(root), "sys>SystemTag", &any);
	} else {
	    printf("Error: couldn't create system tag\n");
	    ntsc_dbgstop();
	}
    }


#if defined(CONFIG_MEMSYS_STD) || defined(CONFIG_MEMSYS_EXPT)
    TRC(printf( " + Creating mmentry.\n"));
    {
	MMEntryMod_cl *mmemod;
	MMEntry_cl    *mmentry; 
	MMNotify_cl   *mmnfy;

	mmemod  = NAME_FIND("modules>MMEntryMod", MMEntryMod_clp);
	mmnfy   = NAME_FIND("sys>MMNotify", MMNotify_clp);
	mmentry = MMEntryMod$New(mmemod, mmnfy, (ThreadF_cl *)Pvs(thds), 
				 Pvs(heap), 10 /* XXX should be param? */);
    }
#endif

    TRC(printf( " + Register binder's protection domain.\n"));
    {
	ProtectionDomain_ID pdid = VP$ProtDomID(Pvs(vp));
	ANY_DECL(tmpany,ProtectionDomain_ID, pdid); 
	Context$Add (Pvs(root), "sys>BinderPDom",&tmpany);
    } 
    
    TRC(printf( " + Register standard root context.\n"));
    CX_ADD ("sys>StdRoot", Pvs(root), Context_clp);

    /* Interrupt Allocation Module */
    TRC(printf( " + Init interrupt allocation module.\n"));
    {
	Interrupt_clp ipt;
	
	/* Find the interrupt allocation module */
	ipt = InterruptInit(kst);
	TRC(printf(" + Created an Interrupt server at %p.\n", ipt));
	TRC(printf(" +   exporting... "));
	IDC_EXPORT ("sys>InterruptOffer", Interrupt_clp, ipt);
	TRC(printf("done\n"));
    }
    
#ifdef CONFIG_CALLPRIV
    /* CallPriv allocation module */
    TRC(printf( " + Init CallPriv allocation module.\n"));
    {
	CallPriv_clp cp;

	/* Find the callpriv module */
	cp = CallPrivInit(kst);
	TRC(printf(" + created a CallPriv server at %p\n", cp));
	TRC(printf(" +   exporting... "));
	IDC_EXPORT ("sys>CallPrivOffer", CallPriv_clp, cp);
	TRC(printf("done.\n"));
    }
#endif /* CONFIG_CALLPRIV */

#ifdef CONFIG_MODULE_OFFSET_DATA
    /* Module offset data module */
    TRC(printf( " + Init ModData module.\n"));
    {
	ModData_clp md;

	md=ModDataInit();
	TRC(printf(" + created a ModData server at %p\n", md));
	TRC(printf(" +   exporting... "));
	IDC_EXPORT ("sys>ModDataOffer", ModData_clp, md);
	TRC(printf("done.\n"));
    }
#endif /* CONFIG_MODULE_OFFSET_DATA */

    {
	Type_Any any;
	if (Context$Get(Pvs(root), "modules>PCIBiosInstantiate", &any)) {
	    Closure_clp pcibiosinstantiate;
	    pcibiosinstantiate = NARROW(&any, Closure_clp);
	    TRC(printf( " + Init PCIBios allocation module.\n"));
	    Closure$Apply(pcibiosinstantiate);
	}
    }
	   
#ifdef AXP3000 
    TRC(printf( " + Probing Turbochannel...\n"));
    {
	char name[TC_ROMNAMLEN+2]; /* 1 extra for number, one for '\0' */
	volatile uint32_t *romptr;
	volatile uint32_t *mchk_expected = &(kst->mchk_expected);
	volatile uint32_t *mchk_taken = &(kst->mchk_taken);
	Context_clp tc_cx;
	NOCLOBBER int i, j, k;
	NOCLOBBER char added, dev_no;
	
	tc_cx = CX_NEW("sys>tc");

	for(i=0; i < 6; i++) {
	    /* get a pointer to the 'name' of the card current slot */
	    romptr = (uint32_t *)((char *)slots[i].addr+(TC_NAME_OFFSET*2));
	    
	    /* setup kernel to ignore a mchk (such as TC read timeout) */
	    *mchk_expected = True;
	    *mchk_taken    = 0;

	    /* dereference the romptr & see if we get a mchk */
	    *((uint32_t *)TC_EREG) = 0; /* unlock tcerr */
	    *romptr;

	    if(!(*mchk_taken)) { /* all is ok => read out the name */
#define TC_ROM_STRIDE 2
		for (j = 0, k = 0; j < TC_ROMNAMLEN; j++, k += TC_ROM_STRIDE) 
		    name[j] = (char)romptr[k];
		name[TC_ROMNAMLEN] = name[TC_ROMNAMLEN+1] = ' ';
		
		/* Get rid of extra spaces at the end & add in device number */
		dev_no= '0';
		added = False;

		for (j = 0; name[j]!=' '; j++)
		    ;
		name[j]   = dev_no;
		name[j+1] = '\0'; 

	        while(!added) {
		    TRY {
			CX_ADD_IN_CX(tc_cx, (char *)name, 
				     &(slots[i]), TCOpt_SlotP);
			added= True;
		    } CATCH_Context$Exists() {
			name[j]= dev_no++;
		    } ENDTRY;
		}

		TRC(printf(" +++ probed slot %d: added card, name= \"%s\"\n", 
			i, name));

	    } else {
#define MCHK_K_TC_TIMEOUT_RD   0x14
		if(*mchk_taken==MCHK_K_TC_TIMEOUT_RD) {
		    TRC(printf(" +++ slot %d is empty.\n", i));
		} else {
		    TRC(printf(" *** Bad MCHK (code 0x%x) taken in TC probe."
			    "Halting.\n",
			    *mchk_taken));
		}
	    }
	}

	
	*((uint32_t *)TC_EREG) = 0; /* unlock tcerr */
	*mchk_expected =  False;    /* enable mchks again */

	TRC(printf(" + Turbochannel probe done.\n"));
    }
#endif /* AXP3000 */

#ifdef CONFIG_QOSCTL
    {
	if(!(sched_ctl_init())) {
	    printf("qosctl init failed.\n");
	    ntsc_dbgstop();
	} else {
	    TRC(printf("qosctl init done.\n"));
	}
    }
#endif

    /* export the TypeSystemF interface so people can register new
     * interfaces with the typesystem */
    IDC_EXPORT("sys>TypeSystemF", TypeSystemF_clp, Pvs(types));

    {
	NTSC_clp n;

	n=NTSC_access_init(kst);

	CX_ADD("sys>NTSC", n, NTSC_clp);
    }

    /* Start the kernel log daemon; it will output kernel messages into
       a pipe. We put the reader end of the pipe in the namespace. */
    {
	Rd_clp rd;
	Wr_clp wr;
	RdWrMod_clp rdwr=NAME_FIND("modules>RdWrMod",RdWrMod_clp);

	rd=RdWrMod$NewPipe(rdwr, Pvs(heap), 4096, 40, &wr);
	start_console_daemon(kst, wr);
	IDC_EXPORT("sys>KernelOutput", Rd_clp, rd);
    }

    /* At this point we're just about ready to go. There is still some
       strangeness about the Nemesis domain (its permanent privilege,
       for example), so we spawn a new domain to run the init script
       and the rest of the system services. */

    {
	IDCOffer_clp         salloc_offer;
	StretchAllocator_clp salloc;
	Builder_clp          builder;
	Activation_clp       vec;
	Domain_ID            dom;
	ProtectionDomain_ID  pdid=VP$ProtDomID(Pvs(vp));
	VP_clp               vp;
	Closure_clp          sys_clp = &sys_cl;
	Context_clp          env=
	    ContextMod$NewContext(NAME_FIND("modules>ContextMod",
					    ContextMod_clp),
				  Pvs(heap),Pvs(types));

	TRC(printf("Starting System domain...\n"));

	builder = NAME_FIND("modules>Builder", Builder_clp);

	vp = DomainMgr$NewDomain(dm, (Activation_cl *)NULL,
				 &pdid, 32 /* contexts */,
				 256 /* eps */, 128 /* frames */,
				 K(64) /* aHeapBytes */, True /* k */,
				 "System", &dom, &salloc_offer);

	salloc = IDC_BIND(salloc_offer, StretchAllocator_clp);

	{
	    uint32_t pHeapWords=65536;
	    CX_ADD_IN_CX(env,"pHeapWords",pHeapWords,uint32_t);
	}

	vec = Builder$NewThreaded (builder, sys_clp,
				   salloc, pdid, RW(vp)->actstr,
				   env);
	VP$SetActivationVector(vp, vec);

	{
	    Security_clp s = IDC_OPEN("sys>SecurityOffer",Security_clp);
	    Security_Tag systag = NAME_FIND("sys>SystemTag",Security_Tag);
	    Security$GiveTag(s, systag, dom, False);
	}

	DomainMgr$AddContracted(dm, dom, MILLISECS(20), MILLISECS(1),
				MILLISECS(1), True);
    }
    
#ifdef __SMP__
  /* Kick the AP's to do something useful... */
    PAUSE(SECONDS(5)); /* Probably not necessary, but I'm paranoid - mdw */
    printf("Kicking AP's to enter scheduler...\n");
    ntsc_smp_msg(SMP_MSG_ALL_BUT_SELF, SMP_MSG_RESCHED_FIRST, 0);
#endif
}

static void System(void)
{
    Context_clp trader;
    MergedContext_clp new_root;
    Context_clp stdroot;
    Clanger_clp interp;
    RdWrMod_Blob initrc;
    Rd_clp rd;
    Wr_clp console;
    ConsoleControl_clp console_control;
    
    stdroot=NAME_FIND("env>Root",Context_clp);

    /* Sort out the console redirector */
    console_control=InitConsole(&console);
    IDC_EXPORT("sys>StdRoot>sys>Console",ConsoleWr_clp,console);

    printf("Starting trader... ");
    StartTrader();
    printf("done.\n");

    /* Get the rc file */
    TRC(printf("Fetching init.rc\n"));
    initrc = NAME_FIND("blob>initrc", RdWrMod_Blob);
    TRC(printf("Obtaining RdWrMod\n"));
    rd = RdWrMod$GetBlobRd(NAME_FIND("modules>RdWrMod", RdWrMod_clp), initrc,
			   Pvs(heap));
	
    trader = IDC_OPEN("sys>TraderOffer", Context_clp);
	
    /* We're in a domain built by the Builder, so this is true */
    new_root = (MergedContext_clp)Pvs(root);
	
    MergedContext$AddContext (new_root, trader,
			      MergedContext_Position_Before,
			      False, stdroot);
	
    /* order is now:
       
       front    ------------->       back
       
       (private root) (trader) (standard root)
       
       */
    
    /* Put the console control closure in the namespace */
    {
	Type_Any any;

	ANY_INIT(&any, ConsoleControl_clp, console_control);
	Context$Add((Context_clp)new_root, "ConsoleControl", &any);
    }

    /* get a clanger interpreter */
    interp = ClangerMod$New(NAME_FIND("modules>ClangerMod", 
				      ClangerMod_clp));
	
    /* Execute the init.rc file */
	
#ifdef DEBUG
    printf("System domain namespace:\n");
    CX_DUMP(Pvs(root),MergedContext_clp);
#endif

    printf("About to exec init.rc\n");
	
    Pvs(out)=Pvs(err)=Pvs(console)=console;
    Clanger$ExecuteRd(interp, rd, Pvs(out));
}

static void SystemDomain(Closure_clp self)
{
    /* Direct output through the NTSC for now; the Builder will have set
       us up with Null output */

    Pvs(err)=Pvs(out)=Pvs(console)=&triv_wr_cl;
    printf("System domain started\n");
    
    TRY {
	System();
    } CATCH_ALL {
	eprintf("System domain caught an exception.\n");
	eprintf("%s:%d: %s in function %s\nHalting.\n",
		exn_ctx.filename, exn_ctx.lineno,
		exn_ctx.cur_exception, exn_ctx.funcname);
	ntsc_dbgstop();
    } ENDTRY;
}

/* End */
