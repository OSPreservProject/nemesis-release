/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      IDCMacros.h
** 
** DESCRIPTION:
** 
**      Macros for dealing with IDC Offers, services, etc.
**      
** 
** ID : $Id: IDCMacros.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef _IDC_Macros_h_
#define _IDC_Macros_h_

#include <contexts.h>
#include <IDCTransport.ih>

#define IDC_BIND(_offer,_type)				\
  ({							\
    Type_Any any;					\
    ObjectTbl$Import (Pvs(objs), (_offer), &any);	\
    NARROW (&any, _type);				\
  })

#define IDC_OPEN_IN_CX(_ctx,_name,_type)		\
  ({							\
    IDCOffer_clp _offer;				\
    _offer = NAME_LOOKUP (_name,_ctx,IDCOffer_clp); 	\
    IDC_BIND (_offer,_type);				\
  })

#define IDC_OPEN(_name,_type)	IDC_OPEN_IN_CX(Pvs(root),_name,_type)

#define IDC_CLOSE(_offer)                                    \
({                                                           \
        ObjectTbl_Handle h;                                  \
        Type_Any any;                                        \
                                                             \
	if(!ObjectTbl$Info(Pvs(objs), (_offer), &any, &h)) { \
	    RAISE_IDC$Failure();                             \
	}                                                    \
                                                             \
        if(h.tag == ObjectTbl_EntryType_Surrogate) {         \
            IDCClientBinding$Destroy(h.u.Surrogate);         \
            ObjectTbl$Delete(Pvs(objs), (_offer));           \
	}                                                    \
})

#define IDC_EXPORT_IN_CX(_ctx,_name,_type,_svr)	\
  ({						\
    IDCTransport_clp	shmt;			\
    Type_Any       	any, offer_any;		\
    IDCOffer_clp   	offer;			\
    IDCService_clp 	service;		\
						\
    shmt = NAME_FIND ("modules>ShmTransport", IDCTransport_clp); \
    ANY_INIT(&any,_type,(_svr));		\
    offer = IDCTransport$Offer (shmt, &any, NULL, \
				Pvs(heap), Pvs(gkpr), \
				Pvs(entry), &service); \
    /* XXX - use publically readadble heap   ^^^^^^ */ \
						\
    ANY_INIT(&offer_any,IDCOffer_clp,offer);	\
    Context$Add ((_ctx), (_name), &offer_any);	\
    service;					\
  })

#define IDC_EXPORT(_name,_type,_svr) \
  IDC_EXPORT_IN_CX(Pvs(root),_name,_type,_svr)

#endif /* _IDC_Macros_h_ */

