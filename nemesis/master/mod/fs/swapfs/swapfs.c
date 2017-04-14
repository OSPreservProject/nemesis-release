/*
*****************************************************************************
**									    *
**  Copyright 1998, University of Cambridge Computer Laboratory		    *
**									    *
**  All Rights Reserved.						    *
**									    *
*****************************************************************************
**
** FACILITY:
**
**	mod/fs/swapfs
**
** FUNCTIONAL DESCRIPTION:
** 
**	Swap 'filesystem': per-domain swap space in a partition.
**      
** ENVIRONMENT: 
**
**	User space server
** 
** ID : $Id: swapfs.c 1.2 Thu, 18 Feb 1999 15:08:33 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <bstring.h>
#include <salloc.h>

#include <FSTypes.ih>
#include <USDCallback.ih>
#include <USDCtl.ih>
#include <USD.ih>
#include <USDDrive.ih>

#include <IDCMacros.h>


#include <Swap.ih>
#include <SwapFS.ih>

#include <ecs.h>
#include <time.h>
#include <links.h>

#define TRC(_x) 
#define DB(_x) _x

typedef struct _extinfo {
    struct _extinfo *next; 
    struct _extinfo *prev; 
    USD_Extent       extent; 
} extinfo_t; 

typedef struct _client {
    struct _client *next; 
    struct _client *prev; 
    USD_Extent      extent; 
    USD_ClientID    sid; 
    FSTypes_QoS     qos;     /* QoS, + also need storage for 'CurrentQos'  */
    IDCOffer_clp    offer; 
} client_t; 

#define MAX_CLTS 16 


typedef struct _swapfs_st 
{
    Swap_cl              cl;             /* Space for our closure       */

    extinfo_t            free;           /* List of free extents space  */
    client_t             clients;        /* List of clients/extents     */

    Swap_Handle          next_hdl;       /* Hint as to next free handle */
    client_t            *hdls[MAX_CLTS]; /* Index for handles           */


    /* A heap for filesystem state */
    Heap_clp	         heap;

    /* Disk-related things */
    IDCClientBinding_clp drive_binding;
    USDDrive_clp         usddrive;
    uint32_t             blocksize; 

    /* Partition-related things */
    uint32_t             partition;      /* Partition number (idx)         */
    uint32_t             part_size;      /* In disk blocks                 */
    IDCClientBinding_clp part_binding;   /* Binding for USDCtl             */
    USDCtl_clp	         usdctl;         /* Handle of USDCtl               */
    USDCallback_cl       usdcallback;    /* Fault handler for TLB misses   */
    
} swapfs_st; 



typedef struct USDCallback_st_struct {
} USDCallback_st;

static USDCallback_Initialise_fn       USDCallback_Initialise_m;
static USDCallback_Dispose_fn          USDCallback_Dispose_m;
static USDCallback_Translate_fn        USDCallback_Translate_m;
static USDCallback_Notify_fn           USDCallback_Notify_m;

static USDCallback_op usd_cb_ms = {
    USDCallback_Initialise_m,
    USDCallback_Dispose_m,
    USDCallback_Translate_m,
    USDCallback_Notify_m
};

/*static const USDCallback_cl usd_cb_cl = { &usd_cb_ms, NULL }; */

/* The callback that likes to say yes. */
static void USDCallback_Initialise_m (
        USDCallback_cl  *self )
{

}

static void USDCallback_Dispose_m (
        USDCallback_cl  *self )
{
    
}

static bool_t USDCallback_Translate_m (
        USDCallback_cl  *self,
        USD_StreamID    sid     /* IN */,
        USD_ClientID    cid     /* IN */,
        const FileIO_Request    *req    /* IN */,
        USD_Extent      *extent /* OUT */,
        uint32_t        *notification   /* OUT */ )
{
    client_t *client = (client_t *)(word_t)cid; 
    if(req->blockno + req->nblocks > client->extent.len) {
	DB(eprintf("Bad client %p: extent is %qx,%qx but req was %qx,%qx\n", 
		   client, client->extent.base, 
		   client->extent.base + client->extent.len, 
		   req->blockno, req->blockno + req->nblocks)); 
	return False;
    }

    extent->base = client->extent.base + req->blockno; 
    extent->len  = req->nblocks; 
    TRC(eprintf("Translated: req %qx,%qx to extent %qx,%qx\n", 
		req->blockno, req->blockno + req->nblocks,  
		extent->base, extent->len));
    return(True);
}

static void USDCallback_Notify_m (
        USDCallback_cl  *self,
        USD_StreamID    sid     /* IN */,
        USD_ClientID    cid     /* IN */,
        const FileIO_Request    *req    /* IN */,
        const USD_Extent        *extent /* IN */,
        uint32_t        notification    /* IN */,
        addr_t  data    /* IN */ )
{
}


static  Swap_BlockSize_fn  BlockSize_m; 
static  Swap_Free_fn       Free_m; 
static  Swap_CurrentQoS_fn CurrentQoS_m;
static  Swap_Open_fn       Open_m;
static  Swap_AdjustQoS_fn  AdjustQoS_m;
static  Swap_Close_fn      Close_m;

static  Swap_op           swap_ms = {
    BlockSize_m, 
    Free_m, 
    CurrentQoS_m, 
    Open_m, 
    AdjustQoS_m, 
    Close_m,
};

static	SwapFS_New_fn   New_m;
static	SwapFS_op       new_ms = {
    New_m,
};

static	const SwapFS_cl new_cl = { &new_ms, NULL };

CL_EXPORT (SwapFS, swapfs, new_cl);




static IDCOffer_clp New_m(SwapFS_cl *self, IDCOffer_clp drive,
			  uint32_t partition)
{
    IDCTransport_clp  shmt;
    USD_PartitionInfo info;
    IDCService_clp    service;
    IDCOffer_clp      part_offer, res; 
    Type_Any          any;
    Heap_clp          heap;
    extinfo_t        *ei; 
    swapfs_st        *st; 
    int i;

    TRC(printf("swapfs: mount %d from %p\n", partition, drive));
    /* It's probably a good idea to have a separate heap for the filesystem.
       For now let's just use Pvs(heap), but eventually create a stretch
       of our own. */

    heap = Pvs(heap);

    if(!(st = Heap$Calloc(heap, 1, sizeof(*st)))) {
	DB(printf("swapfs: cannot allocate state.\n"));
	return (IDCOffer_cl *)NULL;
    }

    st->heap      = heap;

    /* Initialise our handle stuff */
    st->next_hdl = 0; 
    for(i = 0; i < MAX_CLTS; i++) 
	st->hdls[i] = NULL;

    /* Bind to the USDDrive. */
    st->drive_binding = IDCOffer$Bind(drive, Pvs(gkpr), &any);
    st->usddrive      = NARROW(&any, USDDrive_clp);
 
    TRC(printf("swapfs: state at [%p, %p]\n",st, (void *)st + sizeof(*st)));
    DB(printf("swapfs: debugging output is switched on\n"));

    /* Find out some information about our partition. */
    TRC(printf("swapfs: fetching partition info\n"));
    if(!USDDrive$GetPartitionInfo(st->usddrive, partition, &info)) {
	printf("swapfs: failed to get partition info for partition %d\n", 
		partition); 
	return (IDCOffer_cl *)NULL;
    }

    /* XXX Yuk! We don't even know if our ptab is OSF or MSDOS. */
#define OSF_FS_SWAP         1     /*  swap partition under OSF */
#define MSDOS_FS_SWAP     0x82    /*  Linux swap partition            */
#define MSDOS_FS_EXT2     0x83    /*  Linux native (ext2fs) partition */
#define MSDOS_FS_AMOEBA   0x93    /*  Amoeba (aka native Nemesis)     */
    if((info.ostype != MSDOS_FS_AMOEBA) &&
       (info.ostype != MSDOS_FS_SWAP) &&
       (info.ostype != OSF_FS_SWAP)) {  
	DB(printf("swapfs: partition %d is not a swap partition!\n", 
		  partition));
	return (IDCOffer_cl *)NULL;
    }

    /* Ok: done basic sanity checks. Now copy over the partition info */
    st->partition = partition;
    st->blocksize = info.blocksize;
    st->part_size = info.size;
    TRC(printf("swapfs: blocksize %d, partition size %d blocks,\n"
	       "	partition type %s\n",
	       st->blocksize, 
	       st->part_size,
	       info.osname));
    FREE(info.osname);


    /* Connect to the disk */
    TRC(printf("swapfs: Getting USDCtl offer\n"));
    CL_INIT(st->usdcallback, &usd_cb_ms, st);
    if(!USDDrive$GetPartition(st->usddrive, st->partition, &st->usdcallback,
			      &part_offer)) {
	DB(printf("swapfs: failed to get USDCtl for partition %d\n", 
		  st->partition)); 
	return (IDCOffer_cl *)NULL;
    }

	
    TRC(printf("swapfs: Binding to USDCtl offer\n"));
    /* Bind to the USD. Do this explicitly rather than through the object
       table; we don't want anybody else getting hold of this one. */
    st->part_binding = IDCOffer$Bind(part_offer, Pvs(gkpr), &any);
    st->usdctl       = NARROW(&any, USDCtl_clp);

    /* Initialise our free list and client list */
    LINK_INITIALISE(&st->free);
    LINK_INITIALISE(&st->clients);

    if(!(ei = Heap$Malloc(st->heap, sizeof(*ei)))) {
	DB(printf("swapfs: failed to alloc extent info.\n"));
	return (IDCOffer_cl *)NULL;
    }
    /* Everything is free initially */
    ei->extent.base = 0; 
    ei->extent.len  = st->part_size; 
    
    LINK_ADD_TO_TAIL(&st->free, ei);
    

    /* Intialise our Swap closure, and create an offer for it */
    CL_INIT(st->cl, &swap_ms, st); 
    ANY_INIT(&any, Swap_clp, &st->cl );

    shmt = NAME_FIND ("modules>ShmTransport", IDCTransport_clp);
    res = IDCTransport$Offer (shmt, &any, NULL, Pvs(heap), Pvs(gkpr),
			      Pvs(entry), &service);

    TRC(printf("swapfs: offer at %p\n", res));
    return res;
}


static uint32_t BlockSize_m(Swap_cl *self)
{
    swapfs_st *st = self->st;

    return st->blocksize; 
}

static uint32_t Free_m(Swap_cl *self)
{
    swapfs_st *st = self->st;
    extinfo_t *hd, *cur; 
    uint32_t max = 0; 

    hd = &st->free; 
    for(cur = hd->next; cur != hd; cur = cur->next) {
	if(cur->extent.len > max)
	    max = cur->extent.len;
    }

    return max; 
}


static FSTypes_QoS *CurrentQoS_m(Swap_cl *self, Swap_Handle hdl)
{
    swapfs_st *st     = self->st; 
    client_t  *client; 

    if(hdl > MAX_CLTS) {
	DB(eprintf("swapfs: got a bogus handle %x passed in.\n", hdl)); 
	RAISE_Swap$BadHandle(hdl); 
    }

    client = st->hdls[hdl]; 
    return &client->qos; 
}


static IDCOffer_cl *Open_m(Swap_cl *self, uint32_t nb, Swap_Handle *hdl)
{
    swapfs_st *st = self->st;
    extinfo_t *hd, *cur; 
    client_t  *new; 
    int i; 

    hd = &st->free; 
    for(cur = hd->next; cur != hd; cur = cur->next) {
	if(cur->extent.len >= nb)
	    break; 
    }

    if(cur == hd) { 
	DB(printf("Swap$Open: cannot allocate %lx blocks (not free).\n", nb));
	return (IDCOffer_cl *)NULL;
    }

    /* Ok: we've got >= nb; sort out the new client structure.  */
    if(!(new = Heap$Malloc(st->heap, sizeof(*new)))) {
	DB(printf("Swap$Open: cannot alloc memory for new client info.\n"));
	return (IDCOffer_cl *)NULL;
    }

    if(st->hdls[st->next_hdl] != NULL) { 
	/* oops; need linear scan (since may have fragmented) */

	for(i = 0; i < MAX_CLTS; i++) 
	    if(st->hdls[i] == NULL) 
		break;
	
	if(i == MAX_CLTS) {
	    DB(printf("Swap$Open: out of handles for new client.\n"));
	    return (IDCOffer_cl *)NULL;
	}

	*hdl           = i; 
	st->next_hdl   = (i+1) % MAX_CLTS; 
	st->hdls[*hdl] = new; 
	    
    } else {
	*hdl           = st->next_hdl++; 
	st->next_hdl  %= MAX_CLTS; 
	st->hdls[*hdl] = new; 
    }

    new->extent.base = cur->extent.base; 
    new->extent.len  = nb; 
    TRC(printf("Swap$Open: allocated %lx blocks starting at %lx\n", 
	       new->extent.len, new->extent.base));
    
    /* Tidy up our free list */
    if(cur->extent.len > nb) {
	cur->extent.len  -= nb; 
	cur->extent.base += nb; 
    } else {
	LINK_REMOVE(cur); 
	Heap$Free(st->heap, cur); 
    }

    /* Get an offer for the client from the USDCtl */
    /* Create a stream for the client. This involves calling the USD domain;
       must make sure we don't deadlock. Somehow. */
    if(USDCtl$CreateStream(st->usdctl, (USD_ClientID)((word_t)new), 
			   &new->sid, &new->offer) != USDCtl_Error_None) {
	DB(printf("Swap$Open: creation of USD stream failed.\n"));
	return (IDCOffer_cl *)NULL;
    }

    /* Setup the length so that the client can query it later */
    USDCtl$SetLength(st->usdctl, new->sid, nb * st->blocksize); 

    /* Our 'default' QoS is none guaranteed, none extra. */
    new->qos.p  = SECONDS(60);
    new->qos.s  = 0; 
    new->qos.x  = False; 
    new->qos.l  = MILLISECS(5);

    if(USDCtl$AdjustQoS(st->usdctl, new->sid, &new->qos) 
       != USDCtl_Error_None) {
	DB(printf("Swap$Open: initial setup of QoS failed.\n"));
	return False;
    }

    TRC(printf("Swap$Open: success - returning offer at %p\n", new->offer));
    return new->offer; 
}


static bool_t AdjustQoS_m(Swap_cl *self, Swap_Handle hdl, FSTypes_QoS *qos)
{
    swapfs_st *st     = self->st; 
    USD_QoS    new; 
    client_t  *client; 

    if(hdl > MAX_CLTS) {
	eprintf("swapfs: got a bogus handle %x passed in.\n", hdl); 
	RAISE_Swap$BadHandle(hdl); 
    }

    client = st->hdls[hdl]; 
    new.p  = qos->p; 
    new.s  = qos->s; 
    new.x  = qos->x; 
    new.l  = qos->l;

    if(USDCtl$AdjustQoS(st->usdctl, client->sid, &new) != USDCtl_Error_None) {
	DB(printf("Swap$AdjustQoS: adjustment failed.\n"));
	return False;
    }

    client->qos = *qos; 
    return True; 
}

static bool_t Close_m(Swap_cl *self, Swap_Handle hdl)
{
    swapfs_st *st     = self->st; 
    client_t  *client; 

    if(hdl > MAX_CLTS) {
	eprintf("swapfs: got a bogus handle %x passed in.\n", hdl); 
	RAISE_Swap$BadHandle(hdl); 
    }

    client = st->hdls[hdl]; 

    /* XXX And now close it... */

    return False; 
}



