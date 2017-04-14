/*
 *****************************************************************************
 **                                                                          *
 **  Copyright 1996, University of Cambridge Computer Laboratory             *
 **                                                                          *
 **  All Rights Reserved.					            *
 **                                                                          *
 *****************************************************************************
 **
 ** FACILITY:
 **
 **      sys/Primal
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 **      Magically creates the Nemesis domain from thin air.
 ** 
 ** ENVIRONMENT: 
 **
 **      Bizarre limbo-land.
 ** 
 ** ID : $Id: NemesisPrimal.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 ** 
 */
#include <nemesis.h>
#include <autoconf/measure_trace.h>
#include <autoconf/memsys.h>
#include <autoconf/stubgen.h>
#include <stdio.h>
#include <exceptions.h>
#include <contexts.h>
#include <salloc.h>
#include <ntsc.h>
#include <time.h>
#include <VPMacros.h>

#include <Activation.ih>
#include <BootDomain.ih>
#include <VP.ih>
#include <ExnSystem.ih>
#include <FramesMod.ih>
#include <HeapMod.ih>
#include <ThreadsPackage.ih>
#include <SAllocMod.ih>
#include <StretchAllocatorF.ih>
#include <StringTblMod.ih>
#include <TypeSystemMod.ih>
#include <LongCardTblMod.ih>
#include <DomainMgrMod.ih>
#include <Timer.ih>
#include <RdWrMod.ih>
#include <IDCStubs.ih>
#include <ProtectionDomain.ih>
#include <nexus.h>
#include <pip.h>

#include <RamTab.ih>
#include <MMUMod.ih>

#ifdef CONFIG_MEMSYS_EXPT
#include <SDriverMod.ih>
#include <StretchTblMod.ih>
#endif

#include <kernel_st.h>
#ifdef CONFIG_MEASURE_TRACE
#include <TraceMod.ih>
#endif

#ifdef DEBUG
#define TRC(_x) _x
#define TRC_CTX(_x) _x
#define TRC_STUBS(_x) _x
#define TRC_PRG(_x) _x
#else
#define TRC(_x)
#define TRC_CTX(_x)
#define TRC_STUBS(_x) 
#define TRC_PRG(_x) 
#endif 

/* Define below for extra memory-related tracing stuff */
#define MTRC(_x)

extern Wr_cl triv_wr_cl; /* From libdebug.a */

extern void init_nexus();
extern void set_namespace(namespace_entry *, char *);
extern void *lookup_name(char *name);
extern char *lookup_next(addr_t *val);
extern nexus_prog *find_next_prog();

/* memory stuff from addr_init_$(MC).c */
#include "addr_init.h"


static Pervasives_Rec NemesisPVS;
#ifdef __IX86__
static Pervasives_Rec *ppvs = &NemesisPVS;
#endif


static void *lookup(char *name)
{
    void *v = lookup_name(name);

    if (!v)
    {
	eprintf("primal: need: %s!\n", name);
	ntsc_halt();
    }

    return v;
}


/* Add the program described by "ci" into the progs> context,
 * "progs".   Called for each program in the .nbf that doesn't
 * have the 'b' flag set */
static void mk_prog_cx(Context_clp progs, BootDomain_Info *ci)
{
    Context_clp	qos, cpu, env, p;

    TRY {
	p = CX_NEW_IN_CX(progs, ci->name); /* named program context */

	/* the program context:
	 *   qos
	 *     \-cpu
	 *     |   \-nctxts
	 *     |   \-neps
	 *     |   \-p,s,l
	 *     |   \-k,x
	 *     \-aHeapWords
	 *   b_env
	 *     \-defStackWords
	 *     \-pHeapWords
	 *     \-priv_root
	 */
	qos = CX_NEW_IN_CX(p, "qos");
	cpu = CX_NEW_IN_CX(qos, "cpu");
	CX_ADD_IN_CX(cpu, "nctxts", ci->nctxts, uint32_t);
	CX_ADD_IN_CX(cpu, "neps",   ci->neps,   uint32_t);
	CX_ADD_IN_CX(cpu, "p",      ci->p,      Time_ns);
	CX_ADD_IN_CX(cpu, "s",      ci->s,      Time_ns);
	CX_ADD_IN_CX(cpu, "l",      ci->l,      Time_ns);
	CX_ADD_IN_CX(cpu, "k",      ci->k,      bool_t);
	CX_ADD_IN_CX(cpu, "x",      ci->x,      bool_t);
	CX_ADD_IN_CX(qos, "aHeapWords",    ci->aHeapWords, uint32_t);
	env = CX_NEW_IN_CX(p, "b_env");
	CX_ADD_IN_CX(env, "defStackWords", ci->stackWords, uint32_t);
	CX_ADD_IN_CX(env, "pHeapWords",    ci->pHeapWords, uint32_t);
    
	/* priv_root context */
	CX_ADD_IN_CX(env, "priv_root", ci->priv_root, Context_clp);

	/* pdom (not that anyone's interested) */
	CX_ADD_IN_CX(p, "pdom", ci->pdid, ProtectionDomain_ID);
	TRC(eprintf("--- pdom is %x\n", ci->pdid));

	/* Closure_clp that's passable to Exec */
	CX_ADD_IN_CX(p, ci->name, ci->cl, Closure_clp);

    } CATCH_ALL {
	eprintf("WARNING: NemesisPrimal: caught exn while "
		"building prog context for %s\n", ci->name);
    } ENDTRY
}

static void parse_cmdline(Context_clp cx, uint8_t *line)
{
    char *next;
    char *val;
    Type_Any any;
    ANY_DECL(trueany,bool_t,True);

    line=strdup(line); /* Get it out of the kernel state and into somewhere
                          readable */

    if (!*line)
        return;

    next = line;
    while ((line = next) != NULL) {
        if ((next = strchr(line,' ')) != NULL)
            *next++ = 0;

        /* There are two types of option; those that are environment
           variable sets (xxx=yyy) and those that are flags. Flags just
           get put in the namespace as 'True'. */
        if ((val = strchr(line,'=')) != NULL)
        {
            *val++ = 0;
            ANY_INIT(&any, string_t, val);
            Context$Add(cx,line,&any);
        } else {
            Context$Add(cx,line,&trueany);
        }
    }
}

int Main(nexus_ptr_t nexusp, kernel_st *kst, char *k_ident)
{
    Time_clp Time;
    VP_clp vp;
    FramesMod_clp FramesMod;
    FramesF_clp framesF;
    Frames_clp frames;
    ExnSystem_clp ExnSystem;
    ExnSupport_clp exnsys;
    TypeSystemMod_clp tsmod;
    LongCardTblMod_clp lctmod;
    StringTblMod_clp strmod;
    TypeSystemF_clp ts;
    Context_clp root;
    HeapMod_clp HeapMod;
    Heap_clp heap;
    ContextMod_clp ContextMod;
    SAllocMod_clp SAllocMod;
    StretchAllocatorF_clp salloc; 
    StretchAllocator_clp sysalloc; 
    MMUMod_clp MMUMod;
    RamTab_clp rtab; 
    MMU_clp mmu;
    word_t reqd;
    addr_t free;
#ifdef CONFIG_MEMSYS_EXPT
    SDriverMod_clp SDriverMod;
    StretchTblMod_clp StretchTblMod;
    StretchTbl_clp strtab;
#else
    Stretch_clp iostr;
#endif
    DomainMgr_clp dommgr;
    Activation_clp Nemesis;
    Domain_ID did;
    Closure_clp libc_clp;
    BootDomain_Info *NOCLOBBER nemesis_info = NULL;
    ProtectionDomain_ID nemesis_pdid;
    IDCOffer_clp dummy_offer;
    word_t iheap_size;
    addr_t image_start, image_end;
    struct nexus_primal *nexusprimal; 
#ifdef __IX86__
    INFO_PAGE.pvsptr = (addr_t *)&ppvs; 
#endif

    /* intialise the nexus utilities */
    nexusprimal = nexusp.nu_primal;
    init_nexus((void *)nexusprimal, &image_start, &image_end, &iheap_size);
    set_namespace(nexusprimal->namespace, "primal");
    PVS()            = &NemesisPVS;
    Pvs(vp)          = 0;         

    /* Find ourselves a libc */
    if (!(libc_clp = *((Closure_clp *)(lookup_name("libc_clp")))))
	ntsc_halt();
    Pvs(libc) = (addr_t) libc_clp->op;

    /* Get printf() and friends working. We're linked with libdebug,
       so we have access to triv_wr_cl through C linkage. */
    Pvs(out) = Pvs(err) = Pvs(console) = &triv_wr_cl;

    TRC(eprintf("NemesisPrimal: eprintf() works\n"));
    MTRC(eprintf("NemesisPrimal: init heap size is %dK words\n",
		 iheap_size >> 10));
    TRC(eprintf("NemesisPrimal: Pvs(console) = %p\n", Pvs(console)));
    
#ifdef DUMP_PRIMAL
    /* show namespace */
    TRC(eprintf("NemesisPrimal: nexus namespace (at %p):\n", 
		nexusprimal->namespace));
    {
	char *name;
	addr_t val;

	while((name=lookup_next(&val))!=(char *)NULL)
	    TRC(eprintf("  %p  '%s'\n", val, name));
	set_namespace(nexusprimal->namespace, "primal");
    }
#endif

    TRC(eprintf(" + got kst  at %p\n", kst));

    /* Low-level Memory Init */
    TRC(eprintf(" + finding FramesMod:\n"));
    FramesMod = *((FramesMod_clp *) (lookup("FramesModCl")));
    reqd      = FramesMod$Reqd(FramesMod, kst->allmem);

    MTRC(eprintf(" + require %d bytes for FramesMod, + %d for initial heap\n", 
		 reqd, iheap_size*sizeof(word_t)));
    TRC(eprintf(" + finding MMUMod:\n"));
    MMUMod    = *((MMUMod_clp *) (lookup("MMUModCl")));
    MTRC(eprintf(" + creating MMU:\n"));
    mmu = MMUMod$New(MMUMod, kst->allmem, kst->memmap,
		     reqd+(iheap_size*sizeof(word_t)), 
		     &rtab, &free);

    /* frames init */
    MTRC(eprintf(" + creating Frames:\n"));
    framesF = InitPMem(FramesMod, kst->allmem, kst->memmap, rtab, free);
    frames  = (Frames_cl *)framesF;

    /* heap init */
    TRC(eprintf(" + initialising heap.\n"));
    HeapMod = *((HeapMod_clp *) (lookup("HeapModCl")));
    Pvs(heap) = heap = HeapMod$NewRaw(HeapMod, free + reqd,
				      iheap_size*sizeof(word_t));
    TRC(eprintf(" + got heap at %p\n", heap));

    /* Tell the frames mod about our newly created closures */
    FramesMod$Done(FramesMod, framesF, heap);


#ifdef CONFIG_MEMSYS_EXPT
    /* Initialise the VM allocator */
    TRC(eprintf(" + initialising stretch allocator.\n"));
    SAllocMod   = *((SAllocMod_clp *) (lookup("SAllocModCl")));
    salloc      = InitVMem(SAllocMod, kst->memmap, heap, mmu);
    Pvs(salloc) = (StretchAllocator_cl *)salloc;
    TRC(eprintf(" + got stretch allocator.\n"));

    /*
    ** We create a 'special' stretch allocator which produces stretches 
    ** for page tables, protection domains, dcbs, and so forth. 
    ** What 'special' means will vary from architecture to architecture, 
    ** but it will typcially imply at least that the stretches will 
    ** be backed by phyiscal memory on creation. 
    */
    TRC(eprintf(" + creating sysalloc.\n"));
    sysalloc    = StretchAllocatorF$NewNailed(salloc, (Frames_cl *)framesF, 
					      heap); 

    /* Update the mmu with our newly created closures */
    MMUMod$Done(MMUMod, mmu, (Frames_cl *)framesF, heap, sysalloc);

    StretchTblMod = *((StretchTblMod_clp *) (lookup("StretchTblModCl")));
    strtab        = StretchTblMod$New(StretchTblMod, Pvs(heap));

    /* XXX SMH: create an initial default stretch driver */
    SDriverMod    = *((SDriverMod_clp *) (lookup("SDriverModCl")));
    Pvs(sdriver)  = SDriverMod$NewNULL(SDriverMod, heap, strtab);
#else
    /* Initialise the VM allocator */
    TRC(eprintf(" + initialising stretch allocator.\n"));
    SAllocMod   = *((SAllocMod_clp *) (lookup("SAllocModCl")));
    salloc      = InitVMem(SAllocMod, HeapMod, frames, mmu);
    Pvs(salloc) = (StretchAllocator_cl *)salloc;
    TRC(eprintf(" + got stretch allocator.\n"));

    /* Pre memsys expt, sysalloc == salloc */
    sysalloc    = (StretchAllocator_cl *)salloc; 

    /* Update the mmu with our newly created closures */
    MMUMod$Done(MMUMod, mmu, (Frames_cl *)framesF, heap, sysalloc);
#endif


#ifdef DEBUGGING_STRETCHALLOCATOR
    /* Initialise stretch debugging space */
    bzero(STRETCH_DEBUG_TABLE, 4096*sizeof(stretch_info));
#endif DEBUGGING_STRETCHALLOCATOR

#ifndef CONFIG_MEMSYS_EXPT
    /* 
    ** XXX SMH: Temporary Hack. 
    ** A stretch of size 0 causes an allocation of zero bytes so 
    ** nothing is wasted.
    ** In addition, we know (at the moment) that the first stretch id
    ** allocated is in fact zero. 
    ** So we grab this as a pseudo-stretch which 'maps' all additional
    ** memory (viz. anything greater than 64Mb), and in particular 
    ** maps IO addresses. This allows us, by setting protections on 
    ** this stretch for various pdoms, to allow or disallow access
    ** to IO space (as a whole).
    ** Note: this is a temporary thing, since one we have complete
    ** page tables which map the entire address space, we'll be able
    ** to do this in a nicer and more fine-grained manner.
    */
    TRC(eprintf(" + getting stretch 0.\n"));
    iostr= STR_NEW_SALLOC((StretchAllocator_cl *)salloc, 0);
#endif

    /* Create the initial address space; returns a pdom for Nemesis domain */
    TRC(eprintf(" + creating addr space.\n"));
    nemesis_pdid = CreateAddressSpace(frames, mmu, salloc, nexusp);
#ifdef CONFIG_MEMSYS_EXPT
    MapInitialHeap(HeapMod, heap, iheap_size*sizeof(word_t), nemesis_pdid);
#else
    MapInitialHeap(HeapMod, heap, frames, iheap_size*sizeof(word_t),
		   nemesis_pdid);
#endif


    /* Get an Exception System */
    TRC(eprintf( " + Bringing up exceptions\n"));
    ExnSystem = *((ExnSystem_clp *) (lookup("ExnSystemCl")));
    exnsys = (ExnSupport_clp) ExnSystem$New(ExnSystem);
    Pvs(exns)  = exnsys;


    TRC(eprintf( " + Bringing up REAL type system\n"));
#if 0  /* Not sure the SafeLongCardTbl works */
    TRC(eprintf( " +++ getting SafeLongCardTblMod...\n"));
    lctmod = *((LongCardTblMod_clp *) (lookup("SafeLongCardTblModCl")));
#else
    TRC(eprintf( " +++ getting LongCardTblMod...\n"));
    lctmod = *((LongCardTblMod_clp *) (lookup("LongCardTblModCl")));
#endif
    TRC(eprintf( " +++ getting StringTblMod...\n"));
    strmod = *((StringTblMod_clp *) (lookup("StringTblModCl")));
    TRC(eprintf( " +++ getting TypeSystemMod...\n"));
    tsmod = *((TypeSystemMod_clp *) (lookup("TypeSystemModCl")));
    TRC(eprintf( " +++ creating a new TypeSystem...\n"));
    ts= TypeSystemMod$New(tsmod, Pvs(heap), lctmod, strmod);
    TRC(eprintf( " +++ done: ts is at %p\n", ts));
    Pvs(types) = (TypeSystem_clp)ts;
    
    /* Preload any types in the boot image */
    {
	TypeSystemF_IntfInfo *info;
	
	TRC(eprintf(" +++ registering interfaces\n"));
	info = (TypeSystemF_IntfInfo)lookup("Types");
	while(*info) {
	    TypeSystemF$RegisterIntf(ts, *info);
	    info++;
	}
    }

    /* Build initial name space */
    TRC(eprintf( " + Building initial name space: "));
    
    /* Build root context */
    TRC(eprintf( "<root>, "));
    ContextMod = 
      *((ContextMod_clp *) (lookup("ContextModCl")));
    root = ContextMod$NewContext(ContextMod, heap, Pvs(types) );
    Pvs(root)  = root;

    TRC(eprintf( "modules, "));
    { 
	Context_clp mods = ContextMod$NewContext(ContextMod, heap, Pvs(types));
	ANY_DECL(mods_any, Context_clp, mods); 
	char *cur_name;
	addr_t cur_val;

	Context$Add(root,"modules",&mods_any);

	set_namespace(nexusprimal->namespace, "modules");
	while((cur_name=lookup_next(&cur_val))!=NULL) {
	    TRC_CTX(eprintf("\n\tadding %p with name %s", cur_val, cur_name));
	    Context$Add(mods, cur_name, cur_val);
	}
	TRC_CTX(eprintf("\n"));
    }

    TRC(eprintf( "blob, "));
    { 
	Context_clp blobs= ContextMod$NewContext(ContextMod, heap, Pvs(types));
	ANY_DECL(blobs_any, Context_clp, blobs);
	Type_Any b_any;
	char *cur_name;
	addr_t cur_val;

	Context$Add(root, "blob", &blobs_any);

	set_namespace(nexusprimal->namespace, "blob");
	while((cur_name=lookup_next(&cur_val))!=NULL) {
	    TRC_CTX(eprintf("\n\tadding %p with name %s", cur_val, cur_name));
	    ANY_INIT(&b_any, RdWrMod_Blob, cur_val);
	    Context$Add(blobs, cur_name, &b_any);
	}
	TRC_CTX(eprintf("\n"));
    }

    TRC(eprintf( "proc, "));
    { 
	Context_clp proc = 
	    ContextMod$NewContext(ContextMod, heap, Pvs(types));
	Context_clp domains = 
	    ContextMod$NewContext(ContextMod, heap, Pvs(types));
	Context_clp cmdline =
	    ContextMod$NewContext(ContextMod, heap, Pvs(types));
	ANY_DECL(tmpany, Context_clp, proc); 
	ANY_DECL(domany, Context_clp, domains); 
	ANY_DECL(cmdlineany,Context_clp,cmdline);
	ANY_DECL(kstany, addr_t, kst);
	string_t ident=strduph(k_ident, heap);
	ANY_DECL(identany, string_t, ident);
	Context$Add(root, "proc", &tmpany);
	Context$Add(proc, "domains", &domany);
	Context$Add(proc, "kst", &kstany);
        Context$Add(proc, "cmdline", &cmdlineany);
	Context$Add(proc, "k_ident", &identany);
#ifdef INTEL
        parse_cmdline(cmdline, ((kernel_st *)kst)->command_line);
#endif
    }

    TRC(eprintf( "pvs, "));
    {
	Type_Any any;
	if (Context$Get(root,"modules>PvsContext",&any)) {
	    Context$Add(root,"pvs",&any);
	} else {
	    eprintf ("NemesisPrimal: WARNING: >pvs not created\n");
	}
    }

    TRC(eprintf( "symbols, "));
    { 
	Context_clp tmp = ContextMod$NewContext(ContextMod, heap, Pvs(types) );
	ANY_DECL(tmpany,Context_clp,tmp); 
	Context$Add(root,"symbols",&tmpany);
    }

    /* Build system services context */
    TRC(eprintf( "sys, "));
    {
	Context_clp sys = ContextMod$NewContext(ContextMod, heap, Pvs(types) );
	ANY_DECL(sys_any,Context_clp,sys); 
	ANY_DECL(salloc_any, StretchAllocator_clp, salloc); 
	ANY_DECL(sysalloc_any, StretchAllocator_clp, sysalloc); 
	ANY_DECL(ts_any, TypeSystem_clp, Pvs(types));
	ANY_DECL(ffany, FramesF_clp, framesF);
#ifdef CONFIG_MEMSYS_EXPT
	ANY_DECL(strtab_any, StretchTbl_clp, strtab);
#else
	ANY_DECL(iostr_any, Stretch_clp, iostr);
#endif
	Context$Add(root, "sys", &sys_any);
	Context$Add(sys, "StretchAllocator", &salloc_any); 
	Context$Add(sys, "SysAlloc", &sysalloc_any); 
	Context$Add(sys, "TypeSystem", &ts_any);
	Context$Add(sys, "FramesF", &ffany);
#ifdef CONFIG_MEMSYS_EXPT
	Context$Add(sys, "StretchTable", &strtab_any);
#else
	Context$Add(sys, "IOStretch", &iostr_any);
#endif
    }

    /* IDC stub context */
    TRC(eprintf( "stubs, "));
    {
	Closure_clp stubs_register;
	Context_clp stubs = 
	    ContextMod$NewContext(ContextMod, heap, Pvs(types) );

#ifdef CONFIG_STUBGEN_NBF
	Context_clp stubgen = NAME_FIND("modules>StubGenC", Context_clp);
#endif

	/* Create the stubs context */

	CX_ADD("stubs", stubs, Context_clp);

	set_namespace(nexusprimal->namespace, "primal");
	
	/* Dig up the stubs register closure */

	stubs_register = * ((Closure_clp *) lookup("StubsRegisterCl"));
	Closure$Apply(stubs_register);

	TRC_CTX(eprintf("\n"));

#ifdef CONFIG_STUBGEN_NBF
	Context$Remove(root, "stubs");

	CX_ADD("stubs", stubgen, Context_clp);
	
	/* XXX */
	stubgen->st = stubs;
#endif
    }

    /* 
    ** Next is the program information. Need to iterate though them and
    ** get the entry points, values, and namespaces of each of them 
    ** and dump 'em into a context. 
    */

    /* Boot domains/programs context */
    TRC(eprintf( "progs.\n"));
    {
        Context_clp progs= ContextMod$NewContext(ContextMod, heap, Pvs(types));
        ANY_DECL(progs_any,Context_clp,progs);
	ProtectionDomain_ID prog_pdid;
        BootDomain_InfoSeq *boot_seq;
        BootDomain_Info *cur_info;
        nexus_prog *cur_prog;
        char *name;
        Type_Any *any, boot_seq_any, blob_any;

        Context$Add(root,"progs",&progs_any);

        boot_seq = SEQ_NEW(BootDomain_InfoSeq, 0, Pvs(heap));
        ANY_INIT(&boot_seq_any, BootDomain_InfoSeq, boot_seq);

        /* Iterate through progs in nexus and add appropriately... */
        while((cur_prog= find_next_prog())!=NULL) {
            TRC_PRG(eprintf("Found a program, %s,  at %p\n", 
                            cur_prog->program_name, cur_prog));

	    if(!strcmp("Primal", cur_prog->program_name)) {

		TRC_PRG(eprintf(
		    "NemesisPrimal: found params for nemesis domain.\n"));
		
		/* 
		** Allocate some space for the nemesis info stuff, and then 
		** add the info in the 'progs' context s.t. we can get at 
		** it from the Nemesis domain (for Builder$NewThreaded) 
		*/
		nemesis_info= Heap$Malloc(Pvs(heap), sizeof(BootDomain_Info));
		if(!nemesis_info) {
		    eprintf("NemesisPrimal: out of memory. urk. death.\n");
		    ntsc_halt();
		}

		nemesis_info->name       = cur_prog->program_name;
		nemesis_info->stackWords = cur_prog->params.stack;
		nemesis_info->aHeapWords = cur_prog->params.astr;
		nemesis_info->pHeapWords = cur_prog->params.heap;
		nemesis_info->nctxts     = cur_prog->params.nctxts;
		nemesis_info->neps       = cur_prog->params.neps;
		nemesis_info->nframes    = cur_prog->params.nframes;


		/* get the timing parameters from the nbf */
		nemesis_info->p = cur_prog->params.p;
		nemesis_info->s = cur_prog->params.s;
		nemesis_info->l = cur_prog->params.l;
		
		nemesis_info->x = (cur_prog->params.flags & BOOTFLAG_X) ? 
		    True : False;
		nemesis_info->k = (cur_prog->params.flags & BOOTFLAG_K) ? 
		    True : False;
		
		if(!nemesis_info->k)
		    eprintf("WARNING: Nemesis domain has no kernel priv!\n");

		CX_ADD_IN_CX(progs, "Nemesis", nemesis_info, BootDomain_InfoP);
			    
		MapDomain((addr_t)cur_prog->taddr, (addr_t)cur_prog->daddr, 
			  nemesis_pdid);
	    } else {
	    
		cur_info= Heap$Malloc(heap, sizeof(BootDomain_Info));
		
		/* Each program has a closure at start of text... */
		cur_info->cl= (Closure_cl *)cur_prog->taddr;

		/* Create a new pdom for this program */
		prog_pdid      = MMU$NewDomain(mmu);
		cur_info->pdid = prog_pdid;

		MapDomain((addr_t)cur_prog->taddr, (addr_t)cur_prog->daddr, 
			  prog_pdid);
		
		cur_info->name       = cur_prog->program_name;
		cur_info->stackWords = cur_prog->params.stack;
		cur_info->aHeapWords = cur_prog->params.astr;
		cur_info->pHeapWords = cur_prog->params.heap;
		cur_info->nctxts     = cur_prog->params.nctxts;
		cur_info->neps       = cur_prog->params.neps;
		cur_info->nframes    = cur_prog->params.nframes;
		
		/* get the timing parameters from the nbf */
		cur_info->p = cur_prog->params.p;
		cur_info->s = cur_prog->params.s;
		cur_info->l = cur_prog->params.l;
		
		cur_info->x = (cur_prog->params.flags & BOOTFLAG_X) ? 
		    True : False;
		cur_info->k = (cur_prog->params.flags & BOOTFLAG_K) ? 
		    True : False;
		/*
		** We create a new context to hold the stuff passed in in
		** the .nbf as the namespace of this program.
		** This will be later be inserted as the first context of
		** the merged context the domain calls 'root', and hence
		** will override any entries in subsequent contexts. 
		** This allows the use of custom modules (e.g. a debug heap
		** module, a new threads package, etc) for a particular 
		** domain, without affecting any other domains.
		*/
		TRC_PRG(eprintf("Creating program's environment context.\n"));
		cur_info->priv_root= 
		    ContextMod$NewContext(ContextMod, heap, Pvs(types));

		/* XXX what are the other fields of cur_prog->name _for_ ?? */
		set_namespace((namespace_entry *)cur_prog->name->naddr, NULL);
		while((name=lookup_next((addr_t *)&any))!=(char *)NULL) {
		    NOCLOBBER bool_t added= False;

		    /* XXX hack!! */
		    if (!strncmp(name, "blob>", 5))
		    {
			ANY_INIT(&blob_any, RdWrMod_Blob, (addr_t)any);
/*
			TRC_PRG(eprintf("  ++ adding blob '%s':"
					"base=%x, len= %x\n",
					name, any->type, any->val));
					*/
			any = &blob_any;
		    }

		    TRC_PRG(eprintf("  ++ adding '%s': type= %qx, val= %qx\n", 
				    name, any->type, any->val));
		    /* 
		    ** XXX SMH: the below is messy in order to deal with 
		    ** compund names of the form X>Y. If we wanted to deal
		    ** with the more general case (A>B>...>Z), it would be
		    ** even messier.
		    ** Perhaps a 'grow' flag to the Context$Add method would
		    ** be a good move? Wait til after crackle though....
		    ** Also: if want to override something in e.g. >modules>,
		    ** won't work unless copy across rest of modules too.
		    ** If you don't understand why, don't do it. 
		    ** If you do understand why, change Context.c
		    */
		    added  = False;
		    TRY {
			Context$Add(cur_info->priv_root, name, any);
			added= True;
		    } CATCH_Context$NotFound(UNUSED name) {
			TRC_PRG(eprintf(" notfound %s (need new cx)\n", name));
			/* do nothing; added is False */
		    } CATCH_ALL {
			TRC_PRG(eprintf("     (caught exception!)\n"));
			/* ff */
		    } ENDTRY;

		    if(!added) { /* need a subcontext */
			Context_clp new_cx;
			char *first, *rest;
			
			first= strdup(name);
			rest = strchr(first, SEP);
			*rest++ = '\0';
			new_cx= CX_NEW_IN_CX(cur_info->priv_root, first);
			Context$Add(new_cx, rest, any);
		    }
		}

		if (cur_prog->params.flags & BOOTFLAG_B)
		{
		    /* Add to the end of the sequence */
		    SEQ_ADDH(boot_seq, cur_info);
		}
		else
		{
		    /* Not a boot domain, so just dump the info in a context */
		    mk_prog_cx(progs, cur_info);
		}
	    }
	}
	TRC(eprintf(" + Adding boot domain sequence to progs context...\n"));
        Context$Add(progs, "BootDomains", &boot_seq_any);
    }

    /* Find the Virtual Processor module */
    vp = NAME_FIND("modules>VP", VP_clp);
    TRC(eprintf(" + got VP   at %p\n", vp));

    Time = NAME_FIND("modules>Time", Time_clp);
    TRC(eprintf(" + got Time at %p\n", Time));
    Pvs(time)= Time;

    /* IM: init the wall-clock time values of the PIP */
    INFO_PAGE.prev_sched_time = NOW();
    INFO_PAGE.ntp_time  = NOW();
    INFO_PAGE.NTPscaling_factor = 0x0000000100000000LL; /* 1.0 */

    /* DomainMgr */
    TRC(eprintf(" + initialising domain manager.\n"));
    { 
      DomainMgrMod_clp dmm;
      LongCardTblMod_clp LongCardTblMod;
      Type_Any dommgrany;

      dmm = NAME_FIND("modules>DomainMgrMod", DomainMgrMod_clp);
      LongCardTblMod = NAME_FIND("modules>LongCardTblMod", LongCardTblMod_clp);
      dommgr = DomainMgrMod$New(dmm, salloc, LongCardTblMod, 
				framesF, mmu, vp, Time, kst); 

      ANY_INIT(&dommgrany,DomainMgr_clp,dommgr);
      Context$Add(root,"sys>DomainMgr",&dommgrany);
    }

#ifdef CONFIG_MEASURE_TRACE
    TRC(eprintf(" + creating Trace module.\n"));
    {
      Type_Any any;
      Trace_clp t;
      TraceMod_clp tm;

      tm = NAME_FIND("modules>TraceMod", TraceMod_clp);
      t = TraceMod$New(tm);
      kst->trace = t;
      ANY_INIT(&any, Trace_clp, t);
      Context$Add(root, "sys>Trace", &any);
    }
#endif 
    TRC(eprintf(" + creating first domain.\n"));

    /* 
     * The Nemesis domain contains servers which look after all sorts
     * of kernel resources.  It it trusted to play with the kernel
     * state in a safe manner 
     */
    Nemesis = NAME_FIND("modules>Nemesis", Activation_clp);
    Nemesis->st= kst;

    if(nemesis_info == (BootDomain_Info *)NULL) {
	/* shafted */
	eprintf("NemesisPrimal: didn't get nemesis params. Dying.\n");
	ntsc_halt();
    }

    Pvs(vp) = vp = DomainMgr$NewDomain(dommgr, Nemesis, &nemesis_pdid,
				       nemesis_info->nctxts, 
				       nemesis_info->neps,
				       nemesis_info->nframes,
				       0 /* no act str reqd */,
				       nemesis_info->k,
				       "Nemesis",
				       &did,
				       &dummy_offer);
    /* Turn off activations for now */
    VP$ActivationsOff(vp);

#ifdef __IX86__
    /* Frob the pervasives things. This is pretty nasty. */
    RW(vp)->pvs      = &NemesisPVS; 
    INFO_PAGE.pvsptr = &(RW(vp)->pvs);
#endif

    TRC(eprintf(" + did NewDomain.\n"));


    /* register our vp and pdom with the stretch allocators */
    SAllocMod$Done(SAllocMod, salloc, vp, nemesis_pdid);
#ifdef CONFIG_MEMSYS_EXPT
    SAllocMod$Done(SAllocMod, (StretchAllocatorF_cl *)sysalloc, 
		   vp, nemesis_pdid);
#endif    
    DomainMgr$AddContracted(dommgr, did, 
			    nemesis_info->p,
			    nemesis_info->s,
			    nemesis_info->l,
			    nemesis_info->x);

    TRC(eprintf("NemesisPrimal: done DomainMgr_AddContracted.\n"));
    TRC(eprintf("      + domain ID      = %x\n", (word_t)did));
    TRC(eprintf("      + activation clp = %p\n", (addr_t)Nemesis));
    TRC(eprintf("      + vp closure     = %p\n", (addr_t)vp));
    TRC(eprintf("      + rop            = %p\n", (addr_t)RO(vp)));

#if defined(__ALPHA__) || defined(SHARK) || defined(__IX86__) 
    TRC(eprintf("*************** ENGAGING PROTECTION ******************\n"));
    MMU$Engage(mmu, VP$ProtDomID(vp));
#else 
#warning Need some protection for your architecture. 
#endif 



    TRC(eprintf("NemesisPrimal: Activating Nemesis domain\n"));
    ntsc_actdom(RO(vp), Activation_Reason_Allocated);

    /* 
    ** XXX most definitely should not get back here again ;-) 
    */

    eprintf("Nemesis: system death. Time to go to the pub.\n");
    ntsc_halt();

    return 0; /* keep gcc happy */
}
