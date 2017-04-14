/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**
**
** FUNCTIONAL DESCRIPTION:
**
**
**
** ENVIRONMENT: 
**
**
**
** ID : $Id: callback.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
**
*/

#include <nemesis.h>

#include <IDCCallback.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

static  IDCCallback_Request_fn          IDCCallback_Request_m;
static  IDCCallback_Bound_fn            IDCCallback_Bound_m;
static  IDCCallback_Closed_fn           IDCCallback_Closed_m;

static  IDCCallback_op  ms = {
  IDCCallback_Request_m,
  IDCCallback_Bound_m,
  IDCCallback_Closed_m
};

static const IDCCallback_cl  cl = { &ms, NULL };

CL_EXPORT (IDCCallback, NullIDCCallback, cl);


/*---------------------------------------------------- Entry Points ----*/

static bool_t IDCCallback_Request_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        Domain_ID       did     /* IN */,
        ProtectionDomain_ID     pdid    /* IN */,
        const Binder_Cookies    *clt_cks        /* IN */,
        Binder_Cookies  *svr_cks        /* OUT */ )
{
    return True;
}

static bool_t IDCCallback_Bound_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        IDCServerBinding_clp    binding /* IN */,
        Domain_ID       did     /* IN */,
        ProtectionDomain_ID     pdid    /* IN */,
        const Binder_Cookies    *clt_cks        /* IN */,
        Type_Any        *server /* INOUT */ )
{
    return True;
}

static void IDCCallback_Closed_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        IDCServerBinding_clp    binding /* IN */,
        const Type_Any  *server /* IN */ )
{
    return;
}

/*
 * End 
 */
