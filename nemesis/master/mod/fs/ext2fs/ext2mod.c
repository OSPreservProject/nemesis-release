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
**	mod/fs/ext2fs
**
** FUNCTIONAL DESCRIPTION:
** 
**	EXT2FS for Nemesis
** 
** ENVIRONMENT: 
**
**	User space server
** 
** ID : $Id: ext2mod.c 1.2 Wed, 09 Jun 1999 12:18:31 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <bstring.h>
#include <salloc.h>

#include "ext2mod.h"
#include "uuid.h"

extern CSClientStubMod_op stubmod_ms;

#include <MountLocal.ih>
#include <USDCallback.ih>
#include <USDCtl.ih>
#include <USDDrive.ih>
#include <IDCCallback.ih>
#include <CSIDCTransport.ih>
#include <Threads.ih>

#include <ecs.h>
#include <time.h>

#include <ntsc.h>		/* XXX For ntsc_dbgstop */

//#define DEBUG
//#undef DEBUG_CALLBACK
//#undef DEBUG_GROUPS
//#undef DEBUG_USD

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

#ifdef DEBUG_CALLBACK
#define CBTRC(_x) _x
#else
#define CBTRC(_x)
#endif

#ifdef DEBUG_GROUPS
#define GRPTRC(_x) _x
#else
#define GRPTRC(_x)
#endif

#ifdef DEBUG_USD
#define USDTRC(_x) _x
#else
#define USDTRC(_x)
#endif

/* 
 * USDCallbacks.
 * 
 * These are designed so that they can be invoked directly
 * in the USD domain.  We can easily arrange for them to have readonly 
 * access to file system state.  The if we need to consult the Ext2
 * server then we need an IDC connection.  Luckily, the init method lets
 * us set up an appropriate binding, and the exact same interface can be
 * used! :-) 
 * 
 * So we therefore use two sets of USDCallbacks:
 * L1)  Invoked in the USD domain
 * L2)  Invoked by IDC in the EXT2 domain
 */


static USDCallback_Initialise_fn       L1_Initialise_m, L2_Initialise_m;
static USDCallback_Dispose_fn          L1_Dispose_m,    L2_Dispose_m;   
static USDCallback_Translate_fn        L1_Translate_m,  L2_Translate_m; 
static USDCallback_Notify_fn           L1_Notify_m,     L2_Notify_m;    

static USDCallback_op L1_ms = {
    L1_Initialise_m, L1_Dispose_m, L1_Translate_m, L1_Notify_m
};
static USDCallback_op L2_ms = {
    L2_Initialise_m, L2_Dispose_m, L2_Translate_m, L2_Notify_m
};

static bool_t translate(ext2fs_st            *st, 
			struct inode         *ino, 
			const FileIO_Request *req, 
			USD_Extent           *extent)
{
    FileIO_Block  blockno = 0, tmp, last;
    uint32_t      nblocks;

    last = blockno = ext2_bmap(st, ino, req->blockno);
    for (nblocks = 1; nblocks < req->nblocks; nblocks++) {
	tmp = ext2_bmap(st, ino, req->blockno+nblocks);
	if (tmp != last+1) break;
	last = tmp;
    }

    TRC(printf("(%d,%d)->%d(%d)\n", ino->i_ino, 
	   req->blockno, blockno, nblocks));

    extent->base = blockno;
    extent->len  = nblocks;
    return True;
}

/***************** L1 Callbacks : in the device **********************/

typedef struct { 
    USDCallback_cl  l1_callback;
    IDCOffer_cl    *l2_offer;
    USDCallback_cl *l2_callback;
    ext2fs_st      *ext2fs_st;
} L1_st;


static void L1_Initialise_m (USDCallback_cl  *self)
{
    L1_st   *st = self->st;
    Type_Any any;

    FTRC("entered: self=%p, st=%p offer=%p ext2fs_st=%p.\n", self, st,
	 st->l2_offer, st->ext2fs_st);
    
    FTRC("Binding to L2 USDCallback\n");
    IDCOffer$Bind(st->l2_offer, Pvs(gkpr), &any);
    st->l2_callback = NARROW(&any, USDCallback_clp);
    FTRC("Calling USDCallback$Initialise(L2)\n");
    USDCallback$Initialise(st->l2_callback);

    FTRC("leaving, succeded.\n");
}

static void L1_Dispose_m (USDCallback_cl  *self)
{
    TRC(L1_st *st = self->st);
    FTRC("entered: self=%p, st=%p.\n", self, st);

    FTRC("leaving, succeded.\n");
}

static bool_t L1_Translate_m (USDCallback_cl          *self,
			      USD_StreamID             sid          /* IN */,
			      USD_ClientID             cid          /* IN */,
			      const FileIO_Request    *req          /* IN */,
			      USD_Extent              *extent       /* OUT */,
			      uint32_t                *notification /* OUT */)
{
    L1_st        *l1_st  = self->st;
    ext2fs_st    *st     = l1_st->ext2fs_st;
    Client_st    *cur;
    Ext2_Handle   handle;
    struct inode *ino;
    bool_t        rc;

    FTRC("entered: self=%p, st=%p.\n", self, st);

    if (req->op != FileIO_Op_Read) ntsc_dbgstop();

    /* ClientID is actually a pointer to our per-client state record */
    /* This is securely provided by USD and so can be trusted. */
    cur = (Client_st *)(word_t)cid;	

    if (req->nblocks == 0) {
	/* Prefetch */
	uint32_t dummy;
	rc = USDCallback$Translate(l1_st->l2_callback,
				   sid, cid, req, extent, &dummy);
	return False;
    }

    /* req->user1 should be the EXT2 handle - needs to be checked */
    handle = req->user1;

    /* Optimise out the callback for small files */
    if (!check_file_handle(cur, handle))
	return False;

    ino = cur->handles[handle].ino;
    if (S_ISDIR(ino->i_mode)) {
	*notification = (word_t)ino;
    } else {
	*notification = 0;
    }

    if ((req->blockno + req->nblocks) <= EXT2_NDIR_BLOCKS) {
	rc = translate(st, ino, req, extent);
    } else {
	uint32_t dummy;
	rc = USDCallback$Translate(l1_st->l2_callback,
				   sid, cid, req, extent, &dummy);
    }

    /* Reduce the transfer size to the max contig. len */
    ((FileIO_Request *)req)->nblocks  = extent->len;

    /* Scale by FS block size */
    extent->base *= 2 /* XXX PRB: should use logical block size */;
    extent->len  *= 2;

    FTRC("leaving, returning true.\n");
    return rc;
}

static void L1_Notify_m (USDCallback_cl          *self,
			 USD_StreamID             sid             /* IN */,
			 USD_ClientID             cid             /* IN */,
			 const FileIO_Request    *req             /* IN */,
			 const USD_Extent        *extent          /* IN */,
			 uint32_t                 notification    /* IN */,
			 addr_t  data                             /* IN */ )
{
    L1_st        *l1_st  = self->st;
    FTRC("entered: self=%p, st=%p.\n", self, st);

    if (notification) {
	USDCallback$Notify(l1_st->l2_callback, sid, cid, req, 
			   extent, notification, data);
    }

    FTRC("leaving, succeded.\n");
}

/***************** L2 Callbacks : in the server **********************/

typedef ext2fs_st *L2_st;

static void L2_Initialise_m (USDCallback_cl  *self)
{
    TRC(L2_st *st = self->st);
    FTRC("entered: self=%p, st=%p.\n", self, st);

    FTRC("leaving, succeded.\n");
}

static void L2_Dispose_m (USDCallback_cl  *self)
{
    TRC(L2_st *st = self->st);
    FTRC("entered: self=%p, st=%p.\n", self, st);

    FTRC("leaving, succeded.\n");
}

static bool_t L2_Translate_m (USDCallback_cl          *self,
			      USD_StreamID             sid          /* IN */,
			      USD_ClientID             cid          /* IN */,
			      const FileIO_Request    *req          /* IN */,
			      USD_Extent              *extent       /* OUT */,
			      uint32_t                *notification /* OUT */ )
{
    ext2fs_st    *st = self->st;
    Client_st    *cur;
    Ext2_Handle   handle;
    struct inode *ino;

    FTRC("entered: self=%p, st=%p.\n", self, st);

    if (req->nblocks == 0) {
	/* Inode prefectch */
	ino = get_inode(st, req->user1);
	release_inode(st, ino);
	return False;
    }

    /* All checked once by L1 (XXX are there concurrency problems?) */
    cur    = (Client_st *)(word_t)cid;	
    handle = req->user1;
    ino    = cur->handles[handle].ino; 

    translate(st, ino, req, extent);

    FTRC("leaving, returning true.\n");
    return(True);
}

static void L2_Notify_m (USDCallback_cl          *self,
			 USD_StreamID             sid             /* IN */,
			 USD_ClientID             cid             /* IN */,
			 const FileIO_Request    *req             /* IN */,
			 const USD_Extent        *extent          /* IN */,
			 uint32_t                 notification    /* IN */,
			 addr_t                   data            /* IN */ )
{
    ext2fs_st          *st      = self->st;
    struct buffer_head *buf;
    uint32_t            blockno = extent->base / 2;
    uint32_t            nblocks = extent->len  / 2;
    int                 i;

    FTRC("entered: self=%p, st=%p.\n", self, st);
    TRC(printf("L2: %p %qx %qx %p %p %x %p\n",
	   self, sid, cid, req, extent, notification, data));

    MU_LOCK(&st->cache.mu);
    for (i = 0; i < nblocks; i++) {

	buf = bfind(st, blockno + i);
	if (buf) continue;	/* Already cached */

	buf = balloc(st);
	if (!buf) break;	/* No room at the inn */

	LINK_REMOVE(buf);
	bcopy(data, buf->b_data, st->fsc.block_size);
	buf->state     = buf_unlocked;
	buf->b_count   = 0;
	buf->b_blocknr = blockno + i;
	buf->b_size    = st->fsc.block_size;
 	LINK_ADD_TO_HEAD(&st->cache.bufs, buf);	/* Pull to front */
    }
    MU_RELEASE(&st->cache.mu);
    FTRC("leaving, succeded.\n");
}

static	IDCCallback_Request_fn		IDCCallback_Request_m;
static	IDCCallback_Bound_fn		IDCCallback_Bound_m;
static	IDCCallback_Closed_fn		IDCCallback_Closed_m;

static	IDCCallback_op	client_callback_ms = {
  IDCCallback_Request_m,
  IDCCallback_Bound_m,
  IDCCallback_Closed_m
};

static	MountLocal_Mount_fn		Mount_m;
static	MountLocal_SimpleMount_fn	SimpleMount_m;

static	MountLocal_op	mount_ms = {
  Mount_m,
  SimpleMount_m
};

static	const MountLocal_cl   mount_cl = { &mount_ms, NULL };

CL_EXPORT (MountLocal, ext2fs, mount_cl);


/* Lame implementation of log2() */
uint32_t log2(uint32_t n)
{
    uint32_t r=0;

    TRC(printf("log2 of %d is ",n));
    n>>=1;
    for (r=0; n; n>>=1, r++);
    return r;
}

static bool_t flush_superblock(ext2fs_st *st)
{
    if (st->fs.readonly) {
	printf("ext2fs: error: attempt to write superblock on readonly fs\n");
	return False;
    }

    /* Pack in data from working copy? Sync? */

#if 0
    if (!logical_write(st, SBLOCK_BLOCK, 1,
		       st->superblock, st->fsc.block_size)) {
	printf("ext2fs: error: write of superblock failed\n");
	return False;
    }
#endif /* 0 */

    return True;
}

static bool_t read_superblock(ext2fs_st *st)
{
    sblock_t *sblock;
    uint32_t block_size;
    char *OS_Names[] = { 
	"Linux",
	"Hurd", 
	"Masix", 
	"FreeBSD", 
	"Lites"
    };
    uint8_t volname[17], lastmount[65], uuid_s[40];

    /* Read the superblock of the file system */
    /* I _think_ we're supposed to look for the superblock in all the
       possible places it could be, depending on the logical blocksize. */
    /* XXX check this */
    if(!physical_read(st, SBLOCK_DOFF(st), SBLOCK_DBLKS(st),
		      st->cache.buf, st->cache.size)) {
	printf("ext2fs: failed to read superblock from disk.\n");
	return False;
    }

    sblock = (sblock_t *)st->cache.buf;

    if(sblock->s_magic != EXT2_SUPER_MAGIC) {
	printf("ext2fs: superblock magic is %x (should be %x)\n", 
	       sblock->s_magic, EXT2_SUPER_MAGIC);
	return False;
    }
    
    /* Allocate some memory to keep a copy of the superblock */
    st->superblock = Heap$Malloc(st->heap, sizeof(*st->superblock));
    memcpy(st->superblock, sblock, SBLOCK_SIZE);

    sblock = st->superblock;

    if (st->fs.debug) {
	printf("mount_fs: ext2fs rev %d,%d superblock (%s OS)\n", 
	       sblock->s_rev_level, sblock->s_minor_rev_level, 
	       (sblock->s_creator_os <= EXT2_OS_MAX) ? 
	       OS_Names[sblock->s_creator_os] : "unknown");
	
	uuid_unparse(sblock->s_uuid, uuid_s);
	strncpy(volname,sblock->s_volume_name,16);
	volname[16]=0;
	strncpy(lastmount,sblock->s_last_mounted,64);
	lastmount[64]=0;
	printf("ext2fs: mounting volume %s\n",uuid_s);
	if (strlen(volname))
	    printf("        volume label %s\n",volname);
	if (strlen(lastmount))
	    printf("        last mounted on %s\n",lastmount);
    }

    /* determine fs block size */
    block_size = EXT2_MIN_BLOCK_SIZE << sblock->s_log_block_size;
    st->fsc.block_size = block_size;

    /* XXX PRB: If this goes off, see the gross hack in L1_Translate */
    if (block_size != EXT2_MIN_BLOCK_SIZE) ntsc_dbgstop();

    st->fsc.inodes_per_block = block_size / EXT2_INODE_SIZE(sblock);

    /* determine the fs frag size */
    st->fsc.frag_size = EXT2_MIN_FRAG_SIZE << sblock->s_log_frag_size;
    st->fsc.frags_per_block = block_size / st->fsc.frag_size; 
    
    /* copy some superblock info to have it handy */
    st->fsc.blocks_per_group = sblock->s_blocks_per_group;
    st->fsc.frags_per_group  = sblock->s_frags_per_group;
    st->fsc.inodes_per_group = sblock->s_inodes_per_group;

    st->fsc.ngroups = (sblock->s_blocks_count - sblock->s_first_data_block + 
		   st->fsc.blocks_per_group - 1) / st->fsc.blocks_per_group;
 
    st->fsc.itb_per_group       = (st->fsc.inodes_per_group /
				   st->fsc.inodes_per_block);
    st->fsc.desc_per_block      = block_size / sizeof(gdesc_t);
    st->fsc.addr_per_block	= block_size / sizeof(block_t);
    st->fsc.addr_per_block_bits = log2(st->fsc.addr_per_block);
    st->fsc.descb_per_group	= ((st->fsc.ngroups + 
				    st->fsc.desc_per_block - 1) / 
				   st->fsc.desc_per_block);

    st->fsc.maj_rev = sblock->s_rev_level;
    st->fsc.min_rev = sblock->s_minor_rev_level; 

    if (st->fs.debug) {
	printf("+++ fs block_size	 = %d\n", st->fsc.block_size);
	printf("+++ desc_per_block	 = %d\n", st->fsc.desc_per_block);
	printf("+++ inodes_per_block     = %d\n", st->fsc.inodes_per_block);
	printf("+++ frags_per_block	 = %d\n", st->fsc.frags_per_block);
	printf("+++ addr_per_block	 = %d\n", st->fsc.addr_per_block);
	printf("+++ desc_per_block	 = %d\n", st->fsc.desc_per_block);
	printf("+++ no of groups	 = %d\n", st->fsc.ngroups);
	printf("+++ blocks per group     = %d\n", st->fsc.blocks_per_group);
	printf("+++ inodes per group     = %d\n", st->fsc.inodes_per_group);
	printf("+++ frags per group	 = %d\n", st->fsc.frags_per_group);
	printf("+++ descb per group	 = %d\n", st->fsc.descb_per_group);
    }

    if (!(sblock->s_state & EXT2_VALID_FS)) {
	TRC(printf("ext2fs: warning: volume %s (%s) mounted unclean\n",uuid_s,
	       volname));
	/* We currently reject volume if MountUnclean flag not set */
    }

    if (sblock->s_state & EXT2_ERROR_FS) {
	TRC(printf("ext2fs: warning: volume %s (%s) has errors; "
		   "mount readonly\n",
		   uuid_s, volname));
	st->fs.readonly = True;
    }

    if (!st->fs.readonly) {
	/* Set state to dirty, increase mount count, write back */
	sblock->s_state &= ~EXT2_VALID_FS;
	sblock->s_mnt_count++;

	flush_superblock(st);
    }

    return True;
}

static void init_groups(ext2fs_st *st)
{
    int                 i; 
    uint32_t            gdesc_block, offset;
    struct buffer_head *buf   = NULL;
    uint32_t            block = 0;

    st->gdesc = Heap$Malloc(st->heap, st->fsc.ngroups * sizeof(gdesc_t *));

    for (i=0; i < st->fsc.ngroups; i++) {
	st->gdesc[i] = Heap$Malloc(st->heap, sizeof(gdesc_t));

	offset=i*sizeof(gdesc_t);
	gdesc_block=(offset/st->fsc.block_size)+1+SBLOCK_BLOCK;
	offset %= st->fsc.block_size;
	/* Read the gdesc from the disk */
	if (gdesc_block!=block) {
	    if (buf) brelse(buf);
	    buf   = bread(st, gdesc_block, 0);
	    block = gdesc_block;
	}

	memcpy(st->gdesc[i], buf->b_data+offset, sizeof(gdesc_t));
#ifdef DEBUG_GROUPS
	printf("Group %d\n", i);
	printf(" --- block bitmap at %d\n", st->gdesc[i]->bg_block_bitmap);
	printf(" --- inode bitmap at %d\n", st->gdesc[i]->bg_inode_bitmap);
	printf(" --- inode table at  %d\n", st->gdesc[i]->bg_inode_table);
	printf(" --- free block count: %d\n", 
	       st->gdesc[i]->bg_free_blocks_count);
	printf(" --- free inode count: %d\n", 
	       st->gdesc[i]->bg_free_inodes_count);
	printf(" --- used dirs count : %d\n", 
	       st->gdesc[i]->bg_used_dirs_count);
	PAUSE(TENTHS(1));
#endif
    }
    brelse(buf);
    return;
}

static void init_usd(ext2fs_st *st)
{
    USD_PartitionInfo	 info;
    Type_Any		 any;
    IDCOffer_clp         partition;

    
    USDTRC(printf("ext2fs: Creating USDCallbacks\n"));
    {
	IDCTransport_clp    shmt;
	IDCOffer_clp        offer;
	IDCService_clp      service;
	L1_st              *l1_st;

	/* Get a heap writable by the USD domain */
	st->disk.heap = 
	    Gatekeeper$GetHeap(Pvs(gkpr), 
			       IDCOffer$PDID(st->disk.drive_offer), 0, 
			       SET_ELEM(Stretch_Right_Read) | 
			       SET_ELEM(Stretch_Right_Write),
			       True);

	/* The L1 callback executes there */
	l1_st = Heap$Malloc(st->disk.heap, sizeof(L1_st));
	l1_st->ext2fs_st = st;	/* our state is read-only in USD
				   domain */
	st->disk.l1_callback = &(l1_st->l1_callback);
	CLP_INIT(st->disk.l1_callback, &L1_ms, l1_st);

	/* Create an offer for the L2 Callback */
	CL_INIT(st->disk.l2_callback, &L2_ms, st);
	ANY_INIT(&any,USDCallback_clp, &st->disk.l2_callback);
	shmt  = NAME_FIND ("modules>ShmTransport", IDCTransport_clp);
	offer = IDCTransport$Offer (shmt, &any, NULL,
				    Pvs(heap), Pvs(gkpr),
				    Pvs(entry), &service); 
	
	/* Put it in the L1 callback's state record */
	l1_st->l2_offer = offer;
    }

    USDTRC(printf("ext2fs: Getting USDCtl offer\n"));
    if(!USDDrive$GetPartition(st->disk.usddrive,
			      st->disk.partition,
			      st->disk.l1_callback,
			      &partition)) {
	return;
    }
    
	
    USDTRC(printf("ext2fs: Binding to USDCtl offer\n"));
    /* Bind to the USD. Do this explicitly rather than through the object
       table; we don't want anybody else getting hold of this one. */
    st->disk.binding = IDCOffer$Bind(partition, Pvs(gkpr), &any);
    st->disk.usdctl  = NARROW(&any, USDCtl_clp);

    /* Find out some information */
    USDTRC(printf("ext2fs: fetching partition info\n"));
    if(!USDDrive$GetPartitionInfo(st->disk.usddrive, 
				  st->disk.partition, 
				  &info)) {
	return;
    }

    st->disk.phys_block_size = info.blocksize;
    st->disk.partition_size  = info.size;
    DBO(printf("ext2fs: blocksize %d, partition size %d blocks,\n"
	       "	partition type %s\n",
	       st->disk.phys_block_size, 
	       st->disk.partition_size,
	       info.osname));
    FREE(info.osname);

    return; 
}

static void shutdown_usd(ext2fs_st *st)
{
    /* Attempt to break our connection with the USD, if there is one */
    USDTRC(printf("shutdown_usd: disposing of binding\n"));
    if (st->disk.binding) {
	IDCClientBinding$Destroy(st->disk.binding);
	/* XXX since this isn't in the object table, things may go foom
	   at this point. Even if things don't go foom immediately, bad
	   things may happen when the USD server disconnects from the
	   callback service. Speaking of which... */
	   
    }
    USDTRC(printf("shutdown_usd: disposing of usddrive binding\n"));
    if (st->disk.drive_binding) {
	IDCClientBinding$Destroy(st->disk.drive_binding);
	/* XXX since this isn't in the object table, things may go foom
	   at this point. Even if things don't go foom immediately, bad
	   things may happen when the USD server disconnects from the
	   callback service. Speaking of which... */
	   
    }
    USDTRC(printf("shutdown_usd: foom\n"));

    /* So far we haven't bothered keeping a record of the IDCServerBinding
       for the callback server, on the assumption that the USD will behave
       properly and disconnect once we disconnect from it. If this turns
       out not to be the case we may want to implement the IDCCallback
       interface for the callback interface. Spooky. */

    /* The USD ought to have shut down all of the streams associated with
       the USDCtl by this point, so our clients will currently be
       suffocating. */
}
       
static bool_t create_client(ext2fs_st            *st, 
			    Client_st            *c,
			    IDCServerBinding_clp  binding)
{
    int          i;
    USD_QoS      qos;

    c->fs=st;
    CL_INIT(c->cl, &fs_ms, c);
    c->binding=binding;

    /* Default QoS? Hmm. */
    c->qos.p = MILLISECS(200);
    c->qos.s = MILLISECS(10);
    c->qos.l = MILLISECS(5);
    c->qos.x = True;

    qos.p = c->qos.p;
    qos.s = c->qos.s;
    qos.x = c->qos.x;

    for (i=0; i<MAX_CLIENT_HANDLES; i++) {
	c->handles[i].used=False;
    }

    /* Create a stream for the client. This involves calling the USD domain;
       must be careful we don't deadlock. */
    if(USDCtl$CreateStream(st->disk.usdctl,
			   (USD_ClientID)(word_t)c,
			   &(c->usd_stream),
			   &(c->usd))
       != USDCtl_Error_None)
    {
	return False;
    }
    if(USDCtl$AdjustQoS(st->disk.usdctl,
			c->usd_stream,
			&qos)
       != USDCtl_Error_None)
    {
	(void)USDCtl$DestroyStream(st->disk.usdctl,
				   c->usd_stream);
	return False;
    }

    return True;
}

static IDCOffer_clp SimpleMount_m (MountLocal_cl *self, 
				   IDCOffer_clp   drive,
				   uint32_t       partition)
{
    return Mount_m(self, 
		   drive, 
		   partition, 
		   SET_ELEM(MountLocal_Option_ReadOnly),
		   NULL);
}

static IDCOffer_clp Mount_m(MountLocal_cl     *self, 
			    IDCOffer_clp       drive,
			    uint32_t           partition,
			    MountLocal_Options options, 
			    Context_clp        settings)
{
    IDCOffer_clp  res;
    ext2fs_st	 *st;
    Type_Any      any;
    Heap_clp      heap;
    struct inode *root            = NULL;
    uint32_t      blockcache_size = 1024*128; /* Size of blockcache in bytes */
    CSClientStubMod_cl *stubmod_clp; 


    TRC(printf("ext2fs: mount %d from %p\n", partition, drive));
    /* It's probably a good idea to have a separate heap for the filesystem.
       For now let's just use Pvs(heap), but eventually create a stretch
       of our own. */

    heap = Pvs(heap);

    if(!(st = Heap$Malloc(heap, sizeof(*st)))) {
	fprintf(stderr, "ext2fs: cannot allocate state.\n");
	RAISE_MountLocal$Failure();
    }

    /* Where is this declared? */
    bzero(st, sizeof(*st));

    /* Fill in the fields that we can initialise without accessing the
       filesystem */
    st->heap = heap;

    st->entrymod     = NAME_FIND("modules>EntryMod", EntryMod_clp);
    st->shmtransport = NAME_FIND("modules>ShmTransport", IDCTransport_clp);
    st->csidc        = NAME_FIND("modules>CSIDCTransport", CSIDCTransport_clp);


    st->client.entry = Pvs(entry);
    /* It's not clearn how many entries we are going to require yet
       We probably want separate ones for the USDCallback and the
       FSClient offers. We need to arrange that the entry threads die
       properly when the FS is unmounted. */


    /* Interpret mount flags */
    st->fs.readonly = SET_IN(options,MountLocal_Option_ReadOnly);
    st->fs.debug    = SET_IN(options,MountLocal_Option_Debug);


    /* Place the drive in the state. */
    st->disk.partition     = partition;
    st->disk.drive_offer   = drive;
    st->disk.drive_binding = IDCOffer$Bind(drive, Pvs(gkpr), &any);
    st->disk.usddrive      = NARROW(&any, USDDrive_clp);

 
    TRC(printf("ext2fs: state at [%p, %p]\n",st, (void *)st + sizeof(*st)));
    DBO(printf("ext2fs: debugging output is switched on\n"));

    /* Connect to the disk */
    init_usd(st);

    /* We need a stretch shared between us and the USD to allow us to read
       and write metadata. We'll use this stretch as a cache of blocks read
       from the disk. Because we won't know the blocksize until we have
       managed to read the superblock, we'd better make this buffer a
       multiple of 8k long (8k is currently the maximum blocksize). */

    st->cache.str = Gatekeeper$GetStretch(Pvs(gkpr), IDCOffer$PDID(drive), 
					  blockcache_size, 
					  SET_ELEM(Stretch_Right_Read) |
					  SET_ELEM(Stretch_Right_Write), 
					  PAGE_WIDTH, PAGE_WIDTH);
    st->cache.buf = STR_RANGE(st->cache.str, &st->cache.size);

    TRC(printf("ext2fs: buf is %d bytes at %p\n", st->cache.size,
	       st->cache.buf));
    if (st->cache.size < blockcache_size) {
	printf("ext2fs: warning: couldn't allocate a large blockcache\n");
    }

    /* Now we can get at the disk. Read the superblock, and calculate
       constants from it. */
    if (!read_superblock(st)) {
	printf("ext2fs: couldn't read superblock\n");
	shutdown_usd(st);
	RAISE_MountLocal$BadFS(MountLocal_Problem_BadSuperblock);
    }

    /* XXX should sanity check filesystem size with partition size */
    TRC(printf("ext2fs: filesystem size %d blocks (%d phys)\n"
	   "	    partition size %d blocks (%d phys)\n",
	   st->superblock->s_blocks_count,
	   PHYS_BLKS(st, st->superblock->s_blocks_count),
	   LOGICAL_BLKS(st, st->disk.partition_size),
	   st->disk.partition_size));
    if (st->disk.partition_size < 
	PHYS_BLKS(st, st->superblock->s_blocks_count)) {
	printf("WARNING - filesystem is larger than partition **********\n");
	/* XXX should probably give up now */
    }

    /* Now that we know the logical block size we can initialise the block
       cache */
    init_block_cache(st);

    /* From this point on, all access to the filesystem should be done
       through the block cache. DON'T call logical_read, call bread
       instead. Remember to free blocks once you're finished with them. */

    init_groups(st);

    if(!init_inodes(st)) {
	fprintf(stderr, "ext2fs: failed to initialise inode cache.\n");
	shutdown_usd(st);
	RAISE_MountLocal$Failure();
    }

    /* Checking this probably isn't a bad idea, but let's wait until later */

    /* Ok, now we are capable of reading the root inode (I hope!) */
    TRC(printf("ext2fs: checking root inode.\n"));
    root = get_inode(st, EXT2_ROOT_INO);
    if(!root) {
	fprintf(stderr, "ext2fs: failed to read root inode.\n");
	shutdown_usd(st);
	RAISE_MountLocal$BadFS(MountLocal_Problem_BadRoot);
    }
    
    if(!S_ISDIR(root->i_mode)) {
	fprintf(stderr, "ext2fs: urk!\n"
		"	 inode %d does not refer to a directory\n", 
		EXT2_ROOT_INO);
	shutdown_usd(st);
	RAISE_MountLocal$BadFS(MountLocal_Problem_BadRoot);
    }

    release_inode(st, root);

    /* *thinks* should probably do something about deallocating state
       if we fail, too. */

    /* Initialise the list of clients */
    LINK_INIT(&st->client.clients);
    /* We create a server for the local domain; it lives in the head
       of the list of clients. The call to CSIDCTransport$Offer() will
       set up client-side stubs for this domain and put them in the
       object table. */
    create_client(st, &st->client.clients, NULL);

    /* Now we do all the export stuff */
    CL_INIT(st->client.callback, &client_callback_ms, st);
    ANY_INIT(&any, Ext2_clp, &st->client.clients.cl);
    stubmod_clp = Heap$Malloc(st->heap, sizeof(*stubmod_clp)); 
    CLP_INIT(stubmod_clp, &stubmod_ms, NULL);
    res = CSIDCTransport$Offer (
	st->csidc, &any, FSClient_clp__code, stubmod_clp,
	&st->client.callback, /* XXX produces a warning */
	st->heap, Pvs(gkpr), st->client.entry, &st->client.service);

    TRC(printf("ext2fs: offer at %p\n",res));

    return res;
}

/*----------------------------------------- IDCCallback Entry Points ----*/

/* These correspond to FSClient connections */

static bool_t IDCCallback_Request_m (
	IDCCallback_cl	        *self,
	IDCOffer_clp	         offer	        /* IN */,
	Domain_ID	         dom	        /* IN */,
	ProtectionDomain_ID 	 pdid	        /* IN */,
	const Binder_Cookies	*clt_cks	/* IN */,
	Binder_Cookies	        *svr_cks	/* OUT */ )
{
    /* The server that likes to say 'yes' */
    TRC(printf("ext2fs: accepting connection from dom %qx\n",dom));

    return True;
}

static bool_t IDCCallback_Bound_m (
	IDCCallback_cl	       *self,
	IDCOffer_clp	        offer	/* IN */,
	IDCServerBinding_clp	binding /* IN */,
	Domain_ID	        dom	/* IN */,
	ProtectionDomain_ID	pdid	/* IN */,
	const Binder_Cookies   *clt_cks	/* IN */,
	Type_Any	       *server  /* INOUT */ )
{
    ext2fs_st *st = self->st;
    Client_st *c;

    /* A client has finished binding. We need to invent a server. */
    TRC(printf("ext2fs: accepted connection from dom %qx\n",dom));

    c=Heap$Malloc(st->heap, sizeof(*c));
    if (!c) {
	printf("ext2fs: out of memory while accepting connection from %qx\n",
	       dom);
	return False;
    }

    if (!create_client(st,c,binding)) {
	printf("ext2fs: aargh, client create failed. Rejecting connection.\n");
	FREE(c);
	return False;
    }

    /* XXX: concurrency? */
    LINK_ADD_TO_HEAD(&st->client.clients, c);

    ANY_INIT(server, Ext2_clp, &c->cl);

    return True;
}

static void IDCCallback_Closed_m (
	IDCCallback_cl	       *self,
	IDCOffer_clp	        offer	/* IN */,
	IDCServerBinding_clp	binding /* IN */,
	const Type_Any	       *server  /* IN */ )
{
    ext2fs_st *st=self->st; 
    Client_st *c=NULL, *head;
    int i;


    /* A client has gone away. We have to close all of their files,
       remove their stream from the USD, and delete all of the state
       associated with them. */

    /* set head to the start of the client list */
    head= (&st->client.clients)->next;

    /* search through the list to find the current client */
    for (c=head; c!=(&st->client.clients); c=c->next)
	{
	    if (c->binding==binding)
		{	
		    TRC(printf("ext2fs: disconnecting client %p\n", c));

		    TRC(printf("ext2fs: closing client's files\n"));

		    /* attempt to close the client's files (Ext2$Free
                       checks whether a handle is valid or not) */
		    for (i=0; i<MAX_CLIENT_HANDLES; i++)
			{
			    Ext2$Free(&(c->cl), (Ext2_Handle)i, True);
			}
		    
		    /* destroy client's stream to the USD */
		    TRC(printf("ext2fs: deleting client's stream from USD\n"));
		    if ( USDCtl$DestroyStream(st->disk.usdctl, c->usd_stream) != USDCtl_Error_None)
			printf("ext2fs: error deleting stream from USD\n");

		    /* remove client from client list */
		    LINK_REMOVE(c);
		    /* free client's state */
		    FREE(c);
		    
			   
		    TRC(printf("ext2fs: finished deleting stream\n"));
		    
		    return;	
		}
	    
		   
		
	}
    
	
    printf("ext2fs: a client has disconnected. Couldn't find it's binding though. Oh dear.\n");
    
    printf("ext2fs: a client has disconnected. Oh dear.\n");
}

/*-------------------------------------------- Utility functions ----------*/

void ext2_error(ext2fs_st  *st, 
		const char *function,
		const char *fmt, 
		...)
{
    va_list args;
    char    error_buf[160]; /* XXX On stack */

    va_start(args, fmt);

    vsprintf(error_buf, fmt, args);

    va_end(args);

    /* Do something about it */
    printf("Ext2fs error: %s %s\n",function,error_buf);

    /* XXX do something else about it */
}

void ext2_warning(ext2fs_st  *st, 
		  const char *function,
		  const char *fmt, ...)
{
    va_list args;
    char    error_buf[160];

    va_start(args, fmt);
    vsprintf(error_buf, fmt, args);
    va_end(args);

    printf("Ext2fs warning: %s %s\n",function,error_buf);
}

void ext2_panic(ext2fs_st *st, 
		const char *function,
		const char *fmt, 
		...)
{
    va_list args;
    char    error_buf[160];

    va_start(args, fmt);
    vsprintf(error_buf, fmt, args);
    va_end(args);

    printf("Ext2fs panic: %s %s\n",function,error_buf);
    /* Set the 'filesystem has errors' flag, write the superblock, and
       exit */
}
