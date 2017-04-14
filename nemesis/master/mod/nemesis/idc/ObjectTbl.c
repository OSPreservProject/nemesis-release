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
 **      mod/nemesis/idc/ObjectTbl.c
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 **      Implements ObjectTbl.if; maintains list of interfaces exported
 **	by the domain so they can be identified if IDCOffers to them
 **	arrive.
 ** 
 ** ENVIRONMENT: 
 **
 **      Domain code
 ** 
 ** ID : $Id: ObjectTbl.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 ** 
 **
 */

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <time.h>
#include <ecs.h>
#include <domid.h>
#include <VPMacros.h>

#include <ShmTransport.h>

#include <ObjectTblMod.ih>
#include <WordTblMod.ih>
#include <EntryMod.ih>
#include <Entry.ih>
#include <stdio.h>

#ifdef OBJECTTBL_TRACE
#define DEBUG
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
 * Module stuff
 */
static ObjectTbl_Export_fn	Export_m;
static ObjectTbl_Import_fn	Import_m;
static ObjectTbl_Info_fn	Info_m;
static ObjectTbl_Delete_fn	Delete_m;
static ObjectTbl_op ot_op = { 
    Export_m, 
    Import_m,
    Info_m, 
    Delete_m,
};

static ObjectTblMod_New_fn	New_m;
static ObjectTblMod_op mod_op = { New_m };
static const ObjectTblMod_cl mod_cl = { &mod_op, NULL };

CL_EXPORT (ObjectTblMod, ObjectTblMod, mod_cl);

static BinderCallback_Request_fn	Request_m;
static BinderCallback_SimpleRequest_fn 	SimpleRequest_m;
static BinderCallback_op bcb_op = { Request_m, SimpleRequest_m };

/* 
 * State record
 */
typedef struct ot_st ot_st;
struct ot_st {
    ObjectTbl_cl 	        o_cl;   /* Client closure 	*/
    BinderCallback_cl	        bcb_cl;
    Heap_clp	  		heap;
    Binder_clp			binder;
    EntryMod_clp		emod;
    
    Server_t			s;       /* Callback Server state */
    ShmConnection_t             sconn;   /* Callback connection   */
    Entry_clp                   cbentry; /* Callback entry        */

    mutex_t	 		mu;
    /* LL (Locking Level) >= { mu } */
    bool_t			registered;
    WordTbl_clp	  		tbl; /* IDCOffer_clp -> tbl_entry_t	*/
    Event_Count                 ec;
};

/*
 * Object table entries
 */
typedef struct tbl_entry_t tbl_entry_t;
struct tbl_entry_t {
    Type_Any	 	intf;
    ObjectTbl_Handle	handle;
};

/* 
 * Prototypes
 */

static void	RegisterCallback (ot_st *st);


/*-----Object Table Methods---------------------------------------------*/

/*
 * Create a new entry and put it into the object table. 
 */
static void Export_m (ObjectTbl_cl	*self,
		      IDCService_clp	service,
		      IDCOffer_clp	offer,
		      const Type_Any   *intf )
{
    ot_st *st = (ot_st *)(self->st);
    tbl_entry_t *entry;

    TRC(eprintf("ObjectTbl$Export: called; offer=%p\n",offer));

    /* Get some memory */
    if (!(entry = Heap$Malloc(st->heap, sizeof(*entry) ))) {
	RAISE_ObjectTbl$Failure( ObjectTbl_FailType_NoMemory );
    }
    
    /* Fill in the entry */
    ANY_COPY(&entry->intf, intf);
    entry->handle.tag = ObjectTbl_EntryType_Service;
    entry->handle.u.Service = service;

    USING(MUTEX,&st->mu,
      {
	  /* Start the server-side machinery on the first export */
	  if (!st->registered) RegisterCallback (st);

	  /* Put the new entry into the table */
	  if ( WordTbl$Put(st->tbl, (WordTbl_Key) offer, entry) ) {
	      DB(eprintf("ObjectTbl$Export: duplicate entry.\n"));
	      RAISE_ObjectTbl$Failure( ObjectTbl_FailType_Duplicate );
	  }
      });
}


/*
 * Look up an entry in the table to return the corresponding
 * interface.
 */
static void Import_m (ObjectTbl_cl	*self,
		      IDCOffer_clp	offer,
		      Type_Any	*intf )
{
    ot_st 	*st = (ot_st *)(self->st);
    tbl_entry_t 	*entry;
    IDCClientBinding_clp  NOCLOBBER client;
    
    bool_t found = False;

    TRC(eprintf("ObjectTbl$Import: offer %x\n", offer));

    MU_LOCK(&st->mu);

    TRC(eprintf("ObjectTbl$Import: locked.\n"));
    
    found = WordTbl$Get(st->tbl,
		      (WordTbl_Key)offer,
		      (WordTbl_Val *)&entry );

    if(found) {

	TRC(eprintf("ObjectTbl$Import: found entry %p.\n", entry));

	while(found && (entry == NULL)) {
	
	    /* There was an entry in the table for this offer, but it
               was NULL. This means someone else is in the process of
               binding to the offer at the moment. 

	       We loop waiting for either the binding to appear, or
	       the table entry to be removed (indicating failure on
	       the part of the other thread) */
    
	    /* We know that this event count is safe, so no TRY block */
	    Event_Val val = EC_READ(st->ec);
	    
	    MU_RELEASE(&st->mu);

	    TRC(eprintf("Waiting for binding for offer %p\n", offer));

	    EC_AWAIT(st->ec, val + 1);

	    MU_LOCK(&st->mu);

	    found = WordTbl$Get(st->tbl, 
				(WordTbl_Key)offer, 
				(WordTbl_Val *)&entry);

	    TRC(eprintf("ObjectTbl$Import: entry now %p\n", entry));

	}

	MU_RELEASE(&st->mu);

	if(!found) {
	    DB(eprintf("ObjectTbl$Import: Binding for offer %p has vanished\n",
		       offer));
	    
	    RAISE_ObjectTbl$Failure(ObjectTbl_FailType_Bind);
	}
	
    } else {
	
	/* No interface in table, so we need to do a bind. */
	
	/* Before we release the mutex, store a NULL entry in the
	   table in case someone else tries to bind to the same offer */

	WordTbl$Put(st->tbl, (WordTbl_Key) offer, 0);

	MU_RELEASE(&st->mu);

	if (!(entry = Heap$Malloc(st->heap, sizeof(*entry) ) ) ) {

	    /* Remove the temp entry and release the mutex */
	    WordTbl$Delete(st->tbl, (WordTbl_Key)offer, (WordTbl_Val *)&entry);
	    MU_RELEASE(&st->mu);
	    
	    eprintf("ObjectTbl$Import: out of memory.\n");
	    RAISE_ObjectTbl$Failure( ObjectTbl_FailType_NoMemory );
	}
	
	
	TRY {
	    TRC(eprintf("ObjectTbl$Import: Binding to offer %p\n", offer));
	    /* Create a table entry for the new surrogate */
	    client = IDCOffer$Bind(offer, Pvs(gkpr), &entry->intf );
	    TRC(eprintf("ObjectTbl$Import: Bound to %p\n", offer));
	    
	} CATCH_ALL {

	    addr_t dummy;
	    eprintf("ObjectTbl$Import: failure '%s': freeing memory.\n",
		       exn_ctx.cur_exception);
	    DB(eprintf("ObjectTbl$Import: failure '%s': freeing memory.\n",
		       exn_ctx.cur_exception));
	    
	    /* Take the dummy entry out of the table, and kick anyone
               who is waiting for it */
	    
	    MU_LOCK(&st->mu);
	    WordTbl$Delete(st->tbl, (WordTbl_Key) offer, &dummy);
	    EC_ADVANCE(st->ec, 1);
	    MU_RELEASE(&st->mu);
	    
	    FREE(entry);
	    RERAISE;
	}
	ENDTRY;

	entry->handle.tag = ObjectTbl_EntryType_Surrogate;
	entry->handle.u.Surrogate = client;
	
	/* Put into the table, and kick anyone who is waiting for it */

	MU_LOCK(&st->mu);
	WordTbl$Put(st->tbl, (word_t)offer, entry);
	EC_ADVANCE(st->ec, 1);
	MU_RELEASE(&st->mu);

    }
   
    /* Return the interface from the table entry. */
    ANY_COPY (intf,&entry->intf);
    TRC(eprintf("ObjectTbl$Import return (%qx,%qx)\n", entry->intf.val, entry->intf.type));
}

/*
 * Info on an entry.
 */
static bool_t Info_m(ObjectTbl_cl      *self,
		     IDCOffer_clp       offer,
		     Type_Any          *intf,
		     ObjectTbl_Handle  *control )
{
    ot_st 		*st = (ot_st *)(self->st);
    tbl_entry_t	 	*entry;
    NOCLOBBER bool_t	 res;

    TRC(eprintf("ObjectTbl$Info: offer %x\n", offer));

    MU_LOCK(&st->mu);

    res = WordTbl$Get(st->tbl, 
		      (WordTbl_Key)offer, 
		      (WordTbl_Val *)&entry);
    
    if(res && (entry == NULL)) {
	
	/* If entry exists but is NULL, it means that someone is
	   currently trying to bind to this offer. Treat it as
	   though it weren't there */
	
	res = False;
    }
    
    if (res)
    {
	*intf    = entry->intf;
	*control = entry->handle;
    }
    
    MU_RELEASE(&st->mu);

    return res;
}

/*
 * Remove an entry from the table. Not that deleting an entry whilst
 * it is still in use will cause confusion. 
 */
static bool_t Delete_m (ObjectTbl_cl	*self,
			IDCOffer_clp	offer )
{
    ot_st 		*st = (ot_st *)(self->st);
    tbl_entry_t	 	*entry;
    NOCLOBBER bool_t	 res;

    MU_LOCK(&st->mu);
    
    res = WordTbl$Delete(st->tbl, 
			 (WordTbl_Key)offer, 
			 (WordTbl_Val *)&entry );
    
    MU_RELEASE(&st->mu);

    return res;
}

/*-----Callback-----------------------------------------------------------*/

static void
Request_m (BinderCallback_cl	*self,
	   Domain_ID		client /* IN */,
	   ProtectionDomain_ID	pdid /* IN */,
	   Binder_Port		port /* IN */,
	   const Binder_Cookies	*clt_cks /* IN */,
	   Channel_Pairs	*chans /* OUT */,
	   Binder_Cookies	*svr_cks /* OUT */ )
{
    ot_st 	*st = self->st;
    tbl_entry_t 	*entry;
    bool_t      found;
 
    /* Look up the service ID in this object table and ensure that */
    /* it's a service and not a surrogate */
    
    MU_LOCK(&st->mu);

    found = WordTbl$Get(st->tbl, (WordTbl_Key)(port),(WordTbl_Val *)&entry);

    MU_RELEASE(&st->mu);

    if (found && (entry != NULL) &&
	entry->handle.tag == ObjectTbl_EntryType_Service )
    {
	/* Invoke bind on the service */
	TRC (eprintf ("ObjectTbl$Request: attempting bind to %x.\n", port));
	TRC (eprintf ("ObjectTbl$Request: clt_cks=%x len=%x\n",
		      clt_cks, SEQ_LEN(clt_cks)));
	BinderCallback$Request (
	    (BinderCallback_clp) entry->handle.u.Service,
	    client, pdid, port, clt_cks, chans, svr_cks);
    }
    else
    {
	RAISE_Binder$Error (Binder_Problem_BadID);
    }

}

static void
SimpleRequest_m (BinderCallback_cl	*self,
		 Domain_ID		client /* IN */,
		 ProtectionDomain_ID	pdid /* IN */,
		 Binder_Port		port /* IN */,
		 const Binder_Cookie *clt_ck /* IN */,
		 Channel_Pair	       *svr_eps	/* OUT */,
		 Binder_Cookie       *svr_ck /* OUT */ )
{
    ot_st 	*st = self->st;
    tbl_entry_t 	*entry;
    bool_t found;

    /* Look up the service ID in this object table and ensure that
       it's a service and not a surrogate */

    MU_LOCK(&st->mu);

    found = WordTbl$Get(st->tbl, (WordTbl_Key)(port),(WordTbl_Val *)&entry);

    MU_RELEASE(&st->mu);
    
    if(found && (entry != NULL) && 
       entry->handle.tag == ObjectTbl_EntryType_Service )
    {
	/* Invoke bind on the service */
	TRC (eprintf ("ObjectTbl$SimpleReq: attempting bind to %x.\n", port));
	TRC (eprintf ("ObjectTbl$SimpleReq: clt_ck.a=%x .w=%x\n",
		      clt_ck->a, clt_ck->w ));
	
	BinderCallback$SimpleRequest (
	    (BinderCallback_clp) entry->handle.u.Service, 
	    client, pdid, port, clt_ck, svr_eps, svr_ck);
    }
    else
    {
	RAISE_Binder$Error (Binder_Problem_BadID);
    }
}

/* 
 * Register callback channel with the binder (called at first export).
 *
 * LL (Locking Level) = st->mu
 */
static void CallbackThread (addr_t data)
{ 
  while(True) {
    TRY 
      {
	Closure$Apply ((Closure_clp) data); 
      }
    CATCH_ALL  /* There is no IDC$Closed to catch */
      {
	/* Clearly there is nobody above us to deal with this 
	   so why don't we just ignore it */
      }
    ENDTRY;
    /* Get back in there! */
  }
}
    
static void
RegisterCallback (ot_st *st)
{
    IDCStubs_Info	 info;
    Binder_Cookie	 tx_buf;
    Binder_Cookie	 rx_buf;
    Heap_clp		 buf_heap;
    ProtectionDomain_ID bd_pdid;

    TRC (eprintf ("OT_Register: called\n"));

    st->registered = True;

    if (VP$DomainID (Pvs(vp)) == DOM_NEMESIS)
	return;	/* no point making an IDC channel to ourselves;
		   callback has already been set up in DomainMgr_Done. */

    TRC (eprintf ("OT_Register: register\n"));

    /* Find the stubs for BinderCallback */
    info = NAME_FIND (IDC_CONTEXT ">BinderCallback", IDCStubs_Info);

    bd_pdid = NAME_FIND("sys>BinderPDom", ProtectionDomain_ID);

    /* Register */
    st->s.ss.service = &st->bcb_cl;
    st->s.ss.binding = &st->s.binding_cl;
    st->s.ss.marshal = ShmIDC;

    st->s.conn       = &st->sconn;
    
    st->s.conn->txbuf.base = NULL;
    st->s.conn->rxbuf.base = NULL;

    st->s.conn->call   = 0;
    st->s.conn->evs.tx = st->s.conn->evs.rx = NULL_EVENT;
    st->s.conn->eps.tx = st->s.conn->eps.rx = NULL_EP; 

    CL_INIT (st->s.cntrl_cl,     info->stub->op,       &st->s);
    CL_INIT (st->s.binding_cl,  &ShmServerBinding_op,  &st->s);

    st->s.offer = NULL;

    TRY
    {
	st->s.conn->evs.tx = EC_NEW();
	st->s.conn->evs.rx = NULL_EVENT; /* we use Entry sync on server side */
	st->s.conn->eps.tx = Events$AllocChannel (Pvs(evs));
	st->s.conn->eps.rx = Events$AllocChannel (Pvs(evs));

	TRC (eprintf ("OT_Register: got channels tx=%x rx=%x\n",
		      st->s.conn->eps.tx, st->s.conn->eps.rx));

	buf_heap  = Gatekeeper$GetHeap (Pvs(gkpr), bd_pdid, 0, 
					SET_ELEM(Stretch_Right_Read), True);
	tx_buf.w = info->s_sz;
	tx_buf.a = Heap$Malloc (buf_heap, tx_buf.w);
	if (! tx_buf.a) RAISE_Heap$NoMemory();
    
	TRC (eprintf ("OT_Register: call Binder_RegisterDomain... \n"));
	Binder$RegisterDomain (st->binder, &st->s.conn->eps, &tx_buf, &rx_buf);
	TRC (eprintf ("tx buf=%x rx buf=%x\n", tx_buf.a, rx_buf.a));

	TRC (eprintf ("OT_Register: attach tx ec\n"));
	Events$Attach(Pvs(evs), st->s.conn->evs.tx, 
		      st->s.conn->eps.tx, Channel_EPType_TX);

	st->s.conn->txbuf.base   = tx_buf.a;
	st->s.conn->txbuf.ptr    = tx_buf.a;
	st->s.conn->txsize       = st->s.conn->txbuf.space = tx_buf.w;
	st->s.conn->txbuf.heap   = st->heap;

	st->s.conn->rxbuf.base   = rx_buf.a;
	st->s.conn->rxbuf.ptr    = rx_buf.a;
	st->s.conn->rxsize       = st->s.conn->rxbuf.space = rx_buf.w;
	st->s.conn->rxbuf.heap   = st->heap;
    
	TRC (eprintf ("OT_Register: init (unused) mutex\n"));
	MU_INIT (&st->s.conn->mu);	/* XXX - not actually used ? */

	/* Register the callback binding with a private entry (which will
	   handle exacty one endpoint: XXX don't really need all the entry
	   chaff here do we? fix binder, etc. XXX */
	st->cbentry = EntryMod$New(st->emod, Pvs(actf), Pvs(vp), 1);
	Entry$Bind (st->cbentry, st->s.conn->eps.rx, &st->s.cntrl_cl);
	
	/* Stick a dedicated thread for our binder callbacks (this should 
	   help the prevention of some more nasty deadlocks) */
	Threads$Fork(Pvs(thds), CallbackThread, Entry$Closure(st->cbentry), 0);

	/* Finally, ensure there's a thread in the standard entry to 
	   service standard things (like the service we're exporting now) */
	Threads$Fork(Pvs(thds), Entry$Closure(Pvs(entry))->op->Apply, 
		     Entry$Closure(Pvs(entry)), 0);
    }
    CATCH_ALL
    {
	if (tx_buf.a) FREE (tx_buf.a);

	if (st->s.conn->evs.tx != NULL_EVENT) EC_FREE (st->s.conn->evs.tx);
	if (st->s.conn->evs.rx != NULL_EVENT) EC_FREE (st->s.conn->evs.rx);
	if (st->s.conn->eps.rx != NULL_EP) 
	    Events$FreeChannel(Pvs(evs), st->s.conn->eps.rx);
	if (st->s.conn->eps.tx != NULL_EP) 
	    Events$FreeChannel(Pvs(evs), st->s.conn->eps.tx);
	RERAISE;
    }
    ENDTRY;
    TRC (eprintf ("OT_Register: done.\n"));
}



/*-----Module Methods---------------------------------------------------*/

/* 
 * Create a new object table.
 */
static ObjectTbl_clp New_m(ObjectTblMod_cl     *self, 
			   Heap_clp       	heap,
			   Binder_clp		binder, /* RETURNS */
			   BinderCallback_clp  *cb )
{
    ot_st			*st;
    WordTblMod_clp	wtm;

    /* Closures and state */
    if ( !(st = Heap$Malloc(heap, sizeof(*st) )) ) {
	DB(eprintf("ObjectTblMod$New: no memory.\n"));
	RAISE_Heap$NoMemory();
    }

    memset(st, 0, sizeof(*st));

    CL_INIT (st->o_cl,   &ot_op,  st);
    CL_INIT (st->bcb_cl, &bcb_op, st);

    st->heap       = heap;
    st->binder     = binder;
    st->emod       = NAME_FIND("modules>EntryMod", EntryMod_clp);

    st->registered = False;

    TRY 
    {
	MU_INIT (&st->mu);
	/* Create the word table */
	wtm     = NAME_FIND("modules>WordTblMod",WordTblMod_clp);
	st->tbl = WordTblMod$New(wtm,heap);
	st->ec = EC_NEW(); 
    }
    CATCH_ALL
    {
	if(st->mu.ec) MU_FREE(&st->mu);
	if (st->tbl) WordTbl$Destroy (st->tbl);
	if(st->ec) EC_FREE(st->ec);
	FREE (st);
	RERAISE;
    }
    ENDTRY;

    *cb = &st->bcb_cl;
    return &st->o_cl;
}

