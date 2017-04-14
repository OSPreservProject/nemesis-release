/*
 *****************************************************************************
 **                                                                          *
 **  Copyright 1994, University of Cambridge Computer Laboratory             *
 **                                                                          *
 **  All Rights Reserved.                                                    *
 **                                                                          *
 *****************************************************************************
 **
 ** FILE:
 **
 **      mod/nemesis/alpha/VP.c
 **
 ** FUNCTIONAL DESCRIPTION:
 **
 **      User-level Virtual Processor management over DCBs.
 **      Almost entirely boring veneer (in h/$ARCH/VPMacros.h),
 **      with the exception of NextEvent. Usually inline, but it's
 **      valid to call these functions too (usually from Clanger)
 **
 ** ENVIRONMENT:
 **
 **      Intel.
 **
 ** ID : $Id: VP.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 **
 **
 */

#include <nemesis.h>
#include <exceptions.h>
#include <VPMacros.h>

#ifdef __SMP__
#include <smp.h>
#endif

#define DEBUG

#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x)
#endif

#ifdef TRACE_VP
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

/*
 * Imports
 */

/*
 * Prototypes
 */
static VP_NumContexts_fn        NumContexts_m;
static VP_AllocContext_fn       AllocContext_m;
static VP_FreeContext_fn        FreeContext_m;
static VP_Context_fn            Context_m;
static VP_SetActivationVector_fn SetActivationVector_m;
static VP_ActivationsOn_fn      ActivationsOn_m;
static VP_ActivationsOff_fn     ActivationsOff_m;
static VP_ActivationMode_fn     ActivationMode_m;
static VP_GetSaveSlot_fn        GetSaveSlot_m;
static VP_SetSaveSlot_fn        SetSaveSlot_m;
static VP_GetResumeSlot_fn      GetResumeSlot_m;
static VP_SetResumeSlot_fn      SetResumeSlot_m;
static VP_NumChannels_fn        NumChannels_m;
static VP_QueryChannel_fn       QueryChannel_m;
static VP_AllocChannel_fn	AllocChannel_m;
static VP_FreeChannel_fn	FreeChannel_m;

static VP_Send_fn               Send_m;
static VP_Poll_fn               Poll_m;
static VP_EventsPending_fn      EventsPending_m;
static VP_NextEvent_fn          NextEvent_m;
static VP_RFA_fn                RFA_m;
static VP_RFAResume_fn          RFAResume_m;
static VP_RFABlock_fn           RFABlock_m;
static VP_Block_fn              Block_m;
static VP_Yield_fn              Yield_m;
static VP_DomainID_fn           DomainID_m;
static VP_ProtDomID_fn          ProtDomID_m;
static VP_BinderOffer_fn	BinderOffer_m;

static uint32_t NumContexts_m (VP_cl   *self )
{
    return VP$NumContexts(self);
}

static VP_ContextSlot AllocContext_m (VP_cl   *self )
{
    return VP$AllocContext(self);
}

static void FreeContext_m (VP_cl   *self,
			   VP_ContextSlot  cs /* IN */ )
{
    VP$FreeContext(self,cs);
}

static addr_t Context_m (VP_cl   *self,
			 VP_ContextSlot  cs /* IN */ )
{
    return VP$Context(self,cs);
}

static void SetActivationVector_m (VP_cl   *self,
				   Activation_clp  avec	/* IN */ )
{
    VP$SetActivationVector(self,avec);
}

static void ActivationsOn_m (VP_cl   *self )
{
    VP$ActivationsOn(self);
}

static void ActivationsOff_m (VP_cl   *self )
{
    VP$ActivationsOff(self);
}

static bool_t ActivationMode_m (VP_cl   *self )
{
    return VP$ActivationMode(self);
}

static VP_ContextSlot GetSaveSlot_m (VP_cl   *self )
{
    return VP$GetSaveSlot(self);
}

static addr_t SetSaveSlot_m (VP_cl   *self,
			     VP_ContextSlot  cs	/* IN */ )
{
    return VP$SetSaveSlot(self,cs);
}

static VP_ContextSlot GetResumeSlot_m (VP_cl   *self )
{
    return VP$GetResumeSlot(self);
}

static void SetResumeSlot_m (VP_cl   *self,
			     VP_ContextSlot  cs	/* IN */ )
{
    VP$SetResumeSlot(self,cs);
}

static uint32_t NumChannels_m (VP_cl   *self )
{
    return VP$NumChannels(self);
}

static Channel_State 
QueryChannel_m (VP_cl   *self,
		Channel_Endpoint ep	/* IN */
		/* RETURNS */,
		Channel_EPType  *type,
		Event_Val       *rxval,
		Event_Val       *rxack )
{
    return VP$QueryChannel(self,ep,type,rxval,rxack);
}

static Channel_Endpoint AllocChannel_m(VP_cl *self)
{
    dcb_rw_t     *rwp = RW(self);
    dcb_ro_t     *rop = RO(self);
    ep_rw_t      *eprw;
    Channel_EP    ept;

    if ( !(eprw = rwp->free_eps) ) {
#ifdef __SMP__
        DB(eprintf("VP$AllocChannel: no end-points free in domain %qx\n", INFO_PAGE_PCPU(smp_cpuid()).dcb_ro->id));
#else
        DB(eprintf("VP$AllocChannel: no end-points free in domain %qx\n", rop->id));
#endif
	RAISE_Channel$NoSlots( rop->id );
    }
    ept           = eprw - rop->eprws;
    rwp->free_eps = (ep_rw_t *)eprw->ack;
    eprw->state   = Channel_State_Local;
    eprw->ack     = 0;
    return ept;
}

static void FreeChannel_m(VP_cl *self, Channel_Endpoint ep )
{
    dcb_rw_t     *rwp = RW(self);
    dcb_ro_t     *rop = RO(self);
    ep_rw_t      *eprw;
    ep_ro_t      *epro;

    if ( ep >= rop->num_eps ) {
	DB(eprintf("VP$FreeChannel: invalid EP.\n"));
	RAISE_Channel$Invalid( ep );
    }

    epro = DCB_EPRO(rop,ep);
    eprw = DCB_EPRW(rop,ep);
    if ( epro->peer_dcb != NULL || eprw->state == Channel_State_Free ) {
	DB(eprintf("VP$FreeChannel: Bad State.\n"));
	RAISE_Channel$BadState( ep, eprw->state );
    }
    
    eprw->state = Channel_State_Free;
    eprw->ack   = (word_t)(rwp->free_eps);
    rwp->free_eps = eprw;
}

static void Send_m (VP_cl   *self,
		    Channel_TX      tx /* IN */,
		    Event_Val       val	/* IN */ )
{
    VP$Send(self,tx,val);
}

static Event_Val Poll_m (VP_cl   *self,
			 Channel_RX      rx /* IN */ )
{
    return VP$Poll(self,rx);
}

static Event_Val Ack_m (VP_cl   *self,
			Channel_RX      rx /* IN */,
			Event_Val       val /* IN */ )
{
    return VP$Ack(self,rx,val);
}

static bool_t EventsPending_m (VP_cl   *self )
{
    return VP$EventsPending(self);
}

static void RFA_m (VP_cl   *self )
{
    VP$RFA(self);
}

static void RFAResume_m (VP_cl   *self,
			 VP_ContextSlot  cs /* IN */ )
{
    VP$RFAResume(self,cs);
}

static void RFABlock_m (VP_cl   *self,
			Time_T  until /* IN */ )
{
    VP$RFABlock(self,until);
}

static void Block_m (VP_cl   *self,
		     Time_T  until /* IN */ )
{
    VP$Block(self,until);
}

static void Yield_m (VP_cl   *self )
{
    VP$Yield(self);
}

static Domain_ID DomainID_m (VP_cl   *self )
{
    return VP$DomainID(self);
}

static ProtectionDomain_ID ProtDomID_m (VP_cl   *self )
{
    return VP$ProtDomID(self);
}

static IDCOffer_clp BinderOffer_m (VP_clp self)
{
    return RO(self)->binder_offer;
}


/*-------------------------------------------------------- Entry Points ---*/


static bool_t
NextEvent_m (VP_cl              *self,
             /* RETURNS */
             Channel_Endpoint   *ep,
             Channel_EPType     *type,
             Event_Val          *val,
             Channel_State      *state)
{
    dcb_rw_t     *rwp = RW(self);
    dcb_ro_t     *rop = RO(self);
    ep_ro_t      *epro;
    ep_rw_t      *eprw;

    TRC (eprintf ("VP$NextEvent(dom=%lx): rop=%x rwp=%x head=%x tail=%x "
		  "n=%x mask=%x\n",
		  rop->id, rop, rwp, rwp->ep_head, rwp->ep_tail,
		  rop->num_eps, rop->ep_mask));

    if (EP_FIFO_EMPTY (rwp))
	return False;

    /* Pull the first end-point out of the FIFO */
    *ep = rop->ep_fifo [rwp->ep_tail & rop->ep_mask];
    rwp->ep_tail++;

    /* Here,  "ept" is to be returned. */
    epro   = rop->epros + *ep;
    eprw   = rop->eprws + *ep;

    *type  = epro->type;
    *val   = epro->val;
    *state = eprw->state;

    TRC (eprintf ("VP$NextEvent(dom=%lx): rop=%x rwp=%x\n", rop, rwp));

    return True;
}



/*-------------------------------------------------------------------------*/

/*
 * This module is unusual in that it has no creator closure.  Instead,
 * it exports its method suite.  A pointer to this is placed in a VP_cl
 * in the dcb_ro_t for a domain during domain initialisation.  The "st"
 * half of that VP_cl is initialised to point to the dcb_rw_t for the domain.
 */

static VP_op VP_ms = {
    NumContexts_m,
    AllocContext_m,
    FreeContext_m,
    Context_m,
    SetActivationVector_m,
    ActivationsOn_m,
    ActivationsOff_m,
    ActivationMode_m,
    GetSaveSlot_m,
    SetSaveSlot_m,
    GetResumeSlot_m,
    SetResumeSlot_m,
    NumChannels_m,
    QueryChannel_m,
    AllocChannel_m,
    FreeChannel_m,
    Send_m,
    Poll_m,
    Ack_m,
    EventsPending_m,
    NextEvent_m,
    RFA_m,
    RFAResume_m,
    RFABlock_m,
    Block_m,
    Yield_m,
    DomainID_m,
    ProtDomID_m,
    BinderOffer_m
};

static const VP_cl vp_cl= { &VP_ms, BADPTR };

CL_EXPORT(VP, VP, vp_cl);

/*
 * End VP.c
 */
