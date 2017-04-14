/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      ix86/dcb.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**	Intel Nemesis Domain Control Blocks
** 
** ID : $Id: dcb.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
*/

/**********************************************************************

 * NB this file is used as source for dcb.def.h (XXX find real name);
 * comments of the form <--- and --->DEFINE_NAME indicate which
 * members of structures should be included in the automatically
 * generated file.

 Structure of a Nemesis DCB:

---->+------------------------------------------------+	0x0000
     | 	       	       	       	       	       	      |	             
     |  dcb_ro: read-only to the user domain.         |	             
     |                                                |	             
     +------------------------------------------------+	             
     |  VP closure                                    |
     +------------------------------------------------+	             
     |            <padding>                           |	             
     +------------------------------------------------+ 0x2000 (page) XXX
     |                                                |       
     |  dcb_rw: read/writeable by user domain         |
     |                                                |
     +------------------------------------------------+
     |                                                |
     |  Array of context slots                        |
     |						      |
     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     |  					      |
     +------------------------------------------------+

**********************************************************************/


#ifndef _dcb_h_ 
#define _dcb_h_ 
 
#include <kernel_config.h> 
 
#ifdef __LANGUAGE_C
typedef struct _dcb_ro 	dcb_ro_t; 
typedef struct _dcb_rw 	dcb_rw_t; 
#include <nemesis.h> 
#endif /* __LANGUAGE_C__ */


/*------------------------------------------------------------------------*/

/* 
 * Events : how channels are represented in the DCB
 */

#ifdef __LANGUAGE_C__

#include <Channel.ih>
#include <Event.ih>

/* The read-only state of an event channel consists of peer 		*/
/* information, plus current value and type. As far as the kernel is 	*/
/* concerned, the channel is connected iff the peer information is	*/
/* non-null. Similarly, the type field is only valid if the channel is  */
/* connected. The NTSC will refuse to send on a channel which is not 	*/
/* connected, or is not of type RX.					*/

typedef struct ep_ro_t ep_ro_t;
struct ep_ro_t {
    /* Peer information	*/
    dcb_ro_t            *peer_dcb;	/* NULL => not connected.	*/
    Channel_Endpoint   	 peer_ep;	

    /* Read-only state	*/
    Event_Val 	     	 val;		/* current value		*/
    Channel_EPType     	 type;	 	/* RX or TX			*/
};

/* The read-write state of an event channel consists of the state 	*/
/* value, and the acknowledge count for receive events. The state 	*/
/* value does not literally reqpresent the channel state: it should 	*/
/* be read in conjunction with the peer_dcb field. End-points in state 	*/
/* Free can use the ack field as a free list link.			*/

typedef struct ep_rw_t ep_rw_t;
struct ep_rw_t {
    Event_Val		 ack;		/* Acknowledge count for rx	*/
    Channel_State	 state;		/* User-level state		*/
};

#endif /* __LANGUAGE_C__ */

#define EPRW_Q_ACK	0x00
#define EPRW_Q_FREE	0x00
#define EPRW_Q_STATE	0x08

#define EPRW_V_SIZE	0x04		/* EPRW is 16 bytes long	*/

/*------------------------------------------------------------------------*/


/*------------------------------------------------------------------------*/

/* 
 * DCB_RO : Portion of domain control block that is read-only
 *          to the user.
 */

#include <context.h>

#ifdef __LANGUAGE_C__

#include <Domain.ih>
#include <Frames.ih>
#include <VP.ih>
#include <ProtectionDomain.ih>
#include <Stretch.ih>
#include <StretchAllocator.ih>
#include <IDC.ih>
#include <Binder.ih>

#include <frames.h>    /* for flink_t, etc */

typedef struct dcb_link_t dcb_link_t;
struct dcb_link_t {
    dcb_link_t	*next;
    dcb_link_t	*prev;
    dcb_ro_t	*dcb;
};


typedef struct dcb_event {
    Channel_TX ep;
    Event_Val val;
} dcb_event;

#define NUM_DCB_EVENTS 64


/* SDom stuff is defined in sdom.h; only things like, e.g. scheduler 
   actually require to know what it looks like... */
typedef struct _SDom_t SDom_t;

struct _dcb_ro /*<---*/ {

    /* Context-switch and scheduler info	*/
    Domain_ID	id;		/* domain id			*/
    uint32_t 	psmask /*--->DCB_L_PSMASK*/; /* Privilege bitmask */
    word_t	asn;		/* Address space no (not used)  */
    uint32_t	pcc;		/* cycle counter		*/
    uint32_t	ps;		/* Current privilege bitmask    */

    word_t	fen /*--->DCB_L_FEN*/; 	/* Floating-point enable     */
    SDom_t      *sdomptr;	/* Pointer to scheduling domain      */
    word_t 	pdbase;		/* Pointer to protection domain base */
    word_t	schst;		/* scheduler state 		     */

    /* Configuration information	*/
    uint32_t	num_eps /*--->DCB_L_NUMEPS*/; /* How many channel endpoints */
    uint32_t	ep_mask;	/* mask for same		*/      

    /* Events, etc. */
    ep_ro_t     * epros;	/* Read-only endpoint structs	*/
    ep_rw_t     * eprws;	/* Read-write endpoint structs	*/ 
    Channel_EP  * ep_fifo;	/* Event fifo pointer		*/

    /* Other pointers */
    dcb_rw_t    * dcbrw /*--->DCB_A_DCBRW*/; /* ptr to other half of DCB */
    context_t   * ctxts /*--->DCB_A_CTXTS*/; /* base of context array    */

    /* Context information */
    uint32_t	  num_ctxts /*--->DCB_L_NUMCTXTS*/; /* Number of ctxt slots */
    uint32_t	  ctxts_mask;	/* Mask for same		*/
  
    /* Where the two stretches representing the DCB are 		*/
    Stretch_clp   rostretch;	/* Dcb_Ro stretch		*/
    Stretch_clp   rwstretch;	/* Dcb_Rw stretch		*/

    /* VP closure */
    struct _VP_cl vp_cl;		/* VP closure: st == &dcb_rw	*/

    uint32_t      minpfn;       /* Lower bounds limit on ramtab            */
    uint32_t      maxpfn;       /* Upper bounds limit on ramtab            */
    void         *ramtab;       /* Array of global frame info (needs cast) */
    flink_t       finfo;        /* Linked list of client frame info        */

    /* Links for domain manager	*/
    dcb_link_t	  link;

    /* Binder information */
    IDCOffer_clp  binder_offer;	/* To bind to the binder	 */
    word_t        binder_ro[256];

    IDCOffer_clp  dmgr_offer;	/* To bind to the DomainMgr      */

    StretchAllocator_clp salloc;/* The domain's default          */
    IDCOffer_clp  salloc_offer;	/* To bind to StretchAllocator   */

    Frames_clp    frames;       /* Allocate/Free physical memory */
    IDCOffer_clp  frames_offer; /* To bind to the above          */

    ProtectionDomain_ID pdid;    /* Identifier of our protection domain */

    volatile word_t  outevs_processed; /* State of outoing event fifo */

};

#define DCB_EPRO(_rop, _ep)	(_rop->epros + _ep)
#define DCB_EPRW(_rop, _ep)	(_rop->eprws + _ep)

#endif /* C */

/*
 * DCB Schst Enumeration: for use by 
 */
#define DCBST_K_RUN      0x0000  /* Domain is runnable           */
#define DCBST_K_WAIT     0x0001  /* Domain is waiting for CPU    */
#define DCBST_K_BLOCK    0x0002  /* Domain has requested a block */
#define DCBST_K_RFABLOCK 0x0003  /* Domain has requested an RFA block */
#define DCBST_K_YIELD    0x0004  /* Domain has requested a yield */
#define DCBST_K_BLCKD    0x0005  /* Domain is blocked.           */
#define DCBST_K_WAKING   0x0006  /* Domain is marked as waking   */
#define DCBST_K_STOPPED  0x0007  
#define DCBST_K_DYING    0x0008  /* Domain is dying              */

/*
 * XXX PSMASK values
 */
#define PS_KERNEL_PRIV 0
#define PS_IO_PRIV 1
#define PSMASK(x) (1<<x)


/*------------------------------------------------------------------------*/

/* 
 * DCB_RW : Portion of domain control block that is writeable
 *          by user space.
 */

/* Number of bytes for activation stack */
#define DCBRW_W_STKSIZE 2048

#ifdef __LANGUAGE_C__

#define DCB_RO2RW(_rop)		((_rop)->dcbrw)
#define DCB_RW2RO(_rwp)		((_rwp)->dcbro)

#define DCB_RO(id)              ((dcb_ro_t *) ((id) & 0xffffffff))
#define DCB_RW(id)              DCB_RO2RW (DCB_RO (id))

#define DCB_ACT_CTX             0    /* load this on activation */


#define AUG_CACHE_SIZE 64

/* A domain's augmentation cache is an approximation to its current
   working set, and is used to optimistically patch the hw page tables
   with augmentations for this domain. Domains can modify this cache
   if they wish - before accessing the cache a domain should set the
   aug_cache_busy flag, and reset it after accessing. The kernel will
   ignore the cache while this flag is set. */

typedef struct _augment_entry {
    addr_t    linear;    /* address of page base */
    word_t    sid;       /* stretch id of page   */
    uint16_t  augs_left; 
    uint16_t  max_augs;
} augment_entry;

struct _dcb_rw /*<---*/ {

    /* Activation stack */
    word_t 	 stack[ DCBRW_W_STKSIZE / sizeof(word_t) ];

    /* Activation closure */
    addr_t 	 activ_clp;	/* Activation closure pointer 	*/
    void         (*activ_op)();	/* Activation entry point itself*/

    /* Activation flags and values */
/*  word_t	 activ_ps;	 Activation PS register	*/
    uint32_t	 actctx /*--->DCBRW_L_ACTCTX*/; /* Ctx slot: activations on  */
    uint32_t	 resctx /*--->DCBRW_L_RESCTX*/; /* Ctx slot: activations off */
    word_t	 mode   /*--->DCBRW_L_MODE*/;   /* Activation mode: 0 => on  */
    word_t	 status;		        /* Activation status 	     */

    word_t	 free_ctxts;	/* Free-list; ends @ ncontexts	*/
    ep_rw_t     *free_eps;	/* Free-list of eps		*/
  
    /* Event hint FIFO		*/
    uint32_t	 ep_head;	/* head of event hint fifo	*/
    uint32_t	 ep_tail;	/* tail of event hint fifo	*/

    /* Memory management info */
    Channel_Endpoint mm_ep   /*--->DCBRW_L_MMEP*/;   /* Endpoint for mmgmt  */
    word_t       mm_code /*--->DCBRW_L_MMCODE*/; /* Type of mem fault      */
    addr_t       mm_va   /*--->DCBRW_L_MMVA*/;   /* Faulting VA: may == ip */
    addr_t       mm_str  /*--->DCBRW_L_MMSTR*/;  /* Faulting stretch_clp   */

    /* back pointer to ro */
    dcb_ro_t    *dcbro;

    /* Pervasives */
    addr_t       pvs /*--->PVS_DCBRW_OFFSET*/;

    /* Stretches for the text and data of this domain */
    Stretch_cl  *textstr;
    Stretch_cl  *datastr;

    /* Stretch for the bss of this domain; may be NULL if bss follows data */
    Stretch_cl  *bssstr;

    /* Stretch for use by the domain in activation handlers, etc */
    Stretch_cl  *actstr;

    /* Binder information */
    word_t       binder_rw[256];	

    /* Outgoing event queue */
    word_t        outevs_pushed;
    dcb_event     outevs[NUM_DCB_EVENTS];

    bool_t        aug_cache_busy;
    word_t        num_augs;
    augment_entry augments[AUG_CACHE_SIZE]; /* The domain's patch cache */
    

};


/*  
 * Fifo management
 *
 * NB. - ep_fifo_get requires (! ep_fifo_empty(rwp))
 *     - ep_fifo_normalise must be called atomically wrt. others.
 *       TODO: decide whether this is ever possible (it's simple if
 *             others are only called by user with actns off).
 */

#define EP_FIFO_EMPTY(rwp)	((rwp)->ep_tail == (rwp)->ep_head)
#define EP_FIFO_RESET(rwp)	((rwp)->ep_tail = (rwp)->ep_head)

#define EP_FIFO_PUT(rop,rwp,epi) 				\
{								\
  Channel_EP ep = (epi);					\
  ((rop)->ep_fifo)[(rwp)->ep_head++ & (rop)->ep_mask] = ep;	\
}

#define EP_FIFO_GET(rop,rwp)					\
  (((rwp)->ep_fifo)[(rwp)->ep_tail++ & (rop)->ep_mask)]

#endif	/* __LANGUAGE_C__ */

/* Fields in DCB_Q_STATUS	*/
#define DSTAT_M_EVENTS  0x0001	/* To sched: Events are pending	*/
#define DSTAT_M_ARRIVE	0x0002	/* To domain: Events arrived	*/

#define DSTAT_V_EVENTS  0x0000
#define DSTAT_V_ARRIVE	0x0001

#endif /* _dcb_h_ */
