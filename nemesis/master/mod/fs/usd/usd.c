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
**      mod/fs/usd
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Device driver top-end providing user-safe access to disk.
** 
** ENVIRONMENT: 
**
**      Module within driver domain
** 
** ID : $Id: usd.c 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <IOMacros.h>
#include <IDCMacros.h>
#include <stdio.h>
#include <links.h>
#include <time.h>

#include <ntsc.h>

#include "usd.h"

// #define DEBUG

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
/* extern const QoSEntryMod_cl daves_qosentry_cl; */

/*****************************************************************/
/*
 * Module stuff : Creator method
 */
#include <USDMod.ih>

static USDMod_New_fn	New;
static USDMod_op	ms = { New };
static const USDMod_cl	cl = { &ms, BADPTR };

CL_EXPORT (USDMod, USDMod, cl);

/*****************************************************************/
/*
 * USD Control interface
 */

#include <USDCtl.ih>

static  USDCtl_CreateStream_fn          USDCtl_CreateStream_m;
static  USDCtl_DestroyStream_fn         USDCtl_DestroyStream_m;
static  USDCtl_AdjustQoS_fn             USDCtl_AdjustQoS_m;
static  USDCtl_GetLength_fn             USDCtl_GetLength_m;
static  USDCtl_SetLength_fn             USDCtl_SetLength_m;
static  USDCtl_Request_fn               USDCtl_Request_m;

static  USDCtl_op       usd_ctl_ms = {
    USDCtl_CreateStream_m,
    USDCtl_DestroyStream_m,
    USDCtl_AdjustQoS_m,
    USDCtl_GetLength_m,
    USDCtl_SetLength_m,
    USDCtl_Request_m
};

/*****************************************************************/
/*
 * USD Drive interface
 */

#include <USDDrive.ih>

static  USDDrive_GetDiskInfo_fn                 USDDrive_GetDiskInfo_m;
static  USDDrive_GetPartitionInfo_fn            USDDrive_GetPartitionInfo_m;
static  USDDrive_GetWholeDisk_fn                USDDrive_GetWholeDisk_m;
static  USDDrive_GetPartition_fn                USDDrive_GetPartition_m;
static  USDDrive_RescanPartitions_fn            USDDrive_RescanPartitions_m;

static  USDDrive_op     usd_drive_ms = {
  USDDrive_GetDiskInfo_m,
  USDDrive_GetPartitionInfo_m,
  USDDrive_GetWholeDisk_m,
  USDDrive_GetPartition_m,
  USDDrive_RescanPartitions_m
};

/*****************************************************************/
/*
 * IDCCallback for bind to USDCtl
 */

#include <IDCCallback.ih>

static  IDCCallback_Request_fn          IDCCallback_Request_m;
static  IDCCallback_Bound_fn            IDCCallback_Bound_m;
static  IDCCallback_Closed_fn           IDCCallback_Closed_m;

static  IDCCallback_op  usd_callback_ms = {
  IDCCallback_Request_m,
  IDCCallback_Bound_m,
  IDCCallback_Closed_m
};

/*---------------------------------------------------- Entry Points ----*/

static USDDrive_clp New (USDMod_clp self, 
			 Disk_clp   d, 
			 string_t   n)
{
    disk_t           *disk;
    sched_params_t   *sp;
    word_t            i;
    IDCTransport_clp  shmt;

    FTRC("entered: self=%p, disk=%p, name=\"%s\".\n",
	 self, d, n);

    FTRC("getting shm transport.\n");
    shmt = NAME_FIND("modules>ShmTransport", IDCTransport_clp);

    FTRC("allocating disk.\n");
    disk = (disk_t *)Heap$Malloc(Pvs(heap), sizeof(disk_t));
    disk->device = d;
    MU_INIT(&(disk->mu));

    FTRC("getting disk parameters.\n");
    Disk$GetParams(disk->device, &disk->params);

    FTRC("setting name.\n");
    strncpy(disk->name, n, 31);
    disk->name[31] = '\0';           
  
    FTRC("clearing partitions.\n");
    /* Clear list of partitions */
    for (i=0; i < USD_MAX_PARTITIONS; i++)
    {
	disk->partition[i] = NULL;
    }
    FTRC("clearing disk.\n");
    disk->whole_disk = NULL;

    FTRC("sorting out  partition table.\n");
    /* look for an MSDOS-style partition table first */
    if (msdos_partition(disk)) 
    {
	FTRC("MS-DOS.\n");
	disk->msdos_ptab = True;
    } 
    else 
    {
	FTRC("not MS-DOS.\n");
	/* if not, must (we hope!) be an alpha disk */
	if (!osf1_partition(disk)) 
	{
	    FTRC("Not OSF/1.\n");
	    printf("USD: Couldn't find partition table\n");
	    disk->label.d_npartitions = 0;
	}
	FTRC("OSF/1.\n");
	disk->msdos_ptab = False;
    } 

    /* Amount of contracted time */
    FTRC("zero-ing contracted.\n");
    disk->contracted = 0;

    /* Leaky bucket stuff */
    FTRC("leaky buckets.\n");
    MU_INIT(&(disk->besteffort.mu));
    sp                = &disk->besteffort;
    sp->credits       = 0;
    sp->creditspersec = MAX_CREDITS_PER_SECOND;
    sp->nspercredit   = MILLISECS(1000) / MAX_CREDITS_PER_SECOND;
    sp->last          = NOW();
    sp->lastdbg       = sp->last;


    /* Initialise stream DS */
    FTRC("initialising streams.\n");
    MU_INIT(&(disk->smu));
    for (i=0; 
	 i<USD_MAX_STREAMS; 
	 i++) 
    {
	stream_t *s = &disk->stream[i];
	
	s->sid    = i;
	s->free   = True;
	s->qos.p  = s->qos.s = s->qos.x = 0;
	MU_INIT(&s->params.mu);
    }


    FTRC("creating entry.\n");
    {
	QoSEntryMod_clp qosem;
	qosem = NAME_FIND("modules>QoSEntryMod", QoSEntryMod_clp);

	disk->qosentry = QoSEntryMod$New(qosem, Pvs(actf), Pvs(vp),
					 USD_MAX_STREAMS * 4);
	FTRC("created entry at %p.\n", disk->qosentry);
    }

    /* Export USDCtl interfaces */
    FTRC("exporting usdctls.\n");
    for (i=0; 
	 i<disk->label.d_npartitions; 
	 i++) 
    {
	Type_Any  any;
	usd_st   *usd;

	FTRC("%d: setting label.\n", i);		
	if (disk->label.d_partitions[i].p_size == 0) 
	{
	    disk->partition[i]=NULL;
	    continue;
	}

	FTRC("%d: allocating usd.\n", i);
	usd = (usd_st *)Heap$Malloc(Pvs(heap), sizeof(usd_st));

	FTRC("%d: setting up stuff.\n", i);
	disk->partition[i] = usd;
	usd->number        = i;

	CL_INIT(usd->control, &usd_ctl_ms, usd);
	usd->disk      = disk;
	usd->partition = &disk->label.d_partitions[i];

	usd->binding = NULL;

	CL_INIT(usd->callback, &usd_callback_ms, usd);

	FTRC("%d: exporting %8d +%8d fstype %d\n", 
	     i,
	     usd->partition->p_offset,
	     usd->partition->p_size,
	     usd->partition->p_fstype);
	
	ANY_INIT(&any, USDCtl_clp, &usd->control);
	usd->offer = IDCTransport$Offer(shmt, &any, &usd->callback,
					Pvs(heap), Pvs(gkpr),
					Pvs(entry), &usd->service);

    } 
    
    FTRC("whole disk\n", i);
    /* Set up the whole disk ctl */
    {
	Type_Any  any;
	usd_st   *usd;
	
	FTRC("whole_disk: allocating usd.\n");;
	usd = (usd_st *)Heap$Malloc(Pvs(heap), sizeof(usd_st));

	FTRC("whole disk: setting up stuff.\n");
	disk->whole_disk = usd;
	usd->number      = disk->label.d_npartitions;

	CL_INIT(usd->control, &usd_ctl_ms, usd);
	usd->disk      = disk;
	usd->partition = Heap$Malloc(Pvs(heap), sizeof(struct d_partition));
	usd->partition->p_offset = 0;
	usd->binding   = NULL;

	CL_INIT(usd->callback, &usd_callback_ms, usd);
	
	FTRC("whole_disk: exporting %8d +%8d fstype %d\n", 
	     usd->partition->p_offset,
	     usd->partition->p_size,
	     usd->partition->p_fstype);

	ANY_INIT(&any, USDCtl_clp, &usd->control);
	usd->offer = IDCTransport$Offer(shmt, &any, &usd->callback,
					Pvs(heap), Pvs(gkpr),
					Pvs(entry), &usd->service);

	
    }


    FTRC("setting up closure.\n");
    /* Set up USDDrive closure */
    CL_INIT(disk->drive, &usd_drive_ms, disk);
    
    FTRC("forking IOThread.\n");
    /* Fork the Disk IO scheduler thread */
    Threads$Fork(Pvs(thds), IOThread, disk, 0);

    FTRC("finished, returning %p.\n", &disk->drive);
    return &disk->drive;
}

/*-------------------------------------------- USDDrive Entry Points ----*/

static bool_t USDDrive_GetDiskInfo_m (
        USDDrive_cl     *self,
	USD_DiskInfo    *info /* OUT */ )
{
    disk_t *st = self->st;

    info->label      = strdup(st->name);         /* XXX leak */
    info->blocksize  = st->params.blksz; 
    info->partitions = st->label.d_npartitions;
    info->size       = st->params.blocks;

    return(True); 
}

static const char * const osf_fstypenames[] = {
        "unused",
        "swap",
        "Version 6",
        "Version 7",
        "System V",
        "4.1BSD",
        "Eighth Edition",
        "4.2BSD",
        "ext2",
        "resrvd9",
        "resrvd10",
        "resrvd11",
        "resrvd12",
        "resrvd13",
        "resrvd14",
        "resrvd15",
        "ADVfs",
        "LSMpubl",
        "LSMpriv",
        "LSMsimp",
        0
};

static bool_t USDDrive_GetPartitionInfo_m (
    USDDrive_cl       *self,
    uint32_t           partition /* IN  */,
    USD_PartitionInfo *info      /* OUT */)
{
    disk_t      *st = self->st;
    extern char *msdos_fstypenames(uint8_t ptype);

    if ((partition >= st->label.d_npartitions) || 
	!st->partition[partition]) 
    {
	TRC(printf("USD: GetPartitionInfo: bad partition %d on disk %s\n", 
		   partition, st->name));
	return(False);
    }


    info->ostype    = st->partition[partition]->partition->p_fstype;

    if(st->msdos_ptab) 
    {
	info->osname = strdup(msdos_fstypenames((uint8_t)info->ostype));
    }
    else
    {
	info->osname = strdup(osf_fstypenames[info->ostype]);
    }

    info->blocksize = st->params.blksz; 
    info->size      = st->partition[partition]->partition->p_size; 
    
    return(True); 
}

static bool_t USDDrive_GetWholeDisk_m (
        USDDrive_cl     *self,
	USDCallback_clp  callbacks /* IN */
	/* RETURNS */,
	IDCOffer_clp    *ctl)
{
    disk_t         *st=self->st;


    /* XXX we currently trust this closure */
    st->whole_disk->callbacks = callbacks;
    USDCallback$Initialise(callbacks);
    
    *ctl = st->whole_disk->offer;
    return(True);
}

static bool_t USDDrive_GetPartition_m (
        USDDrive_cl     *self,
	uint32_t         partition /* IN */,
	USDCallback_clp  callbacks /* IN */
	/* RETURNS */,
	IDCOffer_clp    *ctl)
{
    disk_t *st=self->st;


    if ((partition >= st->label.d_npartitions) || 
	!st->partition[partition]) 
    {
	TRC(printf("USD: GetPartition: request for bad partition %d on disk"
		   " %s\n",partition,st->name));
	return(False);
    }

    TRC(printf("USD: GetPartition(%d)\n",partition));

    /* XXX we currently trust this closure */
    st->partition[partition]->callbacks = callbacks;
    USDCallback$Initialise(callbacks);

    *ctl = st->partition[partition]->offer;
    return(True);
}

static bool_t USDDrive_RescanPartitions_m (
        USDDrive_cl     *self )
{
    printf("USD: RescanPartitions: unimplemented\n");
    return False;
}


/*----------------------------------------- IDCCallback Entry Points ----*/

static bool_t IDCCallback_Request_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        Domain_ID       dom     /* IN */,
        ProtectionDomain_ID    pdom    /* IN */,
        const Binder_Cookies    *clt_cks        /* IN */,
        Binder_Cookies  *svr_cks        /* OUT */ )
{
    usd_st *st = self->st;

    if (st->binding) {
	printf("USD: dom %qx tried to bind to %s/%d (refused; inuse)\n",
	       dom, st->disk->name, st->number);

	return False;
    }

    TRC(printf("USD: dom %qx tried to bind to %s/%d (accepted)\n",
	       dom, st->disk->name, st->number));
    return True;
}

static bool_t IDCCallback_Bound_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        IDCServerBinding_clp    binding /* IN */,
        Domain_ID       dom     /* IN */,
        ProtectionDomain_ID    pdom    /* IN */,
        const Binder_Cookies    *clt_cks        /* IN */,
        Type_Any        *server /* INOUT */ )
{
    usd_st *st = self->st;

    st->binding = binding;

    return True;
}

static void IDCCallback_Closed_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        IDCServerBinding_clp    binding /* IN */,
        const Type_Any  *server /* IN */ )
{
    usd_st *st = self->st;

    /* XXX we should close all of the streams associated with this
       partition */

    TRC(printf("USD: binding to %s/%d has been closed.\n",
	       st->disk->name, st->number));
    st->binding = NULL;
}

/*-------------------------------------- IDCCallback Entry Points ----*/


/*
 * IDCCallback methods 
 */
static	IDCCallback_Request_fn 		USD_Request_m;
static	IDCCallback_Bound_fn 		USD_Bound_m;
static	IDCCallback_Closed_fn 		USD_Closed_m;

static	IDCCallback_op	usdio_callback_ms = {
    USD_Request_m,
    USD_Bound_m,
    USD_Closed_m
};

static bool_t USD_Request_m (IDCCallback_cl *self, IDCOffer_clp offer,
			     Domain_ID dom, ProtectionDomain_ID pdom,
			     const Binder_Cookies *clt_cks, 
			     Binder_Cookies *svr_cks)
{
    return True;
}

static bool_t USD_Bound_m (IDCCallback_cl *self, IDCOffer_clp offer,
			   IDCServerBinding_clp binding, Domain_ID dom,
			   ProtectionDomain_ID pdom, 
			   const Binder_Cookies *clt_cks, Type_Any *server)    
{
    stream_t *s  = self->st;
    IO_cl    *io = NARROW(server, IO_clp);

    TRC(printf("IDCCallback$Bound(%p, %p) (SID=%qd)\n", self, io, s->sid));
    s->io = io; 
    QoSEntry$SetQoS(s->usd->disk->qosentry, io, 
		    s->qos.p, s->qos.s, s->qos.x, s->qos.l);
    TRC(printf(__FUNCTION__": leaving.\n"));
    return True; 
}

static void USD_Closed_m (IDCCallback_cl *self, IDCOffer_clp offer,
			  IDCServerBinding_clp binding, const Type_Any *server)
{
    return;
}



/*---------------------------------------------- USDCtl Entry Points ----*/

static USDCtl_Error 
USDCtl_CreateStream_m (
    USDCtl_cl       *self,
    USD_ClientID    cid     /* IN */,
    /* RETURNS */ 
    USD_StreamID    *ret_sid,
    IDCOffer_clp    *offer )
{
    usd_st     	 *           st  = self->st;
    USD_StreamID   NOCLOBBER sid = 0;
    stream_t     * NOCLOBBER s;

    TRC(printf("USD_CreateStream_m (%p, %lx)\n", self, cid));

    /* Find an unused stream */
    USING(MUTEX, &(st->disk->smu),
	  while(sid < USD_MAX_STREAMS && 
		!st->disk->stream[sid].free) 
	  {
	      sid++;
	  }

	  if (sid < USD_MAX_STREAMS) 
	  {
	      s        = &st->disk->stream[sid];
	      s->free  = False;
	      s->qos.p = MILLISECS(100);
	      s->qos.s = 0;
	  } 
	);

    if (sid == USD_MAX_STREAMS) 
    {
	TRC(printf("USD_CreateStream_m: no streams free\n"));
	return(USDCtl_Error_NoResources);
    }

    TRC(printf("USD_CreateStream_m: got stream_t *s = %p\n", s));
    s->usd     = st; 
    s->cid     = cid;

    /* SMH: init s->io to NULL so don't get problems with AdjustQoS */
    s->io     = NULL;

    s->qos.p  = FOREVER;
    s->qos.s  = 0;
    s->qos.x  = True;

    /* Set up an update stream offer */
    {
	IOTransport_clp	 iot;
	IOData_Shm      *shm;

	TRC(printf("USD_CreateStream_m: setting up update stream offer\n"));

	shm   = SEQ_NEW(IOData_Shm, 1, Pvs(heap)); 
	SEQ_ELEM(shm, 0).buf.base     = NO_ADDRESS; /* not yet allocated */
	SEQ_ELEM(shm, 0).buf.len      = 1024*1024;  /* XXX:-(= */
	SEQ_ELEM(shm, 0).param.attrs  = 0;
	SEQ_ELEM(shm, 0).param.awidth = PAGE_WIDTH;
	SEQ_ELEM(shm, 0).param.pwidth = PAGE_WIDTH;

	CL_INIT(s->scb, &usdio_callback_ms, s);

	iot = NAME_FIND ("modules>IOTransport", IOTransport_clp);
	s->io_offer = (IDCOffer_clp) 
	    IOTransport$Offer(iot, 
			      Pvs(heap), 
			      128, 
			      IO_Mode_Dx, 
			      IO_Kind_Master, 
			      shm, 
			      Pvs(gkpr), 
			      &s->scb, 
			      (IOEntry_clp)(st->disk->qosentry), 
			      (word_t)s, 
			      &s->io_service);

	s->offer = create_fileio_offer(s->io_offer, st->partition->p_size,
				       st->disk->params.blksz);
    }

    TRC(printf("USD_CreateStream_m: Done\n"));
    *offer   = s->offer;
    *ret_sid = sid;
    return(USDCtl_Error_None);
}

static USDCtl_Error USDCtl_DestroyStream_m (USDCtl_cl       *self,
					    USD_StreamID    sid     /* IN */ )
{
    usd_st               *st     = (usd_st *)self->st;
    stream_t *NOCLOBBER   s      = (stream_t *)NULL;
    NOCLOBBER bool_t      badsid = False;
    USD_QoS               antiqos;

    /* Check for the stream identified by 'sid' */
    USING(MUTEX, &(st->disk->smu),
	  if(st->disk->stream[sid].free) 
	  {
	      badsid = True;
	  } 
	  else 
	  {
	      s       = &st->disk->stream[sid];
	      s->free = True;
	  }
	);

    /* Sanity checks */
    if(badsid) 
    {
	printf("USD_DestroyStream_m: bad sid %qd\n", sid);
	return(USDCtl_Error_DoesNotExist);
    }

    if(!s->offer) 
    {
	printf("USD_DestroyStream_m: sid %qd had no offer ?!?\n", sid);
	return(USDCtl_Error_Failure);
    }

    /* Cuterize ss->io */
    if(s->io) 
	s->io = NULL;
	
    /* Anti-Adjust QoS (will this work?) */
    antiqos.p = s->qos.p;
    antiqos.s = 0;
    USDCtl_AdjustQoS_m (self, sid, &antiqos);

    /* Free the resources with our FileIO offer */
    free_fileio_offer(s->offer);

    return(USDCtl_Error_None);
}


static USDCtl_Error USDCtl_AdjustQoS_m (USDCtl_cl       *self,
					USD_StreamID     sid     /* IN */,
					const USD_QoS   *q       /* IN */ )
{
    usd_st    *st = self->st;
    stream_t  *s  = &st->disk->stream[sid];
    int        new_percent, sid_percent;

    printf("USDCtl_AdjustQoS_m client %d -> (%d, %d, %d, %s)\n",
	   (int)sid,
	   (int)(q->p/MILLISECS(1)),
	   (int)(q->s/MILLISECS(1)),
	   (int)(q->l/MILLISECS(1)),
	   q->x ? "true": "false");

    /* We need better Admission control: currently based on percentages! */
    sid_percent = (s->qos.s * 100) / s->qos.p;
    new_percent = (q->s * 100) / q->p;

    if ((st->disk->contracted + new_percent - sid_percent) > 100) 
    {
	TRC(printf("USDCtl_AdjustQoS_m: no resources\n"));
	return(USDCtl_Error_NoResources);
    }

    memcpy(&s->qos, q, sizeof(USD_QoS));

    st->disk->contracted -= sid_percent;
    st->disk->contracted += new_percent;

    if (s->io) {
	TRC(printf("USDCtl_AdjustQoS_m: about to config QoSEntry\n"));
	QoSEntry$SetQoS(st->disk->qosentry, s->io, 
			s->qos.p, s->qos.s, s->qos.x, s->qos.l);
    }

    printf("USD: Contracted now %d%%\n", st->disk->contracted);
    return(USDCtl_Error_None);
}

static USDCtl_Error USDCtl_GetLength_m(USDCtl_cl *self, USD_StreamID sid, 
				       /* OUT */ FileIO_Size *length)
{
    usd_st    *st = self->st;
    stream_t  *s  = &st->disk->stream[sid];

    TRC(printf("USDCtl_GetLength_m (%p, %qx)\n", self, sid));
    
    *length = get_fileio_length(s->offer);
    return(USDCtl_Error_None);
}

static USDCtl_Error USDCtl_SetLength_m(USDCtl_cl *self, USD_StreamID sid, 
				       FileIO_Size length)
{
    usd_st    *st = self->st;
    stream_t  *s  = &st->disk->stream[sid];

    TRC(printf("USDCtl_SetLength_m (%p, %qx, %qx)\n", self, sid, length));

    /* XXX SMH: sanity check not > partition size? */
    
    set_fileio_length(s->offer, length); 
    return(USDCtl_Error_None);
}


static USDCtl_Error USDCtl_Request_m(USDCtl_cl        *self,
				     const USD_Extent *extent, /* IN */
				     FileIO_Op         type,   /* IN */
				     addr_t            buffer  /* IN */ )
{
    usd_st    *st   = self->st;
    disk_t    *disk = st->disk;
    bool_t     rc;
    Time_ns    pre, post, cost;

    pre = NOW();

    switch(type) {
    case FileIO_Op_Read:
	MU_LOCK(&disk->mu);
	rc = BlockDev$Read((BlockDev_clp)disk->device, 
			   extent->base + st->partition->p_offset, 
			   extent->len, 
			   buffer); 
	MU_RELEASE(&disk->mu);
	break;
	
    case FileIO_Op_Write:
	MU_LOCK(&disk->mu);
	rc = BlockDev$Write((BlockDev_clp)disk->device, 
			    extent->base + st->partition->p_offset, 
			    extent->len, 
			    buffer); 
	MU_RELEASE(&disk->mu);
	break;
	
    case FileIO_Op_Sync:
	printf(__FUNCTION__": FileIO_Op_Sync unimplemented.\n");
	return(USDCtl_Error_Failure);

	
    default:
	printf(__FUNCTION__": bad FileIO_Op %x\n", type);
	ntsc_dbgstop();
	return(USDCtl_Error_Failure);
    }

    post = NOW();
    cost = post - pre;
    QoSEntry$Charge(disk->qosentry, 0, cost);

    return(USDCtl_Error_None);
}

/*
 * End 
 */
