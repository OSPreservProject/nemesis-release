/*
 *****************************************************************************
 **                                                                          *
 **  Copyright 1994, University of Cambridge Computer Laboratory             *
 **                                                                          *
 **  All Rights Reserved.					             *
 **                                                                          *
 *****************************************************************************
 **
 ** FILE:
 **
 **      ix86/VPMacros.h
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 **      Inline implementations of most of VP.if over intel/dcb.h etc.
 ** 
 ** ENVIRONMENT: 
 **
 **      Intel
 ** 
 ** ID : $Id: VPMacros.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 ** 
 **
 */

#ifndef _VPMacros_h_
#define _VPMacros_h_

#include <dcb.h>
#include <VP.ih>
#include <ntsc.h>		/* defines ntsc_XXX()  */


#define RW(self)		((dcb_rw_t *) (self->st))
#define RO(self)		(DCB_RW2RO (RW(self)))


/* 
 ** Context Slots
 */

#undef  VP$NumContexts
#define VP$NumContexts(self)	(RO(self)->num_ctxts)

#undef  VP$AllocContext
#define VP$AllocContext(self)						\
({									\
     dcb_rw_t	*_rwp = RW(self);					\
     dcb_ro_t	*_rop = RO(self);					\
     VP_ContextSlot _res;						\
									\
     if (_rwp->free_ctxts == _rop->num_ctxts )				\
	 RAISE_VP$NoContextSlots();					\
     _res = (VP_ContextSlot) _rwp->free_ctxts;				\
     _rwp->free_ctxts = (_rop->ctxts[_res]).CX_FREELINK;		\
     _res;								\
 })

#undef  VP$FreeContext
#define VP$FreeContext(self,cs)						\
{									\
    dcb_rw_t	*_rwp = RW(self);					\
    dcb_ro_t	*_rop = RO(self);					\
									\
    if (cs >= VP$NumContexts(self)) RAISE_VP$InvalidContext (cs);	\
    (_rop->ctxts)[cs].CX_FREELINK = _rwp->free_ctxts;			\
    _rwp->free_ctxts = (word_t) cs;					\
}

#undef  VP$Context
#define VP$Context(self,cs) 						\
( (cs < VP$NumContexts(self)) ? &((RO(self)->ctxts)[cs]) : NULL )


/* 
 * Activations
 */

#undef  VP$SetActivationVector
#define VP$SetActivationVector(self,avec)			\
{								\
    RW(self)->activ_clp = (avec);  				\
    RW(self)->activ_op  = (avec)->op->Go;			\
}

#undef  VP$ActivationsOn(self)
#define VP$ActivationsOn(self)		(RW(self)->mode = 0)

#undef  VP$ActivationsOff(self)
#define VP$ActivationsOff(self)		(RW(self)->mode = 1)

#undef  VP$ActivationMode
#define VP$ActivationMode(self)		(! (RW(self)->mode))

#undef  VP$GetSaveSlot(self)
#define VP$GetSaveSlot(self)		(RW(self)->actctx)

#undef  VP$SetSaveSlot(self,cs)
#define VP$SetSaveSlot(self,cs)						\
({									\
     if (cs >= VP$NumContexts(self)) RAISE_VP$InvalidContext (cs);	\
     RW(self)->actctx = (cs);						\
     &((RO(self)->ctxts)[cs]);	/* result */				\
 })

#undef  VP$GetResumeSlot(self)
#define VP$GetResumeSlot(self)		(RW(self)->resctx)

#undef  VP$SetResumeSlot(self,cs)
#define VP$SetResumeSlot(self,cs)					\
{									\
    if (cs >= VP$NumContexts(self)) RAISE_VP$InvalidContext (cs);	\
    RW(self)->resctx = (cs);						\
}


/* 
 * Event Channels
 */

#undef  VP$NumChannels
#define VP$NumChannels(self)		(RO(self)->num_eps)

#undef  VP$QueryChannel
#define VP$QueryChannel(self,ep,tpe,rxval,rxack)			\
({									\
     Channel_State _state;						\
     ep_ro_t      *_epro;						\
     ep_rw_t      *_eprw;						\
									\
     if (ep >= VP$NumChannels(self)) RAISE_Channel$Invalid (ep);	\
     _epro      = DCB_EPRO(RO(self), ep);				\
     _eprw      = DCB_EPRW(RO(self), ep);				\
     _state     = _eprw->state;						\
     *(tpe)     = _epro->type;						\
     *(rxval)   = _epro->val;						\
     *(rxack)   = _eprw->ack;						\
     (_state==Channel_State_Connected && _epro->peer_dcb==NULL)		\
	 ? Channel_State_Dead : _state;					\
 })

/* AllocChannel is in lib/nemesis/VP/alpha/VP.c */

/* FreeChannel is in lib/nemesis/VP/alpha/VP.c */

#undef  VP$Send
#define VP$Send(self,tx,val)						 \
{									 \
									 \
    dcb_rw_t *_rwp = RW(self);						 \
    dcb_ro_t *_rop = _rwp->dcbro;					 \
    ep_ro_t *_eprop = DCB_EPRO (_rop, (tx));				 \
    ep_rw_t *_eprwp = DCB_EPRW (_rop, (tx));				 \
									 \
    if(_eprop->type != Channel_EPType_TX) {				 \
	RAISE_Channel$Invalid((tx));					 \
    } else if(_eprwp->state != Channel_State_Connected) {		 \
	RAISE_Channel$BadState((tx), 0);				 \
    } else {								 \
	dcb_event *_ev =						 \
	    &_rwp->outevs[(_rwp->outevs_pushed + 1) % NUM_DCB_EVENTS];	 \
	while(_rwp->outevs_pushed ==					 \
	      _rop->outevs_processed + NUM_DCB_EVENTS) {		 \
	    /* Need ntsc_flush_evs() */					 \
	    ntsc_yield();						 \
	}								 \
	_ev->ep = (tx);							 \
	_ev->val = (val);						 \
	_rwp->outevs_pushed++;						 \
    }									 \
}									
									
#undef  VP$Poll	
#define VP$Poll(self,ep)			\
({						\
     if (ep >= VP$NumChannels(self) )		\
	 RAISE_Channel$Invalid (ep);		\
     DCB_EPRO(RO(self),ep)->val;		\
 })

/* 
 * XXX Note that we currently stop Ack raising exceptions. When TRY
 * costs nothing, this should go back in and someone should fix the
 * threads packages.
 */
#undef  VP$Ack
#define VP$Ack(self,_ep,_val)						\
({									\
     if (_ep >= VP$NumChannels(self)) /* RAISE_Channel$Invalid (rx) */;	\
     else DCB_EPRW(RO(self),_ep)->ack = _val;				\
     DCB_EPRO(RO(self),_ep)->val;					\
 })

#undef  VP$EventsPending
#define VP$EventsPending(self)	(! EP_FIFO_EMPTY (RW (self)))

/* NextEvent is in lib/nemesis/VP/alpha/VP.c */

/* 
 * Scheduling
 */

#undef  VP$RFA
#define VP$RFA(self)		ntsc_rfa() 

#undef  VP$RFAResume
#define VP$RFAResume(self,cs)						\
{									\
    if (cs >= VP$NumContexts(self)) RAISE_VP$InvalidContext (cs);	\
    ntsc_rfa_resume (cs);						\
}

#undef  VP$RFABlock
#define VP$RFABlock(self,until)	ntsc_rfa_block (until)

#undef  VP$Block
#define VP$Block(self,until)	ntsc_block (until)

#undef  VP$Yield
#define VP$Yield(self)		ntsc_yield()



/* 
 * Domain information
 */

#undef  VP$DomainID
#define VP$DomainID(self)	(RO(self)->id)

#undef  VP$ProtDomID
#define VP$ProtDomID(self)	(RO(self)->pdid)



#endif /* _VPMacros_h_ */
