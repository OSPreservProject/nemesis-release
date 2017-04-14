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
**      mod/fs/usd
**
** FUNCTIONAL DESCRIPTION:
** 
**      What does it do?
** 
** ENVIRONMENT: 
**
**      Disk device driver domain
** 
** FILE :	io.c
** CREATED :	Wed Jan  8 1997
** AUTHOR :	Paul Barham (Paul.Barham@cl.cam.ac.uk)
** ID : 	$Id: io.c 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <time.h>

#include <IOMacros.h>   /* for IO_HANDLE() */

#include <FileIO.ih>

#include <ntsc.h>

#include "usd.h"

#define QTRC(x) 
#define TTRC(x)
#define ETRC(x)
#define IOTRC(_x) 

#ifdef DEBUG
#define TRC(_x) _x
#define FTRC(format, args...)			\
fprintf(stderr,					\
	__FUNCTION__": " format			\
	, ## args				\
      )
#else
#define TRC(_x)
#define FTRC(format, args...)
#endif

static  IO_PutPkt_fn            FileIO_PutPkt_m;
static	IO_AwaitFreeSlots_fn    FileIO_AwaitFreeSlots_m;
static  IO_GetPkt_fn            FileIO_GetPkt_m;
static  IO_QueryDepth_fn        FileIO_QueryDepth_m;
static  IO_QueryKind_fn         FileIO_QueryKind_m;
static  IO_QueryMode_fn         FileIO_QueryMode_m;
static  IO_QueryShm_fn          FileIO_QueryShm_m;
static  IO_QueryEndpoints_fn    FileIO_QueryEndpoints_m;
static  IO_Dispose_fn           FileIO_Dispose_m;
static  FileIO_BlockSize_fn     FileIO_BlockSize_m;
static  FileIO_GetLength_fn     FileIO_GetLength_m;
static  FileIO_SetLength_fn     FileIO_SetLength_m;

static FileIO_op       fileio_ms = {
    FileIO_PutPkt_m,
    FileIO_AwaitFreeSlots_m, 
    FileIO_GetPkt_m,
    FileIO_QueryDepth_m,
    FileIO_QueryKind_m,
    FileIO_QueryMode_m,
    FileIO_QueryShm_m,
    FileIO_QueryEndpoints_m,
    FileIO_Dispose_m,
    FileIO_BlockSize_m,
    FileIO_GetLength_m,
    FileIO_SetLength_m
};

static  IDCOffer_Type_fn                IDCOffer_Type_m;
static  IDCOffer_PDID_fn                IDCOffer_PDID_m;
static  IDCOffer_GetIORs_fn             IDCOffer_GetIORs_m;
static  IDCOffer_Bind_fn                IDCOffer_Bind_m;

static  IDCOffer_op     offer_ms = {
  IDCOffer_Type_m,
  IDCOffer_PDID_m,
  IDCOffer_GetIORs_m,
  IDCOffer_Bind_m
};

/* Client-side stub state, passed from server */
typedef struct {
    IDCOffer_cl offer;
    IDCOffer_clp io_offer;
    uint64_t length;
    uint32_t blocksize;
    ProtectionDomain_ID pdom;
} IDCOffer_st;

typedef struct {
    FileIO_cl            cl;           /* Our FileIO_cl */
    Type_Any             srgt;         /* Surrogate for client to narrow */
    IDCClientBinding_clp binding;      /* Underlying binding [for IO]    */
    Type_Any             io_any;       /* Underlying surrogate [for IO]  */
    IO_clp               io;           /* Underlying IO = NARROW(io_any) */
    uint64_t             length;       /* FileIO state: length of 'file' */
    uint32_t             blocksize;    /*   ""        : block size       */
} FileIO_st;

/*--------------------------------------------- IDCOffer Entry Points ----*/

static Type_Code IDCOffer_Type_m (
        IDCOffer_cl     *self )
{
    return FileIO_clp__code;
}

static ProtectionDomain_ID IDCOffer_PDID_m (
        IDCOffer_cl     *self )
{
    IDCOffer_st   *st = self->st;

    return st->pdom;
}

static IDCOffer_IORList *IDCOffer_GetIORs_m (
        IDCOffer_cl     *self )
{
    printf("USD:io.c:IDCOffer_GetIORs_m: ERROR: UNIMPLEMENTED\n");

    return NULL;
}

static IDCClientBinding_clp IDCOffer_Bind_m (
        IDCOffer_cl     *self,
        Gatekeeper_clp  gk      /* IN */,
        Type_Any        *cl     /* OUT */ )
{
    IDCOffer_st   *st = self->st;
    FileIO_st     *fio;

    FTRC("entered, self=%p, gk=%p, cl=%p.\n", self, gk, cl);

    /* Allocate memory for client-side stuff */
    fio           = Heap$Malloc(Pvs(heap), sizeof(*fio));
    fio->binding  = IDCOffer$Bind(st->io_offer, gk, &fio->io_any);
    fio->io       = NARROW(&fio->io_any,IO_clp);

    CL_INIT(fio->cl, &fileio_ms, fio);

    fio->blocksize = st->blocksize;
    fio->length    = st->length;

    /* XXX SMH: init our client surrogate and return it to client. */
    ANY_INIT(&fio->srgt, FileIO_clp, &fio->cl);
    ANY_COPY(cl, &fio->srgt);    

    return fio->binding;        /* XXX should really invent our own */
}


/*------------------------------------------- Client-side Entry Points ----*/

static bool_t FileIO_PutPkt_m (
    IO_cl       *self,
    uint32_t     nrecs   /* IN */,
    IO_Rec_ptr   recs    /* IN */,
    word_t       value   /* IN */, 
    Time_T       until   /* IN */)
{
    FileIO_st     *st = self->st;

    return IO$PutPkt(st->io, nrecs, recs, value, until);
}

static bool_t FileIO_AwaitFreeSlots_m(IO_cl *self, 
				      uint32_t nslots, 
				      Time_T until)
{
    /* XXX This is probably wrong; fix it if you ever think you'll use it */
    return True;
}

static bool_t FileIO_GetPkt_m (
    IO_cl       *self,
    uint32_t     max_recs  /* IN */,
    IO_Rec_ptr   recs      /* IN */,
    Time_T       until     /* IN */,
    uint32_t    *nrecs     /* OUT */,
    word_t      *value     /* OUT */)
{
    FileIO_st     *st = self->st;
    
    return IO$GetPkt(st->io, max_recs, recs, until, nrecs, value);
}


static uint32_t FileIO_QueryDepth_m (
        IO_cl   *self )
{
    FileIO_st     *st = self->st;
    return IO$QueryDepth(st->io);
}

static IO_Kind FileIO_QueryKind_m (
        IO_cl   *self )
{
    FileIO_st     *st = self->st;
    return IO$QueryKind(st->io);
}

static IO_Mode FileIO_QueryMode_m (
        IO_cl   *self )
{
    FileIO_st     *st = self->st;
    return IO$QueryMode(st->io);
}

static IOData_Shm *FileIO_QueryShm_m (
        IO_cl   *self )
{
    FileIO_st     *st = self->st;
    return IO$QueryShm(st->io);
}

static void FileIO_QueryEndpoints_m (IO_clp self, Channel_Pair *pair) {
    FileIO_st *st = self->st;
    IO$QueryEndpoints(st->io, pair);
}

static void FileIO_Dispose_m (
        IO_cl   *self )
{
    FileIO_st     *st = self->st;
    
    /* First get rid of the real IO */
    IO$Dispose(st->io);
    
    /* ... then get rid of our own data */
    
    /* ... and our client binding? */
}


static uint32_t FileIO_BlockSize_m (
        FileIO_cl       *self )
{
    FileIO_st     *st = self->st;

    return st->blocksize;
}

static FileIO_Size FileIO_GetLength_m (
        FileIO_cl       *self )
{
    FileIO_st     *st = self->st;

    return st->length;
}

static FileIO_RC FileIO_SetLength_m (
        FileIO_cl       *self,
        FileIO_Size     length  /* IN */ )
{
    return FileIO_RC_FAILURE;
}

/* Utility function to create the FileIO offer */
IDCOffer_clp create_fileio_offer(IDCOffer_clp io_offer,
				 uint64_t length, uint32_t blocksize)
{
    IDCOffer_st *st;

    /* XXX use globally readable heap */
    st            = Heap$Malloc(Pvs(heap), sizeof(*st));
    st->io_offer  = io_offer;
    st->length    = length;
    st->blocksize = blocksize;
    st->pdom      = VP$ProtDomID(Pvs(vp));
    CL_INIT(st->offer, &offer_ms, st);

    return &st->offer;
}

uint64_t get_fileio_length(IDCOffer_clp offer)
{
    IDCOffer_st *st = offer->st; 

    return st->length;
}

void set_fileio_length(IDCOffer_clp offer, uint64_t length)
{
    IDCOffer_st *st = offer->st; 

    st->length = length; 
    return;
}

/* Utility function to destory the FileIO offer */
void free_fileio_offer(IDCOffer_clp io_offer)
{
    IDCOffer_st *st = (IDCOffer_st *)io_offer->st;

    Heap$Free(Pvs(heap), st);
    return;
}


/* ---------------------------------- Server side IO implementation ---- */
PUBLIC void IOThread(disk_t *disk)
{
    stream_t *stream;
    QoSEntry_clp qosentry = disk->qosentry;
    Time_T pre,post;
    IO_clp io;
    IO_Rec recs[2]; 
    uint32_t nrecs;
    word_t value;
    uint32_t offset;
    Time_ns cost;
    uint32_t  notify;
    USD_Extent ext;

    FileIO_Request copyreq;
    FileIO_Request *req;

    while (True) {
	TRC(printf(__FUNCTION__": about to Rendezvous...\n"));
	io = IOEntry$Rendezvous((IOEntry_clp)qosentry, FOREVER);

	/* 
	** Account the time we're about to spend processing this packet
	** to the QosEntry: for now, we just are sych. and so can 
	** actually count the amount of time. XXX no overhead accounted.
	*/
	pre = NOW();

	TRC(printf(__FUNCTION__": back from Rdzvs, io at %p\n", io));

	/* Find out whose IO it is */
	stream = (stream_t *)IO_HANDLE(io);
	offset = stream->usd->partition->p_offset;

	TRC(printf("(It's stream %d)\n",stream->sid));

	/* XXX SMH: nasty hardwired request/buffer IO_Rec pair */
	if (IO$GetPkt(io, 2, recs, 0, &nrecs, &value)) {
	    
	    FTRC("got packet.\n");

	    req = (FileIO_Request *)recs[0].base;
	
	    copyreq = *req;

	    IOTRC(printf("req=%p\n",req));

	    IOTRC(printf("%qx:(%qd,%d)\n", stream->sid, 
			 copyreq.blockno, copyreq.nblocks));
	
	    /* Translate the addresses in the request */
	    if(!USDCallback$Translate(stream->usd->callbacks,
				      stream->sid,
				      stream->cid,
				      &copyreq,
				      &ext,
				      &notify))
	    {
		TRC(printf("denied %qx:(%qd,%x)\n", stream->sid,
			   copyreq.blockno, copyreq.nblocks));
		req->rc = FileIO_RC_DENIED;
		goto denied;
	    }

	    /* Actually do the transaction */
	    switch(copyreq.op) {
	    case FileIO_Op_Read:
		FTRC("trying to read.\n");
		MU_LOCK(&disk->mu);
		if (BlockDev$Read((BlockDev_clp)disk->device, 
			      ext.base + offset, 
			      ext.len, 
				  recs[1].base)) {
		  req->rc = FileIO_RC_OK;
		  FTRC("done.\n");
		}
		else {
		  req->rc = FileIO_RC_FAILURE;
		  FTRC("failure trying to read.\n");
		}
		MU_RELEASE(&disk->mu);
		break; 
		
	    case FileIO_Op_Write:
		FTRC("trying to write.\n");
		MU_LOCK(&disk->mu);
		if (BlockDev$Write((BlockDev_clp)disk->device, 
			       ext.base + offset, 
			       ext.len, 
				   recs[1].base)) {
		  req->rc = FileIO_RC_OK;
		  FTRC("done.\n");
		}
		else {
		  req->rc = FileIO_RC_FAILURE;
		  FTRC("failure trying to write.\n");
		}
		MU_RELEASE(&disk->mu);
		break;

	    case FileIO_Op_Sync:
		FTRC("trying to sync.\n");
		printf(__FUNCTION__": FileIO_Op_Sync unimplemented.\n");
		FTRC("done.\n");
		break;

	    default:
		printf(__FUNCTION__": bad FileIO_Op %x\n", copyreq.op);
		ntsc_dbgstop();
	    }

	    USDCallback$Notify(stream->usd->callbacks,
			       stream->sid,
			       stream->cid,
			       &copyreq,
			       &ext,
			       notify,
			       recs[1].base);
	denied:
	    post = NOW();
	    cost = post - pre;
	    QoSEntry$Charge(qosentry, io, cost);
	
	    /* Return the IO_Rec vector */
	    TRC(printf("USD$IOThread: returning IOs.\n"));
	    IO$PutPkt(io, nrecs, &recs[0], cost, 0);
	}    
    } /* while true */

    printf("USD$IOThread: BAD NEWS. Exiting (?)\n");
}
