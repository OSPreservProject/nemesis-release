/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      The Eoin Hyden Memorial Doubly-Linked List Macros
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Doubly linked lists. Maintains doubly linked lists of
**	structures which must be homogeneous and have fields called
**	'next' and 'prev'.
** 
** ENVIRONMENT: 
**
**      Pretty much anywhere. No locking.
** 
** ID : $Id: links.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _links_h_
#define _links_h_

#define LINK_ADD_TO_HEAD(_hp, _lp)		\
{						\
    __typeof__(_lp) _linkp = (_lp);		\
    __typeof__(_hp) _headp = (_hp);		\
    _linkp->next = _headp->next;		\
    _linkp->prev = _headp;			\
    _linkp->next->prev = _linkp;		\
    _linkp->prev->next = _linkp;		\
}

#define LINK_ADD_TO_TAIL(_hp, _lp)		\
{						\
    __typeof__(_lp) _linkp = (_lp);		\
    __typeof__(_hp) _headp = (_hp);		\
    _linkp->next = _headp;			\
    _linkp->prev = _headp->prev;		\
    _linkp->next->prev = _linkp;		\
    _linkp->prev->next = _linkp;		\
}

#define LINK_INSERT_AFTER(_headp, _linkp)  LINK_ADD_TO_HEAD(_headp, _linkp)
#define LINK_INSERT_BEFORE(_headp, _linkp) LINK_ADD_TO_TAIL(_headp, _linkp)

#define LINK_REMOVE(_lp)			\
{						\
    __typeof__(_lp) _linkp = (_lp);		\
    (_linkp)->next->prev = (_linkp)->prev;	\
    (_linkp)->prev->next = (_linkp)->next;	\
}

#define LINK_INITIALISE(_hp)			\
{						\
    __typeof__(_hp) _headp = (_hp);		\
    (_headp)->next = _headp;			\
    (_headp)->prev = _headp;			\
}

#define LINK_INIT(_headp)	LINK_INITIALISE(_headp)	

#define LINK_EMPTY(_headp)	((_headp)->next == (_headp))

#define LINK_LENGTH(_headp)					\
({								\
    int _i=0;							\
    __typeof__(_headp) _p;					\
    for(_p=(_headp)->next;_p != (_headp); _p=_p->next) _i++;	\
    _i;								\
})

#define LINK_REMOVE_HEAD(_headp)		\
({						\
    __typeof__(_headp) _p;			\
    if(LINK_EMPTY(_headp))			\
	_p = NULL;				\
    else {					\
	_p = (_headp)->next;			\
	LINK_REMOVE(_p);			\
    }						\
    _p;						\
})



/* 
 * The above, spelled differently: 
 */

#define Q_INSERT_AFTER(_qp, _lp)	LINK_INSERT_AFTER(_qp, _lp)
#define Q_INSERT_BEFORE(_qp, _lp)	LINK_INSERT_BEFORE(_qp, _lp)
#define Q_REMOVE(_lp)			LINK_REMOVE(_lp)
#define Q_EMPTY(_qp)			LINK_EMPTY(_qp)
#define Q_INIT(_qp)			LINK_INITIALISE(_qp)
#define Q_LEN(_qp)			LINK_LENGTH(_qp)
#define Q_ENQUEUE(_qp,_lp)		LINK_ADD_TO_TAIL(_qp,_lp)
#define Q_DEQUEUE(_qp)			LINK_REMOVE_HEAD(_qp)

#endif /* _links_h_ */
