/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/nemesis/builder/Builder.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Creates domain entry points which instantiate state on
**	startup. A bit like crt0.o, but better.
** 
** ENVIRONMENT: 
**
**      User-space service.
** 
** ID : $Id: Builder.c 1.4 Wed, 09 Jun 1999 15:26:27 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <domid.h>
#include <ntsc.h>
#include <salloc.h>
#include <exceptions.h>
#include <contexts.h>

#include <stdio.h>
#include <time.h>
#include <ecs.h>

#include <IDCMacros.h>
#include <VPMacros.h>

#include <ActivationF.ih>
#include <Builder.ih>
#include <ExnSystem.ih>
#include <Type.ih>
#include <ThreadsPackage.ih>
#include <HeapMod.ih>
#include <ObjectTblMod.ih>
#include <GatekeeperMod.ih>
#include <ContextMod.ih>
#include <WordTblMod.ih>
#include <ActivationMod.ih>
#include <EntryMod.ih>

#include <autoconf/binder.h>
#include <autoconf/net_console.h>
#include <autoconf/memsys.h>

#ifdef EXPORT_RSSMONITOR
#include <RSSMonitorMod.ih>
#endif
#if defined(CONFIG_MEMSYS_STD) || defined(CONFIG_MEMSYS_EXPT)
#  include <MMEntryMod.ih>
#  include <MMNotifyMod.ih>
#  include <StretchTblMod.ih>
#endif

#ifdef CONFIG_MEMSYS_EXPT
#include <SDriverMod.ih>
#include <WordTblMod.ih>
#include <frames.h>         /* pull in flist_t */
#endif

#ifdef DEBUG
#define PTRC(_x) _x
#define TRC(_x) _x
#else
#define PTRC(_x)
#define TRC(_x)
#endif
 
/* 
 * Domains fail currently by just plain halting the machine
 */
#define DomainFail() ntsc_dbgstop()

/*
 * Module stuff
 */
PUBLIC Builder_NewThreaded_fn	NewThreaded_m;
PUBLIC Builder_op		b_ms = { NewThreaded_m };
PUBLIC const Builder_cl		b_cl = { &b_ms, (addr_t) 0 };

CL_EXPORT (Builder, Builder, b_cl);


/*
 * Activations
 */
PUBLIC Activation_Go_fn		NewThreaded_Go;
PUBLIC Activation_op		NewThreaded_ops = { NewThreaded_Go };

/*
 * Proto main thread operations
 */
PUBLIC void			Proto_Main (addr_t data);


/*
 * Type of record passed to domain at startup.
 */
typedef void (*entry_t)(addr_t data);

typedef struct Initrec_t Initrec_t;
struct Initrec_t {
    Activation_cl         cl;
    Heap_clp		  heap;
    TypeSystem_clp	  typsys;
    addr_t                libc;
    Context_clp           env;
    ThreadsPackage_Stack  protoStack;
    uint32_t		  defStackWords;
    addr_t                stackbase;
    Stretch_clp           userStretch;
    Closure_clp           main_cl;

    MMNotify_clp          mmnfy; /* Passed from NewThreaded_Go to Proto_Main */
};
	   


PUBLIC Activation_clp
NewThreaded_m (Builder_cl	*self,
	       Closure_clp	cl,
	       StretchAllocator_clp salloc,
	       ProtectionDomain_ID pdid,
	       Stretch_clp      actstr,
	       Context_clp      env)
{
    HeapMod_clp	 heapmod;
    Heap_clp   	 new_heap;
    Initrec_t	*st;
    Type_Any     any;
    Context_clp  parent_env;
    uint32_t pHeapWords=8192, defStackWords=4096;

    PTRC(printf("Builder$NewThreaded: entered.\n"));

    if (pdid == NULL_PDID) {
       PTRC(fprintf(stderr,"Builder$NewThreaded: no PDID supplied.\n"));
       return (Activation_cl *)NULL;
    }

    /* Sort out defaults */

    if (!env) {
	ContextMod_clp cmod=NAME_FIND("modules>ContextMod",ContextMod_clp);
	PTRC(fprintf(stderr,"Builder: Dude, you didn't supply an env context; "
		    "\n  - I'm going to try to copy from yours implicitly\n"));
	env=ContextMod$NewContext(cmod,Pvs(heap),Pvs(types));
    }

    if (Context$Get(Pvs(root), "env", &any) && ISTYPE(&any, Context_clp)) {
	parent_env=NARROW(&any, Context_clp);
    } else {
	PTRC(fprintf(stderr,"Builder: the parent domain doesn't have an "
		     "env context; \n  - using compiled-in defaults\n"));
	parent_env=NULL;
    }

#define DEFAULT(_name,_defname,_type) \
    if (!Context$Get(env,_name,&any)) { \
	if (parent_env && Context$Get(parent_env,_name,&any) && \
	    ISTYPE(&any, _type)) { \
	    Context$Add(env, _name, &any); \
	} else if (Context$Get(Pvs(root),_defname,&any) && \
		   ISTYPE(&any, _type)) { \
	    Context$Add(env, _name, &any); \
	} \
    }

#define DEFNUM(_name) \
    if (Context$Get(env,#_name,&any)) { \
	if (ISTYPE(&any, uint32_t)) { \
	    _name=NARROW(&any, uint32_t); \
	} else if (ISTYPE(&any, int64_t)) { \
	    _name=NARROW(&any, int64_t); /* XXX Clanger hack */ \
	    TRC(printf("Builder: Clanger hack: " #_name " = 0x%x\n",_name)); \
	} else { \
	    TRC(fprintf(stderr,"Builder$NewThreaded: " #_name " in env is not " \
		    "CARDINAL - defaulting\n")); \
	    Context$Remove(env,#_name); \
	    ANY_INIT(&any, uint32_t, _name); \
	    Context$Add(env,#_name,&any); \
	} \
    } else { \
	ANY_INIT(&any, uint32_t, _name); \
	if (parent_env && Context$Get(parent_env,#_name,&any) && \
	    ISTYPE(&any, uint32_t)) { \
	    _name=NARROW(&any, uint32_t); \
	} \
	Context$Add(env,#_name,&any); \
    }

    DEFAULT("Root","sys>StdRoot",Context_clp);
    DEFAULT("ThreadsPackage","modules>PreemptiveThreads",ThreadsPackage_clp);
    DEFAULT("ExnSystem","modules>ExnSystem",ExnSystem_clp);
    DEFAULT("ContextMod","modules>ContextMod",ContextMod_clp);
    DEFAULT("Trader","sys>TraderOffer",IDCOffer_clp);
    DEFAULT("Console","sys>Console",IDCOffer_clp);

    DEFNUM(pHeapWords);
    DEFNUM(defStackWords);

    PTRC(fprintf(stderr,"Builder: pHeapWords=%d defStackWords=%d\n",
		pHeapWords, defStackWords));
#ifdef DEBUG
    {
	word_t size=0;

	Stretch$Info(actstr, &size);

	printf("Builder: actstr is %d bytes\n",size);
    }
#endif /* DEBUG */

    /* 
    ** Create a heap inside the child's activation stretch. This will be
    ** the heap in which the threads package, event counts, and various 
    ** other activation handler-level things will store their state. 
    */
    heapmod = NAME_LOOKUP("Root>modules>HeapMod",env,HeapMod_clp);
    new_heap = HeapMod$NewGrowable(heapmod, actstr, 1);
    /* Create an init record. Fill it in.  */
    st = Heap$Malloc(new_heap, sizeof(*st));
    if (!st) RAISE_Heap$NoMemory();
    st->heap          = new_heap;
    st->typsys        = NAME_LOOKUP("Root>sys>TypeSystem",env,TypeSystem_clp);
    st->libc          = Pvs(libc);
    st->defStackWords = defStackWords;
    
    {
	StretchAllocator_SizeSeq *sizes;
	StretchAllocator_StretchSeq *stretches;
	
	sizes = SEQ_NEW(StretchAllocator_SizeSeq, 3, Pvs(heap));
	SEQ_ELEM(sizes, 0) = FRAME_SIZE; /* for guard page */
	SEQ_ELEM(sizes, 1) = defStackWords*sizeof(word_t);
	SEQ_ELEM(sizes, 2) = pHeapWords*sizeof(word_t); 
	
	PTRC(printf (" + Allocating Stretches\n"));
	/* 
	** XXX SMH: don't use STR_NEWLIST_SALLOC() here since we 
	** don't want to bind the below stretches to one of our 
	** (we are the parent) stretch drivers. We'll bind 'em
	** later in the child.
	*/
	stretches = StretchAllocator$NewList(salloc, sizes, 0);
	
	st->protoStack.guard   = SEQ_ELEM(stretches, 0);
	st->protoStack.stretch = SEQ_ELEM(stretches, 1);
	st->userStretch        = SEQ_ELEM(stretches, 2);
	
	PTRC(printf (" + Freeing SEQs\n"));
	SEQ_FREE(sizes);
	SEQ_FREE(stretches);
	
	PTRC(printf (" + Setting protection\n"));
	SALLOC_SETGLOBAL(salloc, st->protoStack.guard, 0);
	SALLOC_SETPROT(salloc, st->protoStack.guard, pdid, 0); 
	PTRC(printf("+ Setting stack access rights\n"));
	SALLOC_SETPROT(salloc, st->protoStack.stretch, pdid,
		       SET_ELEM(Stretch_Right_Read)    |
		       SET_ELEM(Stretch_Right_Write)   |
		       SET_ELEM(Stretch_Right_Meta)    );
	SALLOC_SETPROT(salloc, st->userStretch, pdid,
		       SET_ELEM(Stretch_Right_Read)    |
		       SET_ELEM(Stretch_Right_Write)   |
		       SET_ELEM(Stretch_Right_Meta)    );
	/* 
	** XXX XXX
	** The 'userStretch' is turned into a heap later on by the threads 
	** package. In fact, it becomes the new Pvs(heap). 
	** Since for now we want Pvs(heap) to be [default] globally readable, 
	** we need to do the below. 
	*/
	SALLOC_SETGLOBAL(salloc, st->userStretch, 
			 SET_ELEM(Stretch_Right_Read));


	/*
	 * XXX init the protoStack with d0d0 so we can do stack usage
	 * testing
	 */
#ifndef CONFIG_MEMSYS_EXPT
	{
	    addr_t base;
	    word_t length;
	    base = STR_RANGE(st->protoStack.stretch, &length);
	    st->stackbase= base;
	    SALLOC_SETGLOBAL(salloc, st->protoStack.stretch, 
			   SET_ELEM(Stretch_Right_Read)|
			   SET_ELEM(Stretch_Right_Write));

	    memset(base, 0xd0, length);
	    SALLOC_SETGLOBAL(salloc, st->protoStack.stretch, 0);

	}
#endif
    }

    PTRC(printf("+ copying env\n"));
    /* Copy env into the new heap, taking care not to duplicate Root */
    {
	Context_clp root=NAME_LOOKUP("Root",env,Context_clp);
	Context_clp env_copy = env;
	WordTblMod_clp wtmod = NAME_LOOKUP("Root>modules>WordTblMod",env,
					   WordTblMod_clp);
	WordTbl_clp xl = WordTblMod$New (wtmod, st->heap);
	
	WordTbl$Put (xl, (WordTbl_Key) root, (WordTbl_Val) root);
	
	TRY
	{
	    st->env = Context$Dup (env_copy, new_heap, xl);
	}
	CATCH_ALL
	{
	    PTRC(printf("Got exception whilst duplicating env\n"));
	}
	ENDTRY;
	
	WordTbl$Destroy (xl);
    }

    st->main_cl = cl;
    CL_INIT(st->cl,&NewThreaded_ops,st);

    PTRC(printf("Builder$NewThreaded: st=%x st->heap=%x st->env=%x.\n",
		st, st->heap, st->env));
#ifdef DEBUG
    CX_DUMP(st->env,Context_clp);
#endif /* DEBUG */
    PTRC(PAUSE(SECONDS(1)));
    return &st->cl;
}

/*
 * The entry point of the domain created by NewThreaded.
 */
void NewThreaded_Go( Activation_cl *self, VP_clp vpp, Activation_Reason ar )
{
    Initrec_t 	    *init = (Initrec_t *)(self->st);
    ExnSystem_clp    excsys;
    ActivationF_cl  *actf;
    StretchTbl_cl   * NOCLOBBER strtab;
    Pervasives_Rec  act_pvs;

    /* Have to initialise Pvs(libc) for eprintf() to work */
    PVS()        = &act_pvs;

    Pvs(vp) 	 = vpp;
    Pvs(libc)    = init->libc;
    Pvs(console) = NULL; /* Catch things that rely on it... */
    Pvs(out)     = NULL;
    Pvs(err)     = NULL;
    Pvs(in)      = NULL;
    Pvs(heap)	 = init->heap;
    Pvs(types)	 = init->typsys;
    Pvs(exns)    = NULL;

    TRC(eprintf( "NewThreaded: act_pvs at %p.\n", &act_pvs));

    /* We need to initialise Pvs(root) before memsys_expt will work;
       but, we must be careful not to raise any exceptions. */
    {
	Type_Any any;
	
	if (!Context$Get(init->env, "Root", &any)) {
	    eprintf("NewThreaded: can't find standard root in env\n");
	    DomainFail();
	}
	if (!ISTYPE(&any, Context_clp)) {
	    eprintf("NewThreaded: env>Root is of wrong type\n");
	    DomainFail();
	}
	Pvs(root)=NARROW(&any, Context_clp);
    }

    /* When we arrive here, we have been activated for the first time */ 
    TRC(eprintf("Builder$NewThreaded_Go: entered\n"));
    TRC(eprintf(" + Activation clp at %lx\n", self));
    TRC(eprintf(" + VP clp at %lx\n", vpp));
    TRC(eprintf(" + Activation Reason %lx\n", ar));
    TRC(eprintf(" + init->heap    = %x\n", init->heap ));
    TRC(eprintf(" + init->typsys  = %x\n", init->typsys ));
    TRC(eprintf(" + init->libc    = %x\n", init->libc ));
    TRC(eprintf(" + init->protoStack.guard   = %p\n", init->protoStack.guard));
    TRC(eprintf(" + init->protoStack.stretch = %p\n", 
		init->protoStack.stretch));
    TRC(eprintf(" + init->userStretch  = %p\n", init->userStretch ));

#ifdef CONFIG_MEMSYS_EXPT
    {
	StretchTblMod_cl *stmod;
	SDriverMod_cl    *sdmod; 
	StretchDriver_cl *sdriver;
#define MAX_DESCS 5
	Mem_PMemDesc pmem[MAX_DESCS + 1];
	Type_Any foffer_any; 
	flink_t *lk;
	flist_t *cur; 
	dcb_ro_t *rop = RO(vpp);
	int i = 0; 
	
	stmod  = NAME_FIND("modules>StretchTblMod", StretchTblMod_clp);
	TRC(eprintf("NewThreaded_Go: got stmod at %p\n", stmod));
	sdmod  = NAME_FIND("modules>SDriverMod", SDriverMod_clp);
	TRC(eprintf("NewThreaded_Go: got sdmod at %p\n", sdmod));
	strtab = StretchTblMod$New(stmod, init->heap);
	TRC(eprintf("NewThreaded_Go: created strtab at %p\n", strtab));
	
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
	
	ANY_INIT(&foffer_any, IDCOffer_clp, rop->frames_offer);
	sdriver = SDriverMod$NewPhysical(sdmod, vpp, init->heap, 
					 strtab, pmem, &foffer_any);


	TRC(eprintf("Threads$New: created sdriver at %p\n", sdriver));

	/* Now want to bind the stretches our parent created on our behalf */
	StretchDriver$Bind(sdriver, init->protoStack.guard, PAGE_WIDTH);
	StretchDriver$Bind(sdriver, init->protoStack.stretch, PAGE_WIDTH);
	StretchDriver$Bind(sdriver, init->userStretch, PAGE_WIDTH);
	Pvs(sdriver) = sdriver;
    }
#endif

    /* We get the threads package and exception system directly from env */

    /* Get hold of the exception system, being careful not to raise any */
    /* exceptions */
    TRC(eprintf("NewThreaded: Creating exception runtime\n"));
    {
	Type_Any any;
	
	if (!Context$Get(init->env,"ExnSystem", &any )) {
	    eprintf("NewThreaded: failed to "
		    "find exception runtime.\n");
	    DomainFail();
	}
	if (!ISTYPE(&any, ExnSystem_clp)) {
	    eprintf(" !!! wrong type for exception runtime.\n");
	    DomainFail();
	} else {
	    excsys    = NARROW (&any, ExnSystem_clp);
	    Pvs(exns) = (ExnSupport_clp) ExnSystem$New(excsys);
	}
    }

    /* We now have an exception system */

    TRY {
	
	TRC(eprintf("NewThreaded: creating threads package.\n"));
	{
	    ThreadsPackage_clp tp;
	    ThreadF_clp thdf;

	    tp = NAME_LOOKUP("ThreadsPackage", init->env, ThreadsPackage_clp); 
	    
	    /* the heap we have at this point in Pvs(heap) will be used
	       by our threads package for stuff early on ; i.e. it is 
	       our "activation heap". So better make sure the damn thing
	       is mapped completed (cannot take faults on it!) */
	    thdf = ThreadsPackage$New(tp, (addr_t) &Proto_Main,
				      (addr_t) init, &init->protoStack,
				      init->userStretch,
				      init->defStackWords*sizeof(word_t),
				      (Pervasives_Init *)PVS(), &actf);
	    if(!thdf) {
		eprintf("NewThreaded: failed to get threads package.\n");
		DomainFail();
	    } 
#if defined(CONFIG_MEMSYS_STD) || defined(CONFIG_MEMSYS_EXPT)
	    TRC(eprintf("NewThreaded: creating mmnotify.\n"));
	    {
		MMNotifyMod_cl *mmnmod;
		dcb_rw_t *rwp = RW(vpp);

		mmnmod = NAME_FIND("modules>MMNotifyMod", MMNotifyMod_clp);
		init->mmnfy= MMNotifyMod$New(mmnmod, vpp, init->heap, strtab); 

		/* get/set an event channel for memory management faults */
		rwp->mm_ep = VP$AllocChannel(vpp);
		ActivationF$Attach(actf, (ChannelNotify_cl *)init->mmnfy, 
				   rwp->mm_ep);
	    }
#endif
	}
	

	TRC(eprintf("NewThreaded: yield\n"));
	VP$ActivationsOn (vpp);
	/* Here, the save slot == the resume slot */

	VP$Yield (vpp);
	
    } CATCH_ALL {
	eprintf("NewThreaded: UNHANDLED EXCEPTION %s\n",
		    exn_ctx.cur_exception);
    } ENDTRY;
    
    TRC(eprintf("NewThreaded: domain exiting.\n"));
    DomainFail();
    return;
}

/*
 * Proto main thread
 * 
 * This is the first thread of a NewThreaded domain.  
 */
PUBLIC void Proto_Main (addr_t data)
{
    Initrec_t	       *init        = (Initrec_t *)(data);
    Closure_clp		main_cl     = init->main_cl;
    MMNotify_cl        *mmnfy       = init->mmnfy;
    Heap_cl            *actheap     = init->heap;
    Context_clp         env         = init->env;
    IDCClientBinding_clp binder_binding;
    StretchAllocator_clp  sa;
    GatekeeperMod_clp gkm; 
    Frames_clp frames; 
    HeapMod_clp hmod; 

    TRC(eprintf("Proto_Main: pvs=%x heap=%x\n", PVS(), Pvs(heap)));

    /* We now free the init record. */
    FREE (init);
    
    /* XXX PRB A proper gatekeeper COULD be provided with a stretch for 
       talking to the Nemesis domain - put it in the DCBRO, for example */
    /* XXX SMH for now we use a 'simple' gatekeeper for the next 30 lines 
       or so of code, and then replace it with a proper one. */
    TRC(eprintf("ProtoThread: creating initial gatekeeper.\n"));
    gkm= NAME_FIND("modules>GatekeeperMod", GatekeeperMod_clp);
    Pvs(gkpr) = GatekeeperMod$NewSimple(gkm,Pvs(heap));

    TRC(eprintf("ProtoThread: creating default entry.\n"));
    {
	EntryMod_clp etm;

	etm  = NAME_FIND("modules>EntryMod", EntryMod_clp);
	Pvs(entry) = EntryMod$New (etm, Pvs(actf), Pvs(vp), 
				   VP$NumChannels (Pvs(vp)));
    } 
    
    /* Must get Pvs(bndr) before initialising ObjectTbl */
    TRC(eprintf("ProtoThread: binding to Binder.\n"));
    {
	Type_Any bany;

	binder_binding = IDCOffer$Bind (VP$BinderOffer (Pvs(vp)), 
					Pvs(gkpr), &bany);
	Pvs(bndr) = NARROW (&bany, Binder_clp);
    }

    /* XXX PRB Could probably do with a connection to a Stretch
       Allocator now so that we can do IDC! If the GateKeeper already
       has a stretch for talking to the Nemesis Domain then we can do a
       standard IDCOffer_Bind to a per-domain StretchAllocator offer in
       our initial namespace.  Note that the macro IDC_BIND imports the
       offer via the ObjectTbl so as to cache the binding - we haven't got
       one of those yet! */

    /* XXX PRB ObjectTbl really needs to run with a LOCKED heap so if 
       we move that code out of ThreadsPackege_ThreadTop it should go
       before this point.  */

    TRC(eprintf("ProtoThread: creating object table.\n"));
    {
	ObjectTblMod_clp otm = 
	    NAME_FIND("modules>ObjectTblMod", ObjectTblMod_clp);
	BinderCallback_clp waste;
	Pvs(objs) = ObjectTblMod$New(otm, Pvs(heap), Pvs(bndr), &waste);
    }
    TRC(eprintf("ProtoThread: success.\n"));

#ifdef CONFIG_BINDER_MUX
    {
	Type_Any fany, sany; 

	/* XXX SMH You never know... this may just work! */
	TRC(eprintf("ProtoThread: binding to StretchAlloc.\n"));
	(void) IDCOffer$Bind(RO(Pvs(vp))->salloc_offer, Pvs(gkpr), &sany);
	sa = NARROW(&sany, StretchAllocator_clp);

	TRC(eprintf("ProtoThread: binding to Frames.\n"));
	(void) IDCOffer$Bind(RO(Pvs(vp))->frames_offer, Pvs(gkpr), &fany);
	frames = NARROW(&fany, Frames_clp);
	TRC(eprintf("ProtoThread: success.\n"));
    }
#else
    /* XXX PRB You never know... this may just work! */
    TRC(eprintf("ProtoThread: binding to StretchAlloc.\n"));
    sa = IDC_BIND(RO(Pvs(vp))->salloc_offer, StretchAllocator_clp);
    TRC(eprintf("ProtoThread: binding to Frames.\n"));
    frames = IDC_BIND(RO(Pvs(vp))->frames_offer, Frames_clp);
    TRC(eprintf("ProtoThread: success.\n"));
#endif


    Pvs(salloc)= sa;

    /* Once we have a stretch allocator, we can create a proper gatekeeper */ 
    hmod= NAME_FIND("modules>HeapMod", HeapMod_clp);
    Pvs(gkpr) = GatekeeperMod$New(gkm, sa, hmod, Pvs(heap), frames);

    /* XXX SMH destroy the simple gatekeeper now?? */
#if defined(CONFIG_MEMSYS_STD) || defined(CONFIG_MEMSYS_EXPT)
    {
	MMEntryMod_cl *mmemod;
	MMEntry_cl    *mmentry; 

	mmemod  = NAME_FIND("modules>MMEntryMod", MMEntryMod_clp);
	mmentry = MMEntryMod$New(mmemod, mmnfy, (ThreadF_cl *)Pvs(thds), 
				 Pvs(heap), 10 /* XXX should be param? */);
    }
#endif

    /* Now we sort out the new domain's namespace. Up until now, Pvs(root)
       was the supplied env>Root from the parent domain. We create a
       MergedContext, with the intention of having it look like this:

       (new root)  [priv_root] (Bind[env>Trader])  (env>Root)

       Inside (new root) we bind env as "env", and also create some new
       contexts for our own use: "transports", "sys". We add priv_root
       if it's present in the env context.

       */
    
    {
	ContextMod_clp     cmod;
	MergedContext_clp  merged_root, merged_sys;
	Context_clp
	    std_root,
	    new_root, priv_root,
	    root_sys, local_sys,
	    transports;
	Context_clp  trader;
	
	cmod = NAME_LOOKUP("ContextMod",env,ContextMod_clp);
	std_root    = Pvs(root);
	merged_root = ContextMod$NewMerged(cmod, Pvs(heap), Pvs(types));
	merged_sys  = ContextMod$NewMerged(cmod, Pvs(heap), Pvs(types));
	new_root    = ContextMod$NewContext(cmod, Pvs(heap), Pvs(types));
	local_sys   = ContextMod$NewContext(cmod, Pvs(heap), Pvs(types));
	transports  = ContextMod$NewContext(cmod, Pvs(heap), Pvs(types));
	root_sys    = NAME_FIND("sys", Context_clp);
	{
	    Type_Any any;
	    if (Context$Get(env, "priv_root", &any)) {
		priv_root=NARROW(&any, Context_clp);
	    } else {
		priv_root=NULL;
	    }
	}
	    
	/* merged_root */
	MergedContext$AddContext(merged_root, Pvs(root),
				 MergedContext_Position_Before,
				 False, NULL);
	if (priv_root) {
	    MergedContext$AddContext(merged_root, priv_root,
				     MergedContext_Position_Before,
				     False, NULL);
	}
	MergedContext$AddContext(merged_root, new_root,
				 MergedContext_Position_Before,
				 False, NULL);

	/* merged_sys */
	MergedContext$AddContext(merged_sys, root_sys,
				 MergedContext_Position_Before,
				 False, NULL);
	MergedContext$AddContext(merged_sys, local_sys,
				 MergedContext_Position_Before,
				 False, NULL);

	/* new_root */
	CX_ADD_IN_CX(new_root, "env", env, Context_clp);
	CX_ADD_IN_CX(new_root, "sys", merged_sys, MergedContext_clp);
	CX_ADD_IN_CX(new_root, "transports", transports, Context_clp);
	CX_NEW_IN_CX(new_root, "symbols");
	CX_NEW_IN_CX(new_root, "binaries");
	/* Save the binding to the binder - we exit the domain by 
	   destroying this binding */ 
	CX_ADD_IN_CX (local_sys, "Binder", 
		      binder_binding, IDCClientBinding_clp);

	/* Save the StretchAllocator somewhere handy */
	CX_ADD_IN_CX (local_sys, "StretchAllocator", 
		      sa, StretchAllocator_clp);
	/* And the Frames allocator too */
	CX_ADD_IN_CX (local_sys, "Frames", frames, Frames_clp);
	/* And our activation heap (nailed and unlocked) */
	CX_ADD_IN_CX(local_sys, "ActHeap", actheap, Heap_clp);

	Pvs(root) = (Context_clp) merged_root;

	/* Set up the stub generator - up until this point in
	   initialisation, we've relied on Nemesis having primed its
	   own stubs cache for us, and have used the Nemesis state
	   records directly. */

	{
	    Closure_clp sg = NAME_FIND("modules>StubGenInit", Closure_clp);
	    /* Slight hack - we point the state of the closure at the
               local sys context, so that it knows where to put our
               stubgen closure */
	    /* XXX SDE: what do you mean, a slight hack? This is awful. */

	    Closure_cl local_sg;

	    CL_INIT(local_sg, sg->op, local_sys);
	    Closure$Apply(&local_sg);
	}

	/* If we have a trader offer in our hands, bind to it. If "env"
	   doesn't contain a trader offer, we don't bother: perhaps this
	   domain is going to be a trader. */
	{
	    Type_Any any;
	    IDCOffer_clp o;

	    if (Context$Get(env, "Trader", &any)) {
		TRC(eprintf("ProtoThread: bind to trader.\n"));
		o=NARROW(&any, IDCOffer_clp);
		trader = IDC_BIND (o, Context_clp);
		TRC(eprintf("ProtoThread: back from bind to trader!\n"));

		/* Add trader behind everything except stdroot */
		MergedContext$AddContext (merged_root, trader,
					  MergedContext_Position_Before,
					  False, std_root);
	    }
	}

    }

    /* If env>stdout or env>stderr exist, use those as Pvs(out) and Pvs(err).
       If only one of them exists, use modules>NullWr for the other. */
    Pvs(out)=Pvs(err)=Pvs(console)=NAME_FIND("modules>NullWr",Wr_clp);
    Pvs(in) = NAME_FIND("modules>NullRd",Rd_clp);

    TRY {
	Pvs(console) = IDC_OPEN("env>Console", Wr_clp);
    } CATCH_ALL {
	Domain_ID did;

	did = VP$DomainID(Pvs(vp));
	if (did != 0xd000000000000001ULL) {
	    eprintf("Builder: Warning: domain %qx has no Pvs(console)\n",
		    VP$DomainID(Pvs(vp)));
	} else {
	    /* XXX: we expect domain 1 to have no Pvs(console) */
	}
    } ENDTRY;

    TRY {
	Pvs(out) = IDC_OPEN("env>stdout", Wr_clp);
    } CATCH_ALL {
	Pvs(out) = Pvs(console);
    } ENDTRY;

    TRY {
	Pvs(err) = IDC_OPEN("env>stderr", Wr_clp);
    } CATCH_ALL {
	Pvs(err) = Pvs(console);
    } ENDTRY;

    TRY {
	Pvs(in) = IDC_OPEN("env>stdin", Rd_clp);
    } CATCH_ALL {
    } ENDTRY;
    TRY {
	Context_clp self;
	char *domname = malloc(64);
	sprintf(domname, "proc>domains>%qx>thiscontext", VP$DomainID(Pvs(vp)));
	
	if (NAME_EXISTS(domname)) {
	    self = IDC_OPEN(domname, Context_clp);
	    CX_ADD("self", self, Context_clp);

	    CX_ADD("self>heap_0", init->heap, Heap_clp);

/* XXX; for some reason, Heap$Query on Pvs(heap) crashes... */
 	    CX_ADD("self>heap_1", Pvs(heap), Heap_clp); 
	    CX_ADD("self>thread_0_stack", init->protoStack.stretch, 
		   Stretch_clp);

#if 0
	    /* this code was written in case special case handling of
	       the proto stack is necessary */
	    CX_ADD("self>thread_0_stack_base", init->stackbase, addr_t);
	    CX_ADD("self>thread_0_stack_length", init->defStackWords*sizeof(word_t), word_t);
#endif
	} else {
	    eprintf("Self %s missing\n", domname);
	}
    } CATCH_ALL {
    } ENDTRY;

    

    TRC(eprintf("ProtoThread: Pvs(out)=%p Pvs(err)=%p Pvs(console)=%p\n",
		Pvs(out),Pvs(err),Pvs(console)));

#ifdef EXPORT_RSSMONITOR
    if ((uint32_t) VP$DomainID(Pvs(vp)) > 1) {
	RSSMonitorMod_clp mod = NAME_FIND("modules>RSSMonitorMod", RSSMonitorMod_clp);
	RSSMonitor_clp rss;
	eprintf("RSS monitor module found; invoking it\n");
	rss = RSSMonitorMod$New(mod, False);
	IDC_EXPORT("self>RSSMonitor", RSSMonitor_clp, rss);
    }
#endif	



    TRC(eprintf("ProtoThread: piling into closure %x\n", main_cl));
    
    Closure$Apply (main_cl);
    
    TRC(eprintf("ProtoThread: main_cl returned.\n"));

    return;
}


