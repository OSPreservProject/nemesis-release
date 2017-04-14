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
**      mod/nemesis/benchmarks
**
** FUNCTIONAL DESCRIPTION:
** 
**      Standard system benchmarks to be run every time a kernel
**      is booted to ensure that some hosehead hasn't broken things.
**        
**      hosehead: proper noun. A member of the set of people who are less
**      good at systems programming than Paul Barham. Hosehads may often be
**      observed at conferences presenting papers with diagrams containing
**      large fluffy clouds marked "the internet" or "CORBA" partially 
**      obscuring their latest brain child.
** 
** ENVIRONMENT: 
**
**      Where does it do it?
** 
** FILE :	idc.c
** CREATED :	Thu Jun  4 1998
** AUTHOR :	Paul Barham (Paul.Barham@cl.cam.ac.uk)
** ORGANISATION :  
** ID : 	$Id: bm.c 1.2 Tue, 04 May 1999 18:45:26 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <bstring.h>
#include <time.h>
#include <ntsc.h>
#include <ecs.h>
#include <dcb.h>
#include <pip.h>

#include <VP.ih>
#include <Domain.ih>
#include <DomainMgr.ih>
#include <Builder.ih>
#include <BootDomain.ih>
#include <Channel.ih>
#include <Closure.ih>

#include <VPMacros.h>
#include <IDCMacros.h>
#include <IOMacros.h>

#include <salloc.h>

#if defined(__ALPHA__) && !defined(CONFIG_MEMSYS_EXPT)
#define MEMRC_OK ((word_t)PAL_RC_OK)
#endif

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif
#define DB(x)  x


#define UNIMPLEMENTED  printf("%s: UNIMPLEMENTED\n", __FUNCTION__);



#define PRE_ITERS   (1000)      // Number of iterations performed as 'warmup'
#define BM_ITERS    (100000)    // Number of iterations performed as benchmark
#define STR_SIZE    (100 << PAGE_WIDTH) // To be fair to all archs
#define VPN(_vaddr) ((word_t)(_vaddr) >> PAGE_WIDTH)
#define PFN(_paddr) ((word_t)(_paddr) >> FRAME_WIDTH)




static Closure_Apply_fn init; 
static Closure_op ms = { init }; 
static const Closure_cl cl = {&ms, NULL}; 
CL_EXPORT(Closure, Benchmarks, cl); 

static Closure_Apply_fn server; 
static Closure_op serverms = { server }; 
static const Closure_cl servercl = {&serverms, NULL}; 

static Closure_Apply_fn nullrpc; 
static Closure_op rpcms = { nullrpc }; 
static const Closure_cl rpccl = {&rpcms, NULL}; 

/*
  IOTransport$Offer : PROC    [ heap    : IREF Heap,
                    depth   : CARDINAL, 
                    mode    : IO.Mode,
                    alloc   : IO.Kind, 
                    shm     : IOData.Shm,
                    gk      : IREF Gatekeeper, 
                    iocb    : IREF IDCCallback,
                    entry   : IREF IOEntry, 
                    hdl     : DANGEROUS WORD  ] 
          RETURNS [ offer   : IREF IOOffer, 
                    service : IREF IDCService ]
          RAISES Heap.NoMemory;
	  */

typedef struct {
    IDCCallback_cl  cl;
    IO_cl          *io;
    Event_Count     ec;
} state_t;

/*------------------------------------------ IDCCallback Interface ----*/

#include <IDCCallback.ih>

static  IDCCallback_Request_fn          IDCCallback_Request_m;
static  IDCCallback_Bound_fn            IDCCallback_Bound_m;
static  IDCCallback_Closed_fn           IDCCallback_Closed_m;

static  IDCCallback_op  iocb_ms = {
    IDCCallback_Request_m,
    IDCCallback_Bound_m,
    IDCCallback_Closed_m
};

/*---------------------------------------------------- Entry Points ----*/
static bool_t IDCCallback_Request_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer           /* IN */,
        Domain_ID       dom             /* IN */,
        ProtectionDomain_ID     pdid    /* IN */,
        const Binder_Cookies    *clt_cks /* IN */,
        Binder_Cookies  *svr_cks        /* OUT */ )
{
    return True;
}

static bool_t IDCCallback_Bound_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer           /* IN */,
        IDCServerBinding_clp    binding /* IN */,
        Domain_ID       dom             /* IN */,
        ProtectionDomain_ID     pdid    /* IN */,
        const Binder_Cookies    *clt_cks /* IN */,
        Type_Any        *server         /* INOUT */ )
{
    state_t *st = self->st;
    
    st->io      = NARROW(server, IO_clp);
    EC_ADVANCE(st->ec, 1);
    return True;
}

static void IDCCallback_Closed_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer           /* IN */,
        IDCServerBinding_clp    binding /* IN */,
        const Type_Any  *server         /* IN */ )
{
}


static void ioexport(uint8_t id)
{
    IOTransport_clp     iot;
    Type_Any            offer_any;
    IOOffer_clp         offer;
    IDCService_clp      service;
    IOData_Shm         *shm;
    char                io_name[32];
    IO_Rec              rec;
    uint32_t            nrecs;
    word_t              value;

    state_t *st = Heap$Malloc(Pvs(heap), sizeof(*st));
    
    CL_INIT(st->cl, &iocb_ms, st);
    st->ec = EC_NEW();

    iot = NAME_FIND ("modules>IOTransport", IOTransport_clp);
    
    /* XXX SMH: can only cope with one data area in macro */
    shm = SEQ_NEW(IOData_Shm, 1, Pvs(heap));
    SEQ_ELEM(shm, 0).buf.base     = NO_ADDRESS; /* not yet allocated */
    SEQ_ELEM(shm, 0).buf.len      = (0);
    SEQ_ELEM(shm, 0).param.attrs  = 0;
    SEQ_ELEM(shm, 0).param.awidth = PAGE_WIDTH;
    SEQ_ELEM(shm, 0).param.pwidth = PAGE_WIDTH;
    
    offer = IOTransport$Offer (iot, Pvs(heap), 64, IO_Mode_Rx, IO_Kind_Master,
                               shm, Pvs(gkpr), &st->cl, NULL, 0, &service);
    
    ANY_INIT(&offer_any,IDCOffer_clp,offer);
    sprintf(io_name, "svc>nullio%02x\n", id);
    TRC(eprintf("Benchmarks: exporting '%s'\n", io_name));
    Context$Add (Pvs(root), io_name, &offer_any);
    EC_AWAIT(st->ec, 1);
    TRC(eprintf("IO is bound!!\n"));
    while (True) {
	IO$GetPkt(st->io, 1, &rec, FOREVER, &nrecs, &value);
	if(value) IO$PutPkt(st->io, 1, &rec, value, FOREVER);
    }
}

static void server (Closure_clp self)
{
    uint8_t id = (uint8_t)(VP$DomainID(Pvs(vp)) & 0xFF); 
    char idc_name[32];

    sprintf(idc_name, "svc>nullrpc%02x\n", id);
    TRC(eprintf("Benchmarks: exporting '%s'\n", idc_name));
    IDC_EXPORT(idc_name, Closure_clp, &rpccl);
    ioexport(id);
    TRC(eprintf("Benchmarks: done\n"));
}

static void nullrpc (Closure_clp self)
{
    TRC(eprintf("nullrpc: called\n"));
}

static Domain_ID fork(Closure_clp entry, bool_t share_pdom)
{
    NOCLOBBER Context_clp new_root = NAME_FIND("sys>StdRoot", Context_clp);
    StretchAllocator_clp salloc;
    IDCOffer_clp salloc_offer;
    Builder_clp	builder;
    Domain_ID dom;
    BootDomain_Info dummy;
    BootDomain_Info *bdinfo = &dummy;
    VP_clp vp;
    Activation_clp vec;
    DomainMgr_clp dm  = IDC_OPEN("sys>DomainMgrOffer",DomainMgr_clp);


    bzero(bdinfo, sizeof(BootDomain_Info));
    bdinfo->name       = share_pdom ? "Server(same pdom)" : 
	"Server(different pdom)";
    bdinfo->cl         = (Closure_clp)entry;
    bdinfo->pdid       = share_pdom ? VP$ProtDomID(Pvs(vp)) : NULL_PDID;
    bdinfo->stackWords = 0x800;
    bdinfo->aHeapWords = 0x2000;
    bdinfo->pHeapWords = 0x8000;
    bdinfo->nctxts     = 0x20;
    bdinfo->neps       = 0x80;
    bdinfo->nframes    = 0x40;
    bdinfo->p          = SECONDS(1);
    bdinfo->s          = MILLISECS(500);
    bdinfo->l          = SECONDS(1);
    bdinfo->x          = True;
    bdinfo->k          = True;


    vp = DomainMgr$NewDomain(dm, (Activation_cl *)bdinfo->cl, 
			     &bdinfo->pdid, bdinfo->nctxts, 
			     bdinfo->neps, bdinfo->nframes, 
			     bdinfo->aHeapWords*sizeof(word_t), bdinfo->k, 
			     bdinfo->name, &dom, &salloc_offer);

    /* 'Bind' to the salloc - if we are Nemesis this is a no-op since
       we are in the same domain. If we're not Nemesis, this will
       cause a real bind */
	
    salloc = IDC_BIND(salloc_offer, StretchAllocator_clp);
	
    /*
    ** XXX SMH: the below call is likely to change in the near
    ** future to make use of the 'vp' we have created above 
    */
    builder = NAME_FIND("modules>Builder",Builder_clp);
    vec = Builder$NewThreaded (builder, bdinfo->cl,
			       salloc, bdinfo->pdid,
			       RW(vp)->actstr, 
			       NULL); /* XXX pass bdinfo through env context */
    VP$SetActivationVector(vp, vec);

    /* Release the StretchAllocator */
    IDC_CLOSE(salloc_offer);

    DomainMgr$AddContracted (dm, dom, bdinfo->p, bdinfo->s, 
			     bdinfo->l, bdinfo->x);
    return dom;
}

static void kill(Domain_ID dom)
{
    DomainMgr_clp dm = IDC_OPEN("sys>DomainMgrOffer", DomainMgr_clp);

    DomainMgr$Destroy(dm, dom);
    return;
}

static void fail()
{
/*  eprintf("         => this kernel is too sh^H^H poor to live.\n"); */
#if FASCIST
    ntsc_dbgstop(); 
#endif
}


#define BENCHMARK(code)						\
({								\
    for (i = 0; i < PRE_ITERS; i++) {				\
	code;							\
    }								\
    before = NOW();						\
    sched_hb[0] = INFO_PAGE.sheartbeat;				\
    int_hb[0]   = INFO_PAGE.iheartbeat;				\
    patch_hb[0] = INFO_PAGE.pheartbeat;				\
    ntsc_hb[0]  = INFO_PAGE.nheartbeat;				\
    for (i = 0; i < BM_ITERS; i++) {				\
	code;							\
    }								\
    after   = NOW();						\
    sched_hb[1] = INFO_PAGE.sheartbeat;				\
    int_hb[1]   = INFO_PAGE.iheartbeat;				\
    patch_hb[1] = INFO_PAGE.pheartbeat;				\
    ntsc_hb[1]  = INFO_PAGE.nheartbeat;				\
    delta   = (after - before);					\
    avgtime = delta / BM_ITERS;					\
    avgcycs = (uint32_t)((avgtime*1000) / ps_per_cycle);	\
    sched_hb[2] = sched_hb[1] - sched_hb[0];			\
    int_hb[2]   = int_hb[1] - int_hb[0];			\
    patch_hb[2] = patch_hb[1] - patch_hb[0];			\
    ntsc_hb[2]  = ntsc_hb[1] - ntsc_hb[0];			\
})

#define BENCHMARK_ALT(code1,code2)				\
({								\
    for (i = 0; i < PRE_ITERS; i+=2) {				\
	code1;							\
	code2;							\
    }								\
    before = NOW();						\
    sched_hb[0] = INFO_PAGE.sheartbeat;				\
    int_hb[0]   = INFO_PAGE.iheartbeat;				\
    patch_hb[0] = INFO_PAGE.pheartbeat;				\
    ntsc_hb[0]  = INFO_PAGE.nheartbeat;				\
    for (i = 0; i < BM_ITERS; i+=2) {				\
	code1;							\
	code2;							\
    }								\
    after   = NOW();						\
    sched_hb[1] = INFO_PAGE.sheartbeat;				\
    int_hb[1]   = INFO_PAGE.iheartbeat;				\
    patch_hb[1] = INFO_PAGE.pheartbeat;				\
    ntsc_hb[1]  = INFO_PAGE.nheartbeat;				\
    delta   = (after - before);					\
    avgtime = delta / BM_ITERS;					\
    avgcycs = (uint32_t)((avgtime*1000) / ps_per_cycle);	\
    sched_hb[2] = sched_hb[1] - sched_hb[0];			\
    int_hb[2]   = int_hb[1] - int_hb[0];			\
    patch_hb[2] = patch_hb[1] - patch_hb[0];			\
    ntsc_hb[2]  = ntsc_hb[1] - ntsc_hb[0];			\
})


#ifdef __ALPHA__
#define PREAMBLE() {							\
    eprintf("               Time(ns)   Cycles   Sched    IRQ\n");	\
    eprintf("-----------------------------------------------\n");	\
}
#else
#define PREAMBLE() {							\
    eprintf("               Time(ns)   Cycles   Sched    IRQ   "	\
	    "Patch     NTSC\n");					\
    eprintf("--------------------------------------------------"	\
	    "--------------\n");					\
}
#endif


#ifdef __ALPHA__
#define POSTAMBLE() {							\
    eprintf("-----------------------------------------------\n");	\
    eprintf("-----------------------------------------------\n");	\
}
#else
#define POSTAMBLE() {							\
    eprintf("--------------------------------------------------"	\
	    "--------------\n");					\
    eprintf("--------------------------------------------------"	\
	    "--------------\n");					\
}
#endif


#ifdef __ALPHA__
#define DUMP_RESULTS(_str)						\
    eprintf("%s %6d    %6d  %6d %6d\n", (_str), (uint32_t)avgtime,	\
	    (uint32_t)avgcycs, sched_hb[2], int_hb[2]);
#else
#define DUMP_RESULTS(_str)					\
    eprintf("%s %6d    %6d  %6d %6d  %7d %7d\n", (_str),	\
	    (uint32_t)avgtime, (uint32_t)avgcycs, sched_hb[2],	\
	    int_hb[2], patch_hb[2], ntsc_hb[2]);
#endif


   
static void init (Closure_clp self)
{
    Time_ns ps_per_cycle, before, after, delta, avgtime;
    word_t sched_hb[3], int_hb[3], patch_hb[3], ntsc_hb[3];
    uint32_t avgcycs;
    Closure_clp nullrpc;
    char idc_name[32], io_name[32];
    ProtectionDomain_ID pdid;
    Domain_ID svr_dom;
    Stretch_Rights axs[2]; 
    Stretch_Size size; 
    Stretch_clp str; 
    IO_clp nullio;
    IO_Rec rec;
    uint32_t nrecs;
    word_t value;
    Event_Count ec;
    addr_t base; 
    volatile word_t pte; 
    uint8_t svr_id;
    int i;


#if defined(EB164)
    ps_per_cycle = INFO_PAGE.cycle_time; 
#elif defined(SHARK)
    ps_per_cycle = 4286;  /* sharks are at 233 Mhz */
#elif defined(INTEL) 
    ps_per_cycle = INFO_PAGE.cycle_cnt;
#else
    eprintf("Warning: you need to define your pico-seconds per cycle here.\n");
    ps_per_cycle = 5000; /* assume 200 Mhz */
#endif

    eprintf("Running benchmark suite: %d iterations per test (+ %d warmup)\n",
	    BM_ITERS, PRE_ITERS);
    eprintf("At START: shb=%x, ihb=%x, phb=%x, nhb=%x\n", 
	    INFO_PAGE.sheartbeat, 
	    INFO_PAGE.iheartbeat, 
	    INFO_PAGE.pheartbeat, 
	    INFO_PAGE.nheartbeat);


    PREAMBLE();

    TRC(eprintf("Benchmarks: testing empty benchmark loop\n"));
    { 
	volatile word_t foo;
	BENCHMARK(foo);
    }
    DUMP_RESULTS("Empty loop     ");

    TRC(eprintf("Benchmarks: testing Time$Now()\n"));
    BENCHMARK(NOW());
    DUMP_RESULTS("Time$Now       ");
    if (avgtime > 200) fail();

#if (defined( __IX86__) || defined(__ALPHA__))
    BENCHMARK(rpcc());
    DUMP_RESULTS("rpcc()         ");
#endif

    TRC(eprintf("Benchmarks: testing ntsc_yield() [acts on]\n"));
    /* Yield with activation on - this causes a pass through the scheduler, 
       and then up through actf & the thread scheduler and back here */
    BENCHMARK(ntsc_yield());
    DUMP_RESULTS("ntsc_yield(on) ");
    if (avgtime > 20000) fail();

    TRC(eprintf("Benchmarks: testing ntsc_yield() [acts off]\n"));
    VP$ActivationsOff(Pvs(vp));
    BENCHMARK(ntsc_yield());
    VP$ActivationsOn(Pvs(vp));
    DUMP_RESULTS("ntsc_yield(off)");
    if (avgtime > 20000) fail();

    TRC(eprintf("Benchmarks: testing ntsc_send() on bogus channel\n"));
    /* XXX This is a bogus (RX) event channel so that we don't do any damage */
    {
	Channel_TX txep = VP$AllocChannel(Pvs(vp));
	BENCHMARK(ntsc_send(txep, i));
	VP$FreeChannel(Pvs(vp), txep);
    }
    DUMP_RESULTS("ntsc_send      ");
    if (avgtime > 20000) fail();

    TRC(eprintf("Benchmarks: testing Events$Advance()\n"));
    ec = EC_NEW();
    BENCHMARK(Events$Advance(Pvs(evs), ec, 1));
    DUMP_RESULTS("Events$Advance ");
    if (avgtime > 20000) fail();

    /* Perform some inter-domain benchmarks - first off, we use the same 
       protection domain for the 'server' => should get few patch faults 
       on x86 and ARM */
    TRC(eprintf("Forking server domain (same pdom)\n"));
    svr_dom = fork((Closure_clp)&servercl, True);
    svr_id  = (uint8_t)(svr_dom & 0xFF);
    sprintf(idc_name, "svc>nullrpc%02x\n", svr_id);
    sprintf(io_name, "svc>nullio%02x\n", svr_id);
    NAME_AWAIT(idc_name);
    NAME_AWAIT(io_name);

    TRC(eprintf("Benchmarks: binding\n"));
    nullrpc = IDC_OPEN(idc_name, Closure_clp);
	
    TRC(eprintf("Benchmarks: NULL RPC test...\n"));
    BENCHMARK(Closure$Apply(nullrpc));
    DUMP_RESULTS("NULL RPC - 1   ");
    if (avgtime > 50000) fail();

    TRC(eprintf("Benchmarks: binding\n"));
    nullio = IO_OPEN(io_name);
	
    TRC(eprintf("Benchmarks: IO$PutPkt test...\n"));
    BENCHMARK(IO$PutPkt(nullio, 1, &rec, 0, FOREVER));
    DUMP_RESULTS("IO$PutPkt - 1  ");
    if (avgtime > 50000) fail();

    TRC(eprintf("Benchmarks: IO ping test...\n"));
    BENCHMARK(IO$PutPkt(nullio, 1, &rec, 1, FOREVER);
	      IO$GetPkt(nullio, 1, &rec, FOREVER, &nrecs, &value));
    DUMP_RESULTS("IO Ping   - 1  ");


    TRC(eprintf("Benchmarks: IO$QueryGet test...\n"));
    {
	uint32_t nrecs;
	BENCHMARK(IO$QueryGet(nullio, 0, &nrecs));
    }
    DUMP_RESULTS("IO$QueryGet    ");

    TRC(eprintf("Terminating server domain (id %qx)\n", svr_dom));
    kill(svr_dom);

    /* Now perform the inter-domain benchmarks again, but this time 
       using a 'server' domain with its own protection domain => should
       get lots of patch faults (on IDC buffers, etc) on x86 & ARM */
    TRC(eprintf("Forking server domain (own pdom)\n"));
    svr_dom = fork((Closure_clp)&servercl, False);
    svr_id  = (uint8_t)(svr_dom & 0xFF);
    sprintf(idc_name, "svc>nullrpc%02x\n", svr_id);
    sprintf(io_name, "svc>nullio%02x\n", svr_id);
    NAME_AWAIT(idc_name);
    NAME_AWAIT(io_name);

    TRC(eprintf("Benchmarks: binding\n"));
    nullrpc = IDC_OPEN(idc_name, Closure_clp);
	
    TRC(eprintf("Benchmarks: NULL RPC test...\n"));
    BENCHMARK(Closure$Apply(nullrpc));
    DUMP_RESULTS("NULL RPC - 2   ");
    if (avgtime > 50000) fail();

    TRC(eprintf("Benchmarks: binding\n"));
    nullio = IO_OPEN(io_name);
	
    TRC(eprintf("Benchmarks: IO$PutPkt test...\n"));
    BENCHMARK(IO$PutPkt(nullio, 1, &rec, 0, FOREVER));
    DUMP_RESULTS("IO$PutPkt - 2  ");
    if (avgtime > 50000) fail();

    TRC(eprintf("Benchmarks: IO ping test...\n"));
    BENCHMARK(IO$PutPkt(nullio, 1, &rec, 1, FOREVER);
	      IO$GetPkt(nullio, 1, &rec, FOREVER, &nrecs, &value));
    DUMP_RESULTS("IO Ping   - 2  ");

    TRC(eprintf("Terminating server domain (id %qx)\n", svr_dom));
    kill(svr_dom);

    
    /* And now some memory system benchmarks */
    str    = STR_NEW(STR_SIZE);
    pdid   = VP$ProtDomID(Pvs(vp));
    axs[0] = SET_ELEM(Stretch_Right_Read) | SET_ELEM(Stretch_Right_Meta);
    axs[1] = SET_ELEM(Stretch_Right_Read) | SET_ELEM(Stretch_Right_Write) | 
	SET_ELEM(Stretch_Right_Meta);

    BENCHMARK(STR_SETPROT(str, pdid, axs[1]));
    DUMP_RESULTS("STR_SETPROT-1  ");

    BENCHMARK_ALT(STR_SETPROT(str, pdid, axs[0]), 
		  STR_SETPROT(str, pdid, axs[1]));
    DUMP_RESULTS("STR_SETPROT-2  ");

    BENCHMARK(STR_SETGLOBAL(str, axs[1]));
    DUMP_RESULTS("STR_SETGLOBAL-1");

    BENCHMARK_ALT(STR_SETGLOBAL(str, axs[0]), 
		  STR_SETGLOBAL(str, axs[1]));
    DUMP_RESULTS("STR_SETGLOBAL-2");


    base = Stretch$Range(str, &size);
    BENCHMARK(pte = ntsc_trans(VPN(base))); 
    DUMP_RESULTS("ntsc_trans     ");

#ifdef CONFIG_MEMSYS_EXPT
    {
	Frames_clp frames = NAME_FIND("sys>Frames", Frames_clp); 
	addr_t pbase      = Frames$Alloc(frames, FRAME_SIZE, FRAME_WIDTH); 

	BENCHMARK_ALT(ntsc_map(VPN(base), PFN(pbase), BITS(VALID(pte))), 
		      ntsc_map(VPN(base), PFN(pbase), BITS(UNMAP(pte))) ); 
	DUMP_RESULTS("ntsc_[un]map   ");
    }
#endif /* CONFIG_MEMSYS_EXPT */

    POSTAMBLE();

    eprintf("At END: shb=%x, ihb=%x, phb=%x, nhb=%x\n", 
	    INFO_PAGE.sheartbeat, 
	    INFO_PAGE.iheartbeat, 
	    INFO_PAGE.pheartbeat, 
	    INFO_PAGE.nheartbeat);


    /* XXX Don't do this until we have a separate pdom or some bloody 
       trivial reference counting! */ 
#if 0
    DomainMgr$Remove(dm, dom);
    DomainMgr$Destroy(dm, vp);
#endif
}
