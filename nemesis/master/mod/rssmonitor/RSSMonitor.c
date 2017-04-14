/*
*****************************************************************************
**                                                                          *
**  Copyright 1999, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**     mod/rssmonitor/RSSMonitor.c
**
** FUNCTIONAL DESCRIPTION:
**
**     A custom fault handler for couting hits on certain stretches, and
**     an interface for manipulating stretches that are handled.
**
** ENVIRONMENT: 
**
**
**
** ID : $Id: RSSMonitor.c 1.1 Thu, 10 Jun 1999 14:42:18 +0100 dr10009 $
**
**
*/

#include <nemesis.h>
#include <mutex.h>
#include <stdlib.h>
#include <stdio.h>
#include <RSSMonitor.ih>
#include <FaultHandler.ih>
#include <exceptions.h>
#include <ntsc.h>
#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);

static	RSSMonitor_AddStretch_fn 		RSSMonitor_AddStretch_m;
static	RSSMonitor_RemoveStretch_fn 		RSSMonitor_RemoveStretch_m;
static	RSSMonitor_ListStretches_fn 		RSSMonitor_ListStretches_m;
static	RSSMonitor_Sample_fn 		        RSSMonitor_Sample_m;
static	RSSMonitor_SampleAll_fn 		RSSMonitor_SampleAll_m;
static  RSSMonitor_Destroy_fn                    RSSMonitor_Destroy_m;

static	RSSMonitor_op	RSSMonitor_ms = {
  RSSMonitor_AddStretch_m,
  RSSMonitor_RemoveStretch_m,
  RSSMonitor_ListStretches_m,
  RSSMonitor_Sample_m,
  RSSMonitor_SampleAll_m,
  RSSMonitor_Destroy_m
};

typedef struct StretchMonitor {
    struct StretchMonitor *next;
    Stretch_clp stretch;
    addr_t base;
    word_t length;
    uint64_t count;
} *StretchMonitor_p;

typedef struct {
    StretchMonitor_p head;
    mutex_t mu;
    RSSMonitor_cl closure;
    FaultHandler_cl faulthandler;

} RSSMonitor_st ;


#define VPN(x)  ((word_t) x>>PAGE_WIDTH)

static INLINE bool_t do_map(uint32_t vpn, uint32_t pfn, word_t pte)
{
    word_t rc; 

    if((rc = ntsc_map(vpn, pfn, BITS(pte)))) {
        switch(rc) {
        case MEMRC_BADVA:
            eprintf("ntsc_map: bad vpn %lx\n", vpn);
            break;
            
        case MEMRC_NOACCESS:
            eprintf("ntsc_map: cannot map vpn %lx (no meta rights!)\n", vpn);
            break;
            
        case MEMRC_BADPA:
            eprintf("ntsc_map: bad pfn %x\n", pfn);
            eprintf("vpn %lx, pte = %lx\n", vpn, pte); 
            ntsc_dbgstop();
            break;
            
        case MEMRC_NAILED:
            eprintf("ntsc_map: vpn %lx is nailed down!\n", vpn);
            break;
            
        default:
            eprintf("ntsc_map: screwed!!!! (rc=%x)\n", rc);
            ntsc_dbgstop();
            break;
        }
        return False;
    }
    return True; 
}

/* this function is called to reset a stretch, with the mutex already locked */
static void reset_stretch(RSSMonitor_st *st, StretchMonitor_p ptr) {
  word_t vpnbase, vpnlimit, i;
  word_t bits;
  vpnbase = VPN((ptr->base));
  vpnlimit = VPN((ptr->base + ptr->length));
  for (i=vpnbase; i<vpnlimit; i++) {
      bits = ntsc_trans(i);
      TRC(eprintf("Resetting VPN %x PFN %x PTE %x\n", i, PFN_PTE(bits), bits));
      do_map(i, PFN_PTE(bits), bits & (~ PDE_M_VALID));
  }
  ptr->count = 0L;
}    
/* this function is called to do a sample, with the mutex already locked */

static uint64_t sample(RSSMonitor_st *st, StretchMonitor_p ptr, RSSMonitor_Action action) {
    uint64_t result;
    result = 0L;
    switch (action) {
    case RSSMonitor_Action_SIZE:
	result = (uint64_t) (ptr->length);
	break;
    case RSSMonitor_Action_BASE:
	result = (uint64_t) ((addr_t)( ptr->base));
	break;
    case RSSMonitor_Action_SAMPLE_AND_RESET:
	result = ptr->count;
	reset_stretch(st,ptr);
	break;
    case RSSMonitor_Action_SAMPLE:
	result = ptr->count;
	break;
    case RSSMonitor_Action_RESET:
	reset_stretch(st,ptr);
	break;
    }
    if (action == RSSMonitor_Action_RESET || action == RSSMonitor_Action_SAMPLE_AND_RESET) {
	reset_stretch(st, ptr);
    }
    return result;
}

/*---------------------------------------------------- Entry Points ----*/


static bool_t RSSMonitor_AddStretch_m (
	RSSMonitor_cl	*self,
	Stretch_clp	stretch	/* IN */ )
{
  RSSMonitor_st	*st = self->st;
  StretchMonitor_p ptr, walker;
  int happy = 1;



  ptr = calloc(sizeof(*ptr), 1);
  if (!ptr) {
      fprintf(stderr,"no memory left\n");
      return 1;
  }
	  
  ptr->next = st->head;
  ptr->stretch = stretch;
  ptr->base = Stretch$Range(stretch, &(ptr->length));

  

  MU_LOCK(&(st->mu));
  walker = st->head;
  while (walker) {
      if (walker->stretch == stretch) {
	  fprintf(stderr,"Stretch registered wtih RSSMonitor twice\n");
	  happy = 1;
      }
      walker = walker->next;
  }
  if (happy)  st->head = ptr;
  MU_RELEASE(&(st->mu));

  reset_stretch(st, ptr);
  return happy;
}


/* remove stretch is untested */
static bool_t RSSMonitor_RemoveStretch_m (
	RSSMonitor_cl	*self,
	Stretch_clp	stretch	/* IN */ )
{
  RSSMonitor_st	*st = self->st;
  StretchMonitor_p walker, *prev, next;
  int donework = 0;
  MU_LOCK(&(st->mu));
  walker = st->head;
  prev = &(st->head);
  while (walker) {
      next = walker->next;
      if (walker->stretch == stretch) {
	  (*prev)->next = next;
	  free(walker);
	  donework = 1;
      }
      prev = &(walker->next);
      walker = next;
  }
  MU_RELEASE(&(st->mu));
  return donework;
}

static RSSMonitor_StretchSequence *RSSMonitor_ListStretches_m (
	RSSMonitor_cl	*self )
{
  RSSMonitor_st	*st = self->st;
  int length, i;
  StretchMonitor_p walker;
  RSSMonitor_StretchSequence *ptr;
  ptr = calloc(sizeof(*ptr), 1);
  MU_LOCK(&(st->mu));
  walker = st->head;
  length = 0;
  while (walker) {
      walker=walker->next;
      length++;
  }


  if (ptr) {
      SEQ_INIT(ptr, length, Pvs(heap));
      i = 0;
      walker = st->head;
      while (walker) {
	  SEQ_ELEM(ptr, i) = walker->stretch;
	  walker=walker->next;
	  i++;
      }
  }
  MU_RELEASE(&(st->mu));

  /* XXX: what if the calloc failed? */
  return ptr;
}



static uint64_t RSSMonitor_Sample_m (
	RSSMonitor_cl	*self,
	Stretch_clp	stretch	/* IN */,
	RSSMonitor_Action	mode	/* IN */
   /* RETURNS */,
	bool_t	*success )
{
  RSSMonitor_st	*st = self->st;
  StretchMonitor_p walker;
  uint64_t result;
  int done;
  result = 0;
  done = 0;
  MU_LOCK(&(st->mu));

  walker = st->head;
  while (walker && !done) {
      
      if (walker->stretch == stretch) {
	  result = sample(st, walker, mode);
	  done = 1;
      }
  }
  MU_RELEASE(&(st->mu));
  if (done) {
      *success = 1;
  } else {
      *success = 0;
  }

  printf("Stretch %p action %d %s result %qx\n", stretch,
	 mode, *success ? "success" : "failure", result);
  return result;
}

static RSSMonitor_LongCardinalSequence *RSSMonitor_SampleAll_m (
	RSSMonitor_cl	*self,
	RSSMonitor_Action	mode	/* IN */ )
{
  RSSMonitor_st	*st = self->st;
  StretchMonitor_p walker;
  int i, length;
  RSSMonitor_LongCardinalSequence *ptr;

  ptr = calloc(sizeof(*ptr),1);
  MU_LOCK(&(st->mu));
  walker = st->head;
  length = 0;
  while (walker) { 
      walker=walker->next;
      length++;
  }
  
  if (ptr) {
      SEQ_INIT(ptr, length, Pvs(heap));
      i = 0;
      walker = st->head;
      while (walker) {
	  SEQ_ELEM(ptr, i) = sample(st, walker, mode);
	  i++;
	  walker = walker->next;
      }
  }

  MU_RELEASE(&(st->mu));
  /* XXX: what if teh called failed */
  return ptr;
}

static void RSSMonitor_Destroy_m (
    RSSMonitor_cl *self ) {
    UNIMPLEMENTED;
}



/*
 * End 
 */

static	FaultHandler_Handle_fn 		FaultHandler_Handle_m;

static	FaultHandler_op	FaultHandler_ms = {
  FaultHandler_Handle_m
};

/*---------------------------------------------------- Entry Points ----*/

static bool_t FaultHandler_Handle_m (
	FaultHandler_cl	*self,
	Stretch_clp	str	/* IN */,
	addr_t	vaddr	/* IN */,
	Mem_Fault	reason	/* IN */ )
{
  RSSMonitor_st	*st = self->st;
  StretchMonitor_p walker;
  word_t bits;
  bool_t success;

  walker = st->head;
  success = False;
  while (walker && !success) {
      if (walker->stretch == str) {
	  word_t pte, pfn;
	  walker->count++;
	  
	  bits = ntsc_trans(VPN(vaddr));
	  pfn = PFN_PTE(bits);
	  TRC(eprintf("vaddr %p VPN %x PFN %x PTE %x->%x\n", vaddr, VPN(vaddr), pfn,bits, bits | (PTE_M_VALID)));
	  if (do_map(VPN(vaddr), PFN_PTE(bits), bits | (PTE_M_VALID))) {
	      success = True;
	      TRC(eprintf("Handled page fault on stretch %x vaddr %x reason %d; returning True\n", str, vaddr, reason));

	  }
      }
      walker = walker->next;
  }
  return success;
}

/*
 * End 
 */
RSSMonitor_clp create(bool_t export) {
    RSSMonitor_st *st;
    
    st = calloc(sizeof(*st), 1);
    st->head = NULL;
    MU_INIT(&(st->mu));
    st->faulthandler.op = &FaultHandler_ms;
    st->faulthandler.st = st;
    st->closure.op = &RSSMonitor_ms;
    st->closure.st = st;
    StretchDriver$AddHandler(Pvs(sdriver), Mem_Fault_PAGE, &(st->faulthandler)); 
    return &(st->closure);
}
