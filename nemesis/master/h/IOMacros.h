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
**      IOMacros.h
** 
** DESCRIPTION:
** 
**      Convenient macros for binding IO channels.
**      
** ID : $Id: IOMacros.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef _IOMacros_h_
#define _IOMacros_h_

#include <IOTransport.ih>
#include <IOEntryMod.ih>

#define IO_BIND(_offer) ({						\
    IOOffer_clp offercopy = (_offer);					\
    Type_Any any;							\
    ObjectTbl_Handle hdl;						\
    IDCClientBinding_clp cb = NULL;					\
    if(!ObjectTbl$Info(Pvs(objs), (IDCOffer_clp)offercopy, &any, &hdl))	\
	cb = IDCOffer$Bind((IDCOffer_clp)offercopy, Pvs(gkpr), &any);	\
    if(!ISTYPE(&any, IO_clp)) {						\
	if(cb) IDCClientBinding$Destroy(cb);				\
	RAISE_TypeSystem$Incompatible();				\
    }									\
    NARROW (&any, IO_clp);						\
})

#define IO_SHM_BIND(_offer,_shm) ({					   \
    IOOffer_clp offercopy = (_offer);					   \
    Type_Any any;							   \
    ObjectTbl_Handle hdl;						   \
    IDCClientBinding_clp cb = NULL;					   \
    if(!ObjectTbl$Info(Pvs(objs), (IDCOffer_clp)offercopy, &any, &hdl))	   \
	cb = IOOffer$ExtBind(offercopy, _shm, Pvs(gkpr), &any); \
    if(!ISTYPE(&any, IO_clp)) {						   \
	if(cb) IDCClientBinding$Destroy(cb);				   \
	RAISE_TypeSystem$Incompatible();				   \
    }									   \
    NARROW (&any, IO_clp);						   \
})

#define IO_PRIME(_io,_size) {					\
    IOData_Shm   *_shm = NULL;					\
    addr_t        _base;					\
    word_t        _len;						\
    uint32_t      _slots, _depth;				\
    IO_Rec        _rec;						\
								\
    if(IO$QueryKind((_io)) != IO_Kind_Master)			\
	RAISE_IO$Failure();					\
    if(IO$QueryMode((_io)) != IO_Mode_Rx)			\
	RAISE_IO$Failure();					\
    _slots = IO$QueryDepth((_io));				\
    _shm   = IO$QueryShm((_io));				\
    if(!_shm || !_shm->len)					\
	RAISE_IO$Failure();					\
    _base  = SEQ_ELEM(_shm, 0).buf.base;			\
    _len   = SEQ_ELEM(_shm, 0).buf.len;				\
								\
    _depth   = MIN(((_slots) >> 1), ((_len) / (_size)));	\
    _rec.len = (_size);						\
    while(_depth) {						\
	_rec.base = _base;					\
	_base    += (_size);					\
	IO$PutPkt ((_io), 1, &_rec, 1, FOREVER);		\
	_depth--;						\
    }								\
}


#define IO_OPEN_IN_CX(_ctx,_name)			\
  ({							\
    IDCOffer_clp _offer;				\
    _offer = NAME_LOOKUP (_name,_ctx,IDCOffer_clp); 	\
    IO_BIND (_offer);					\
  })

#define IO_OPEN(_name)	IO_OPEN_IN_CX(Pvs(root),_name)


#define IO_EXPORT_IN_CX(_ctx,_name,_hdl,_mode,_kind,_shmsz,_depth,_entry) ({  \
    IOTransport_clp	iot;						      \
    IOEntry_clp		entry = (_entry);				      \
    Type_Any       	offer_any;					      \
    IOOffer_clp   	offer;						      \
    IDCService_clp 	service;					      \
    IOData_Shm         *shm;						      \
									      \
    iot = NAME_FIND ("modules>IOTransport", IOTransport_clp);		      \
    if (!entry)								      \
    {									      \
	IOEntryMod_clp em = NAME_FIND ("modules>IOEntryMod", IOEntryMod_clp); \
	entry = IOEntryMod$New (em, Pvs(actf), Pvs(vp),			      \
				VP$NumChannels (Pvs(vp)));		      \
    }									      \
									      \
    /* XXX SMH: can only cope with one data area in macro */		      \
    shm = SEQ_NEW(IOData_Shm, 1, Pvs(heap));				      \
    SEQ_ELEM(shm, 0).buf.base     = NO_ADDRESS; /* not yet allocated */	      \
    SEQ_ELEM(shm, 0).buf.len      = (_shmsz);				      \
    SEQ_ELEM(shm, 0).param.attrs  = 0;					      \
    SEQ_ELEM(shm, 0).param.awidth = PAGE_WIDTH;				      \
    SEQ_ELEM(shm, 0).param.pwidth = PAGE_WIDTH;				      \
									      \
    offer = IOTransport$Offer (iot, Pvs(heap), (_depth), (_mode), (_kind),    \
    /* XXX use publicly readadble heap  ^^^^ */				      \
			       shm,Pvs(gkpr), NULL, entry, (_hdl), &service); \
									      \
    ANY_INIT(&offer_any,IOOffer_clp,offer);				      \
    Context$Add ((_ctx), (_name), &offer_any);				      \
    entry;								      \
})

#define IO_EXPORT(_name,_hdl,_mode,_kind,_shmsz,_depth,_entry) \
    IO_EXPORT_IN_CX(Pvs(root),_name,_hdl,_mode,_kind,_shmsz,_depth,_entry)

#define IO$QueryPut(_io,_nrecs,_until) \
    IO$PutPkt((_io), (_nrecs), NULL, 0, (_until))

#define IO$QueryGet(_io,_until,_nrecs) \
    IO$GetPkt((_io), 0, NULL, (_until), (_nrecs), NULL)


/* 
** XXX SMH: we still tuck away a 'handle' in the first word of whatever it 
** is the state pointer of an IO closure points to. To make code vaguely 
** readable, we provide the below macro (though would be much nicer to kill 
** the handle thing now that we have sensible callbacks, etc).  
*/
#define IO_HANDLE(_io) (((word_t *)(_io)->st)[0])

#endif /* _IOMacros_h_ */

