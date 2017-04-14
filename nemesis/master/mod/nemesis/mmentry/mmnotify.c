/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/nemesis/mmnotify/mmnotify.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      An implementation of MMNotify: special ChannelNotify 
**      handler for memory management events.
** 
** ENVIRONMENT: 
**
**      User-space, within activation handler.
**
*/
#include <nemesis.h>
#include <exceptions.h>

#include "mm_st.h"

// #define MMNOTIFY_TRACE
#ifdef MMNOTIFY_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x)
#endif



/*
 * Create an MMNotify
 */
#include <MMNotifyMod.ih>
static MMNotifyMod_New_fn New_m;
static MMNotifyMod_op m_op = { New_m };
static const MMNotifyMod_cl m_cl = { &m_op, NULL };
CL_EXPORT (MMNotifyMod, MMNotifyMod, m_cl);



/*
 * MMNotify interface operations 
 */

static ChainedHandler_SetLink_fn SetLink_m;
static ChannelNotify_Notify_fn  Notify_m;
static MMNotify_SetMMEntry_fn   SetMMEntry_m;
static MMNotify_op nfy_op = { 
    SetLink_m,
    Notify_m,
    SetMMEntry_m,
};


typedef struct _MMNotify_st {
    
    /* Closures */
    struct _MMNotify_cl notify_cl;

    /* Useful handles */
    VP_clp	        vp;
    Heap_clp            heap;
    StretchTbl_clp      strtab;

    /* Filled in by SetMMEntry_m */
    MMEntry_clp         mmentry;

} MMNotify_st;



/*--------Build-an-entry--------------------------------------------*/

static MMNotify_clp New_m(MMNotifyMod_clp self, VP_clp vp, 
			  Heap_clp heap, StretchTbl_clp strtab)
{

    MMNotify_st *st;
    word_t size = sizeof(*st);

    /* Allocate structure */
    if ( !(st = Heap$Malloc(heap, size)) ) {
	DB(eprintf("MMNotifyMod$New: Allocation failed.\n"));
	RAISE_Heap$NoMemory();
    }

    CL_INIT (st->notify_cl, &nfy_op, st);
	
    st->vp      = vp;
    st->heap    = heap;
    st->strtab  = strtab;
    st->mmentry = NULL;       /* Filled in by SetMMEntry_m */

    return &(st->notify_cl);
}



/*--------MMNotify-operations----------------------------------------*/

const char *const reasons[] = {
    "Translation Not Valid", 
    "Unaligned Access", 
    "Access Violation", 
    "Instruction Fault", 
    "FOE", 
    "FOW", 
    "FOR", 
    "Page Fault",
    "Breakpoint", 
};


#ifdef __ALPHA__    
static const char *const gp_reg_name[] = {
    "v0 ", "t0 ", "t1 ", "t2 ", "t3 ", "t4 ", "t5 ", "t6 ",
    "t7 ", "s0 ", "s1 ", "s2 ", "s3 ", "s4 ", "pvs", "FP ",
    "a0 ", "a1 ", "a2 ", "a3 ", "a4 ", "a5 ", "t8 ", "t9 ",
    "t10", "t11", "ra ", "pv ", "at ", "GP ", "SP ", "0  " };
#endif

#ifdef __ARM__
static const char *const gp_reg_name[] = {
    "r0 ", "r1 ", "r2 ", "r3 ", "r4 ", "r5 ", "r6 ", "r7 ",
    "r8 ", "r9 ", "pvs", "fp ", "ip ", "sp ", "lr ", "pc " 
};
#endif


static void cx_dump (context_t *cx)
{
#ifdef __ALPHA__    
    uint32_t      i;

    for (i = 0; i < 16; i++)
    {
	eprintf("%s: %p %s: %p\n", 
		 gp_reg_name[i],  cx->gpreg[i],
		 gp_reg_name[i+16], (i == 16) ? 0 : cx->gpreg[i+16]);
    }
    eprintf("PC : %p  PS : %p\n", cx->r_pc, cx->r_ps);
#endif
#ifdef __ARM__    
    uint32_t      i;

    for (i = 0; i < 8; i++)
	eprintf("%s: %p  %s: %p\n", gp_reg_name[i],  cx->reg[i],
		 gp_reg_name[i+8], cx->reg[i+8]);
#endif
#ifdef __IX86__
    eprintf("eax=%p  ebx=%p  ecx=%p  edx=%p\nesi=%p  edi=%p  ebp=%p esp=%p\n",
	    cx->gpreg[r_eax], cx->gpreg[r_ebx], cx->gpreg[r_ecx],
	    cx->gpreg[r_edx], cx->gpreg[r_esi], cx->gpreg[r_edi],
	    cx->gpreg[r_ebp], cx->esp);
    eprintf("eip=%p  flags=%p\n",  cx->eip, cx->eflag);
#endif
    return;
}

static void SetLink_m (ChainedHandler_clp self, 
		       ChainedHandler_Position pos, 
		       ChainedHandler_clp link) {
    
    /* XXX There should never be anyone else on this endpoint, should
       there? */
    ntsc_dbgstop();
}

static void Notify_m(ChannelNotify_clp  self, 
		     Channel_Endpoint   chan,
		     Channel_EPType     type,
		     Event_Val          val, 
		     Channel_State      state )
{
    MMNotify_st *st  = self->st;
    VP_cl      *vpp = st->vp;
    dcb_ro_t   *rop = RO(vpp);
    dcb_rw_t   *rwp = RW(vpp);
    Stretch_clp str = rwp->mm_str;
#ifdef CONFIG_MEMSYS_EXPT
    StretchDriver_clp sdriver  = NULL;
    StretchDriver_Result result;
    MMEntry_Info info; 
    uint32_t pw; 
#endif


    TRC(eprintf("*** MMNotify$Notify (D%x): code=%x, va=%p, str=%p\n", 
	    (uint32_t)(rop->id & 0xFFFF), rwp->mm_code, rwp->mm_va, str));
    /* Acking the event is a good plan */
    VP$Ack(vpp, chan, val);


    switch(rwp->mm_code) {
    case Mem_Fault_PAGE:
	
#ifndef CONFIG_MEMSYS_EXPT
	{
	    Stretch_Size sz; 
	    addr_t saddr; 
	    /* XXX SMH: For now, we don't deal with these */
	    eprintf("*** MMNotify$Notify (D%x): code=%x, va=%p, str=%p\n", 
		    (uint32_t)(rop->id & 0xFFFF), rwp->mm_code, 
		    rwp->mm_va, str);
	    saddr = Stretch$Info(str, &sz);
	    eprintf("Stretch range is  [%p,%p)\n", saddr, saddr + sz);
	    eprintf("Dumping faulting context:\n");
	    cx_dump(&(rop->ctxts[rwp->actctx]));
	    eprintf("Halting for now.\n");
	    ntsc_dbgstop();
	    break;
	}
#else
	/* try to map this right here (i.e with activations off) */
	if(!StretchTbl$Get(st->strtab, str, &pw, &sdriver)) {
	    Stretch_Size sz; 
	    addr_t saddr; 

	    eprintf("*** MMNotify$Notify (D%x) '%s' on va %p (stretch=%p)\n", 
		    (uint32_t)(rop->id & 0xFFFF), 
		    reasons[rwp->mm_code], rwp->mm_va, str);
	    eprintf("%qx: failed to find stretch %p in my strtab!\n", 
		    rop->id, str);
	    saddr = Stretch$Info(str, &sz);
	    eprintf("Stretch range is  [%p,%p)\n", saddr, saddr + sz);
	    eprintf("Dumping faulting context:\n");
	    cx_dump(&(rop->ctxts[rwp->actctx]));
	    eprintf("Halting for now.\n");
	    ntsc_dbgstop();
	}
	result = StretchDriver$Map(sdriver, str, rwp->mm_va);
	switch(result) {
	case StretchDriver_Result_Success:
	    return;
	    break; 
	    
	case StretchDriver_Result_Retry:
	    /* need to retry later (i.e. with activations on) */
	    if(!st->mmentry) {
		eprintf("MMNotify$Notify (d%d): cannot RETRY (no mmentry)", 
			rop->id & 0xFF);
		eprintf(" +++ st at %p\n", st);
		ntsc_dbgstop();
	    }

	    info.code    = rwp->mm_code;
	    info.vaddr   = rwp->mm_va;
	    info.str     = str;
	    info.sdriver = sdriver; 
	    info.context = &(rop->ctxts[rwp->actctx]); 

	    if(!MMEntry$Satisfy(st->mmentry, &info)) {
		eprintf("MMNotify$Notify: MMEntry$Satisfy() failed!\n");
		ntsc_dbgstop();
	    }
	    return; 
	    break;

	case StretchDriver_Result_Failure:
	default:
	    eprintf("MMNotify$Notify: Domain %qx failed to map vaddr %p\n", 
		    rop->id, rwp->mm_va);
	    {
		Stretch_Size sz; 
		addr_t saddr; 
		
		saddr = Stretch$Info(str, &sz);
		eprintf("Stretch range is  [%p,%p)\n", saddr, saddr + sz);
		eprintf("Dumping faulting context:\n");
		cx_dump(&(rop->ctxts[rwp->actctx]));
	    }
	    eprintf("Halting for now.\n");
	    ntsc_dbgstop();
	    break; 
	}
	break;
#endif

    CASE_UNRECOVERABLE:
#ifndef CONFIG_MEMSYS_EXPT
	/* XXX SMH: For now, we don't deal with these */
	eprintf("*** MMNotify$Notify (D%x): code=%x, va=%p, str=%p\n", 
		(uint32_t)(rop->id & 0xFFFF), rwp->mm_code, rwp->mm_va, str);
	cx_dump(&(rop->ctxts[rwp->actctx]));
	eprintf("Urk! Unrecoverable fault is bad news.\n");
	ntsc_dbgstop();
#else
	
	/* Try to handle it right here (i.e with activations off) */
	if(!StretchTbl$Get(st->strtab, str, &pw, &sdriver)) {
	    Stretch_Size sz; 
	    addr_t saddr; 
	    
	    eprintf("*** MMNotify$Notify (D%x) '%s' on va %p (stretch=%p)\n", 
		    (uint32_t)(rop->id & 0xFFFF), 
		    reasons[rwp->mm_code], rwp->mm_va, str);
	    eprintf("%qx: failed to find stretch %p in my strtab!\n", 
		    rop->id, str);
	    eprintf("Dumping faulting context:\n");
	    cx_dump(&(rop->ctxts[rwp->actctx]));
	    eprintf("Halting for now.\n");
	    ntsc_dbgstop();
	}
	result = StretchDriver$Fault(sdriver, str, rwp->mm_va, rwp->mm_code);
	switch(result) {
	case StretchDriver_Result_Success:
	    return;
	    break; 
	    
	case StretchDriver_Result_Failure:
	default:
	    eprintf("MMNotify$Notify: Domain %qx failed to handle %s " 
		    "fault on vaddr %p\n", 
		    reasons[rwp->mm_code], rop->id, rwp->mm_va);
	    {
		Stretch_Size sz; 
		addr_t saddr; 
		
		saddr = Stretch$Info(str, &sz);
		eprintf("Stretch range is  [%p,%p)\n", saddr, saddr + sz);
		eprintf("Dumping faulting context:\n");
		cx_dump(&(rop->ctxts[rwp->actctx]));
	    }
	    eprintf("Halting for now.\n");
	    ntsc_dbgstop();
	    break; 
	
	case StretchDriver_Result_Retry:
	    /* need to retry later (i.e. with activations on) */
	    break; 
	}

	/* Here, we're going to need a thread to help us out */
	if(st->mmentry == NULL) {
	    eprintf("*** MMNotify$Notify (D%x) '%s': code=%x va=%p str=%p\n", 
		    (uint32_t)(rop->id & 0xFFFF), reasons[rwp->mm_code],
		    rwp->mm_code, rwp->mm_va, str);
	    cx_dump(&(rop->ctxts[rwp->actctx]));
	    eprintf("MMNotify$Notify: cannot handle unrecoverable fault "
		    "[no mmentry].\n");
	    ntsc_dbgstop();
	}
	
	/* mark as an unrecoverable fault & work out the context */
	info.code    = rwp->mm_code;
	info.vaddr   = rwp->mm_va;
	info.str     = str;
	info.sdriver = sdriver; 
	info.context = &(rop->ctxts[rwp->actctx]); 
	if(!MMEntry$Satisfy(st->mmentry, &info)) {
	    eprintf("MMNotify$Notify: MMEntry$Satisfy() failed!\n");
	    ntsc_dbgstop();
	}
#endif
	return;
	break;

    CASE_RECOVERABLE:
	/* Have some other sort of fault */
#ifndef CONFIG_MEMSYS_EXPT
	/* XXX SMH: For now, we don't deal with these */
	eprintf("*** MMNotify$Notify (D%x): code=%x, va=%p, str=%p\n", 
		(uint32_t)(rop->id & 0xFFFF), rwp->mm_code, rwp->mm_va, str);
	eprintf("Urk! Recoverable fault is bad news.\n");
	ntsc_dbgstop();
#else
	/* try to map this right here (i.e with activations off) */
	if(!StretchTbl$Get(st->strtab, str, &pw, &sdriver)) {
	    Stretch_Size sz; 
	    addr_t saddr; 

	    eprintf("*** MMNotify$Notify (D%x) '%s' on va %p (stretch=%p)\n", 
		    (uint32_t)(rop->id & 0xFFFF), 
		    reasons[rwp->mm_code], rwp->mm_va, str);
	    eprintf("%qx: failed to find stretch %p in my strtab!\n", 
		    rop->id, str);
	    saddr = Stretch$Info(str, &sz);
	    eprintf("Stretch range is  [%p,%p)\n", saddr, saddr + sz);
	    eprintf("Dumping faulting context:\n");
	    cx_dump(&(rop->ctxts[rwp->actctx]));
	    eprintf("Halting for now.\n");
	    ntsc_dbgstop();
	}
	result = StretchDriver$Fault(sdriver, str, rwp->mm_va, rwp->mm_code);
	return; /* XXX should check result */
#endif
	break;

    default:
	/* Urk! Not good. */
	eprintf("MMNotify$Notify: got unhandled fault code 0x%x\n", 
		rwp->mm_code);
	ntsc_dbgstop();
	break;
    }

/* NOTREACHED */
    ntsc_dbgstop();
}



static void SetMMEntry_m(MMNotify_cl *self,  MMEntry_clp mme)
{
    MMNotify_st *st  = self->st;

    TRC(eprintf("MMNotify$SetMMEntry: self=%p, mmentry=%p\n", self, mme));
    st->mmentry = mme;
    return;
}
