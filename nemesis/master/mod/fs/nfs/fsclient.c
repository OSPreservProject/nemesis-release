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
**	mod/fs/rnfs
** 
** FUNCTIONAL DESCRIPTION:
** 
**	NFS client side stubs
** 
** ENVIRONMENT: 
**
**	User land
** 
** ID : $Id: fsclient.c 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <ecs.h>
#include <contexts.h>
#include <exceptions.h>
#include <fsoffer.h>

#include <stdio.h>

#include <FSClient.ih>
#include <FSDir.ih>

#include <Event.ih>
#include <FIFOMod.ih>
#include <IDC.ih>
#include <FIFO.ih>
#include <NFS.ih>

#ifdef DEBUG_FSCLIENT
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

//#define CACHE_TRC
//#define CACHE_STATS

#ifdef CACHE_TRC
#define CTRC(_x) _x
#else
#define CTRC(_x)
#endif

#ifdef CACHE_STATS
#define CSTAT(_x) _x
#else
#define CSTAT(_x)
#endif


#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);

/* cache controls */
#define CACHE_NFIXED	1	/* number of lines independant of open files */
#define CACHE_NFILEIO	1	/* number of lines per open file */
#define CACHE_LINESZ	8192	/* max bytes per cache line */

#define FILEIO_BUFSZ	1024	/* size of a FileIO block */

static  FSClient_CurrentQoS_fn          FSClient_CurrentQoS_m;
static  FSClient_AdjustQoS_fn           FSClient_AdjustQoS_m;
static  FSClient_GetDir_fn              FSClient_GetDir_m;
static  FSClient_Stat_fn                FSClient_Stat_m;
static  FSClient_Unmount_fn             FSClient_Unmount_m;

static  FSClient_op     fsclient_ms = {
    FSClient_CurrentQoS_m,
    FSClient_AdjustQoS_m,
    FSClient_GetDir_m,
    FSClient_Stat_m,
    FSClient_Unmount_m
};

static  FSDir_Lookup_fn                 FSDir_Lookup_m;
static  FSDir_Create_fn                 FSDir_Create_m;
static  FSDir_MkDir_fn                  FSDir_MkDir_m;
static  FSDir_Rename_fn                 FSDir_Rename_m;
static  FSDir_Symlink_fn                FSDir_Symlink_m;
static  FSDir_Duplicate_fn              FSDir_Duplicate_m;
static  FSDir_Delete_fn			FSDir_Delete_m;
static  FSDir_Close_fn                  FSDir_Close_m;
static  FSDir_Delegate_fn               FSDir_Delegate_m;
static  FSDir_Release_fn                FSDir_Release_m;
static  FSDir_InodeType_fn              FSDir_InodeType_m;
static  FSDir_CWD_fn                    FSDir_CWD_m;
static  FSDir_Size_fn                   FSDir_Size_m;
static  FSDir_NLinks_fn                 FSDir_NLinks_m;
static  FSDir_NForks_fn                 FSDir_NForks_m;
static  FSDir_ATime_fn                  FSDir_ATime_m;
static  FSDir_MTime_fn                  FSDir_MTime_m;
static  FSDir_CTime_fn                  FSDir_CTime_m;
static  FSDir_Open_fn                   FSDir_Open_m;
static  FSDir_Link_fn                   FSDir_Link_m;
static  FSDir_ReadDir_fn                FSDir_ReadDir_m;
static  FSDir_ReadLink_fn               FSDir_ReadLink_m;

static  FSDir_op        dir_ms = {
    FSDir_Lookup_m,
    FSDir_Create_m,
    FSDir_MkDir_m,
    FSDir_Rename_m,
    FSDir_Symlink_m,
    FSDir_Duplicate_m,
    FSDir_Delete_m,
    FSDir_Close_m,
    FSDir_Delegate_m,
    FSDir_Release_m,
    FSDir_InodeType_m,
    FSDir_CWD_m,
    FSDir_Size_m,
    FSDir_NLinks_m,
    FSDir_NForks_m,
    FSDir_ATime_m,
    FSDir_MTime_m,
    FSDir_CTime_m,
    FSDir_Open_m,
    FSDir_Link_m,
    FSDir_ReadDir_m,
    FSDir_ReadLink_m
};

static	IO_PutPkt_fn		FileIO_PutPkt_m;
static	IO_AwaitFreeSlots_fn    FileIO_AwaitFreeSlots_m;
static	IO_GetPkt_fn		FileIO_GetPkt_m;
static	IO_QueryDepth_fn	FileIO_QueryDepth_m;
static	IO_QueryKind_fn		FileIO_QueryKind_m;
static	IO_QueryMode_fn		FileIO_QueryMode_m;
static	IO_QueryShm_fn		FileIO_QueryShm_m;
static  IO_QueryEndpoints_fn    FileIO_QueryEndpoints_m;
static	IO_Dispose_fn		FileIO_Dispose_m;
static	FileIO_BlockSize_fn	FileIO_BlockSize_m;
static  FileIO_GetLength_fn     FileIO_GetLength_m;
static	FileIO_SetLength_fn	FileIO_SetLength_m;

static	FileIO_op	fileio_ms = {
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



/*----------------------------------------------------------- State ----*/

struct _FileIO_st;

/* A cache line is the unit of transactions to the NFS layer below us.
 * Cache lines contain multiple contiguous FileIO blocks. */
typedef struct _cacheline_t {
    struct _FileIO_st	*file;		/* which file these blocks are from */
    FileIO_Block	blockbase;	/* file block number at start of line*/
    uint32_t		nblocks;	/* how many file blocks are valid */
    FileIO_Block	nfsblock;	/* NFS block number for whole line */
    bool_t		dirty;		/* True if any blocks need writeback */
    uint32_t		used;		/* increases each time line is hit */
    uint8_t		*data;		/* actual cached data */
} cacheline_t;

/* State for the whole client */
typedef struct {
    /* What we show to the world */
    Type_Any		srgt;
    FSClient_cl		cl;

    /* A Heap for our own use */
    Heap_clp		heap;

    /* Constructor for FIFOs */
    FIFOMod_clp		fmod;

    /* Internal stuff; handle on filesystem */
    NFS_clp		fs;
    IDCClientBinding_clp serverbinding;

    /* cache stuff */
    cacheline_t		cache[CACHE_NFIXED];
} FSClient_st;

/* Per-directory state */
typedef struct {
    FSDir_cl cl;
    FSClient_st *st;

    NFS_Handle cwd;
    NFS_Handle csel; /* Current selection of inode, or -1 */
} FSDir_st;

typedef struct _FileIO_st {
    FileIO_cl    cl;
    FSClient_st *st;
    NFS_Handle   handle;   /* Handle of the open file */
    /* Mode flags? */

    IDC_Buffer   buffer;   /* Descriptor for buffer */
    Event_Pair   txecs;    /* Event counts for FIFO */
    Event_Pair   rxecs;
    FIFO_clp     txfifo;   /* FIFO for packets */
    FIFO_clp     rxfifo;

    /* each FileIO contributes a cache line to the general pool */
    cacheline_t	 cache[CACHE_NFILEIO];
    int		 cache_pos;	/* last victim considered */
#ifdef CACHE_STATS
    cacheline_t	*lasthit;	/* line the last hit came from */
    uint32_t	 lookups;	/* total lookups */
    uint32_t	 hits;		/* total successful lookups */
    uint32_t	 otherhits;	/* lookups that matched in line != lasthit */
#endif /* CACHE_STATS */
} FileIO_st;


/*---------------------------------------- internal helper routines ----*/

static int cache_flushlines(cacheline_t *c, int nlines);
static int cache_flushline(cacheline_t *c);
static int cache_putblock(FileIO_st *st, FileIO_Block b, void *buf);
static int cache_getblock(FileIO_st *st, FileIO_Block b, void *buf);
static cacheline_t *cache_findline(FileIO_st *st, FileIO_Block b);


/*--------------------------------------------- FileIO Entry Points ----*/

static FileIO_clp setup_fileio(FSClient_st *st, NFS_Handle file)
{
    FileIO_st *fio;
    int        i;

    fio         = Heap$Malloc(st->heap, sizeof(*fio));
    CL_INIT(fio->cl, &fileio_ms, fio);
    fio->st     = st;
    fio->handle = file;

    /* Construct FIFO */
    fio->buffer.a = Heap$Malloc(st->heap,1024);
    fio->buffer.w = 1024;  /* XXX should not be hard-coded */
    fio->txecs.tx = EC_NEW();
    fio->txecs.rx = EC_NEW();
    fio->rxecs.tx = EC_NEW();
    fio->rxecs.rx = EC_NEW();
    fio->txfifo   = FIFOMod$New(st->fmod, st->heap, sizeof(IO_Rec),
				&fio->buffer, &fio->txecs);
    fio->rxfifo   = FIFOMod$New(st->fmod, st->heap, sizeof(IO_Rec),
				&fio->buffer, &fio->rxecs);

    /* XXX AND: where is this memory supposed to come from? */
    for(i=0; i<CACHE_NFILEIO; i++)
    {
	fio->cache[i].data = Heap$Malloc(st->heap, CACHE_LINESZ);
	fio->cache[i].file = NULL;	/* line not in use */
    }
    fio->cache_pos = 0;

#ifdef CACHE_STATS
    fio->lasthit = NULL;
    fio->lookups = 0;
    fio->hits    = 0;
    fio->otherhits = 0;
#endif /* CACHE_STATS */

    return &fio->cl;
}

static bool_t FileIO_PutPkt_m (
    IO_cl	    *self,
    uint32_t	    nrecs   /* IN */,
    IO_Rec_ptr	    recs    /* IN */,
    word_t	    value   /* IN */,
    Time_T	    until   /* IN */ )
{
    FileIO_st	   *st = self->st;
    uint8_t	   *buf;
    FileIO_Request *req;
    uint32_t        i;

    /* XXX semantics are wrong for new IO interfaces, but we should
       be alright for now. */

    /* Check the operation, and do the appropriate thing. */
    req=recs[0].base;
    switch(req->op) {
    case FileIO_Op_Read:
	req->rc = FSTypes_RC_OK;
	req->detail = FileIO_RCDetail_OK;

	buf = recs[1].base;

	for(i=0; i < req->nblocks; i++)
	{
	    if (cache_getblock(st, req->blockno+i, buf))
	    {
		req->rc = FSTypes_RC_FAILURE;
		req->detail = FileIO_RCDetail_MediumError;
		break;
	    }
	    buf += FILEIO_BUFSZ;
	}
	break;

    case FileIO_Op_Write:
	req->rc = FSTypes_RC_OK;
	req->detail = FileIO_RCDetail_OK;

	buf = recs[1].base;

	for(i=0; i < req->nblocks; i++)
	{
	    if (cache_putblock(st, req->blockno+i, buf))
	    {
		req->rc = FSTypes_RC_FAILURE;
		req->detail = FileIO_RCDetail_MediumError;
		break;
	    }
	    buf += FILEIO_BUFSZ;
	}
	break;

    case FileIO_Op_Sync:
	/* AND: flushes entire fixed cache, should really only flush lines
	 * from this file */
	i  = cache_flushlines(st->st->cache, CACHE_NFIXED);
	i += cache_flushlines(st->cache, CACHE_NFILEIO);
	req->rc = i? FSTypes_RC_FAILURE : FSTypes_RC_OK;
	req->detail = i? FileIO_RCDetail_MediumError : FileIO_RCDetail_OK;
	break;

    default:
	printf("FileIO$PutPkt(NFS): unknown request opcode %d\n", req->op);
	break;
    }

    /* Put the recs in the queue to be returned to the client */
    FIFO$Put(st->txfifo, nrecs, recs, value, True);

    /* Update rx event count */
    EC_ADVANCE(st->rxecs.rx, EC_READ(st->txecs.tx)-EC_READ(st->rxecs.rx));

    return True;
}



static bool_t FileIO_AwaitFreeSlots_m(IO_cl    *self, 
				      uint32_t  nslots, 
				      Time_T    until)
{
    /* XXX This is probably wrong; fix it if you ever think you'll use it */
    return True;
}

static bool_t FileIO_GetPkt_m (
    IO_cl	    *self,
    uint32_t	     max_recs	    /* IN */,
    IO_Rec_ptr	     recs           /* IN */,
    Time_T           until,
    uint32_t	    *nrecs,
    word_t	    *value          /* OUT */ )
{
    FileIO_st	  *st = self->st;
    uint32_t       res;
    uint32_t       fifo_value;

    /* XXX semantics are wrong for new IO interfaces */

    /* Pull an entry from the FIFO, blocking if necessary */
    res    = FIFO$Get(st->rxfifo, max_recs, recs, True, &fifo_value);
    *value = (word_t)fifo_value;

    /* Update rx event count */
    EC_ADVANCE(st->txecs.rx, EC_READ(st->rxecs.tx)-EC_READ(st->txecs.rx));

    *nrecs=res;

    return True;
}

static uint32_t FileIO_QueryDepth_m (
    IO_cl        *self )
{
    FileIO_st     *st = self->st;

    return FIFO$SlotsFree(st->txfifo);
}

static IO_Kind FileIO_QueryKind_m (
    IO_cl        *self )
{
    return IO_Kind_Master;
}

static IO_Mode FileIO_QueryMode_m (
    IO_cl        *self )
{
    return IO_Mode_Dx;
}

static IOData_Shm *FileIO_QueryShm_m (
    IO_cl        *self )
{
    RAISE_IO$Failure();

    return NULL;
}

static void FileIO_QueryEndpoints_m (IO_clp self, Channel_Pair *pair) {

    /* We don't have any useful endpoints associated with us */
    pair->rx = NULL_EP;
    pair->tx = NULL_EP;
}

static void FileIO_Dispose_m (
    IO_cl	    *self )
{
    FileIO_st	  *st = self->st;
    int		   i;

    /* Ditch the handle, dismantle the FIFO */
    FIFO$Dispose(st->txfifo);
    FIFO$Dispose(st->rxfifo); /* Also frees event counts */

    /* flush & free the cache(s) */

    /* local cache */
    cache_flushlines(st->cache, CACHE_NFILEIO);
    for(i=0; i < CACHE_NFILEIO; i++)
	FREE(st->cache[i].data);

    /* flush and free entries in the fixed cache that are for this
     * file */
    for(i=0; i < CACHE_NFIXED; i++)
	if (st->st->cache[i].file == st)
	{
	    if (cache_flushline(&st->st->cache[i]))
		printf("FileIO_Dispose: warning: "
		       "cache line still dirty after flush, losing data\n");
	    st->st->cache[i].file = NULL;	/* able to reuse this line */
	} 

    /* ditch the layer below us */
    NFS$Free(st->st->fs, st->handle);

    /* Free our own state and return */
    FREE(st);
    return;
}

static uint32_t FileIO_BlockSize_m (
    FileIO_cl	    *self )
{
    return FILEIO_BUFSZ;
}

static FileIO_Size FileIO_GetLength_m (
    FileIO_cl *self )
{
    FileIO_st *st = self->st;

    return NFS$Size(st->st->fs, st->handle);
}

static FSTypes_RC FileIO_SetLength_m (
    FileIO_cl	    *self,
    FileIO_Size      length  /* IN */ )
{
    FileIO_st	  *st = self->st;

    return NFS$SetLength(st->st->fs, st->handle, length);
}


/*------------------------------------------- FSClient Entry Points ----*/

static FSTypes_QoS *FSClient_CurrentQoS_m (
    FSClient_cl	    *self )
{
    /* We don't do here QoS, yet. */

    return NULL;
}

static bool_t FSClient_AdjustQoS_m (
    FSClient_cl	    *self,
    FSTypes_QoS	    *qos    /* INOUT */ )
{
    return True;
}

static FSTypes_RC FSClient_GetDir_m (
    FSClient_cl     *self,
    string_t         certificate     /* IN */,
    bool_t           root            /* IN */
    /* RETURNS */,
    Type_Any       **dir )
{
    FSClient_st   *st = self->st;
    FSDir_st      *d;
    Type_Any      *dir_r;

    /* Construct state */
    d = Heap$Malloc(st->heap, sizeof(*d));
    CL_INIT(d->cl, &dir_ms, d);
    d->st = st;

    if (NFS$Root(st->fs, &d->cwd)!=FSTypes_RC_OK) {
	Heap$Free(st->heap, d);
	TRC(printf("NFS: FSClient: NFS$Root() failed\n"));
	return FSTypes_RC_FAILURE; /* Probably out of handles */
    }

    d->csel = -1; /* It'd be really bad for this to be left at 0... */

    dir_r = Heap$Malloc(st->heap, sizeof(*dir_r));
    ANY_INIT(dir_r, FSDir_clp, &d->cl);
    *dir  = dir_r;

    /* Change directory if requested */
    if (!root) {
	FSDir$Lookup(&d->cl, certificate, False);
	FSDir$CWD(&d->cl);

	/* If it doesn't succeed then we are left in the root. This is ok. */
    }

    return FSTypes_RC_OK;
}

static FSTypes_Info *FSClient_Stat_m (
    FSClient_cl *self )
{
    FSClient_st *st = self->st;

    return NFS$Stat(st->fs);
}

static FSTypes_RC FSClient_Unmount_m (
    FSClient_cl     *self,
    bool_t  force   /* IN */ )
{
#if 0
    FSClient_st   *st = self->st;
#endif
    /* Deregister the client */
    /* XXX AND: need to free up buffers used for CACHE_NFIXED purposes */
#if 0
    return NFS$Unmount(st->fs, force);
#endif
    return FSTypes_RC_OK;
}

/*---------------------------------------------- FSDir Entry Points ----*/

static FSTypes_RC FSDir_Lookup_m (
    FSDir_cl        *self,
    FSTypes_Name     name    /* IN */,
    bool_t           follow  /* IN */ )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;
    NFS_Handle   h;
    FSTypes_RC   rc;

    /* Free the currently selected handle, if there is one */
    if (dir->csel != -1) 
	NFS$Free(st->fs, dir->csel);

    dir->csel = -1;

    /* Do the lookup and store the result */
    if((rc = NFS$Lookup(st->fs, dir->cwd, name, follow, &h)) == FSTypes_RC_OK)
	dir->csel = h;

    return rc;
}

static FSTypes_RC FSDir_Create_m (
    FSDir_cl        *self,
    FSTypes_Name     name    /* IN */ )
{
    FSDir_st      *dir = self->st;
    FSClient_st   *st  = dir->st;
    NFS_Handle     handle;
    FSTypes_RC     rc;
    
    /* Try to do the Create operation first */
    rc = NFS$Create(st->fs, dir->cwd, name, 0, &handle);

    if (rc!=FSTypes_RC_OK) return rc;

    /* If it worked then point the FSDir at it */
    if (dir->csel != -1) NFS$Free(st->fs, dir->csel);
    dir->csel = handle;

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_MkDir_m (
    FSDir_cl        *self,
    FSTypes_Name     name    /* IN */ )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    return NFS$MkDir(st->fs, dir->cwd, name);
}

static FSTypes_RC FSDir_Rename_m (
    FSDir_cl        *self,
    FSTypes_Name     from    /* IN */,
    FSTypes_Name     to      /* IN */ )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    /* XXX eventually must deal with paths rather than names */

    return NFS$Rename(st->fs, dir->cwd, from, dir->cwd, to);
}

static FSTypes_RC FSDir_Symlink_m (
    FSDir_cl       *self,
    FSTypes_Name    name    /* IN */,
    FSTypes_Name    path    /* IN */ )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    return NFS$Symlink(st->fs, dir->cwd, name, path);
}

static FSTypes_RC FSDir_Duplicate_m (
    FSDir_cl        *self,
    /* RETURNS */
    Type_Any       **fsdir )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;
    FSDir_st    *nd;
    NFS_Handle   h;
    Type_Any    *fsdir_r;

    nd     = Heap$Malloc(st->heap, sizeof(*nd));
    CL_INIT(nd->cl, &dir_ms, nd);
    nd->st = st;

    if (NFS$Lookup(st->fs, dir->cwd, "", False, &h) != FSTypes_RC_OK) {
	/* Out of handles */
	Heap$Free(st->heap, nd);
	return FSTypes_RC_FAILURE;
    }

    nd->cwd  = h;
    nd->csel = -1; /* Don't duplicate handle */

    fsdir_r  = Heap$Malloc(st->heap, sizeof(*fsdir_r));
    ANY_INIT(fsdir_r, FSDir_clp, &nd->cl);
    *fsdir   = fsdir_r;

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_Delete_m (
    FSDir_cl     *self,
    FSTypes_Name  name )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;
    FSTypes_RC   rc;

    /* We need to work out whether it's a directory; try both calls XXX */
    rc = NFS$Delete(st->fs, dir->cwd, name);

    if (rc != FSTypes_RC_OK) {
	rc = NFS$RmDir(st->fs, dir->cwd, name);
    }
    
    return rc;
}

static void FSDir_Close_m (
    FSDir_cl        *self )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    /* Free the CWD */
    NFS$Free(st->fs, dir->cwd);
    /* Free the currently selected inode, if appropriate */
    if (dir->csel!=-1)
	NFS$Free(st->fs, dir->csel);

    /* Free state */
    Heap$Free(st->heap, dir);

    return;
}

static string_t FSDir_Delegate_m (
    FSDir_cl        *self )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    return NFS$GetPath(st->fs, dir->cwd);
}

static void FSDir_Release_m (
    FSDir_cl        *self )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    /* Free the currently selected inode, if appropriate */
    if (dir->csel!=-1)
	NFS$Free(st->fs, dir->csel);

    return;
}

static FSTypes_RC FSDir_InodeType_m (
    FSDir_cl                *self
    /* RETURNS */,
    FSTypes_InodeType       *type )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    *type = NFS$InodeType(st->fs, dir->csel);

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_CWD_m (
    FSDir_cl        *self )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    TRC(printf("NFS: FSDir: in CWD\n"));
    if (dir->csel == -1) {
	TRC(printf("NFS: FSDir: CWD: nothing selected\n"));
	return FSTypes_RC_FAILURE;
    }

    TRC(printf("NFS: FSDir: CWD: checking inode type\n"));
    /* Check that the currently selected inode is a directory */
    if (NFS$InodeType(st->fs, dir->csel) != FSTypes_InodeType_Dir) {
	TRC(printf("NFS: FSDir: attempt to change to non-directory\n"));
	return FSTypes_RC_FAILURE;
    }

    TRC(printf("NFS: FSDir: CWD: Happy. About to free %d\n",dir->cwd));
    /* Ok, we're happy. Free the cwd, swizzle the handles, and return */
    NFS$Free(st->fs, dir->cwd);
    dir->cwd  = dir->csel;
    dir->csel = -1;

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_Size_m (
    FSDir_cl        *self
    /* RETURNS */,
    FileIO_Size     *size )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    *size = NFS$Size(st->fs, dir->csel);

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_NLinks_m (
    FSDir_cl        *self
    /* RETURNS */,
    FSTypes_NLinks  *n )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    *n = NFS$NLinks(st->fs, dir->csel);

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_NForks_m (
    FSDir_cl        *self
    /* RETURNS */,
    uint32_t        *n )
{
    FSDir_st      *dir = self->st;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    *n = 1; /* Constant for NFS */
    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_ATime_m (
    FSDir_cl        *self
    /* RETURNS */,
    FSTypes_Time    *atime )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    *atime = NFS$ATime(st->fs, dir->csel);

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_MTime_m (
    FSDir_cl        *self
    /* RETURNS */,
    FSTypes_Time    *mtime )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    *mtime = NFS$MTime(st->fs, dir->csel);

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_CTime_m (
    FSDir_cl        *self
    /* RETURNS */,
    FSTypes_Time    *ctime )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    *ctime = NFS$CTime(st->fs, dir->csel);

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_Open_m (
    FSDir_cl        *self,
    uint32_t        fork,
    FSTypes_Mode    mode    /* IN */,
    FSTypes_Options options /* IN */
    /* RETURNS */,
    FileIO_clp      *fileio )
{
    FSDir_st      *dir = self->st;
    FSClient_st   *st  = dir->st;
    NFS_Handle     handle;
    FSTypes_RC     rc;
    
    if (dir->csel == -1) {
	TRC(printf("NFS: FSDir: Open: nothing selected\n"));
	return FSTypes_RC_FAILURE;
    }
    
    if (fork!=0) return FSTypes_RC_FAILURE;
	
    /* Check that the currently selected inode is a file */
    if (NFS$InodeType(st->fs, dir->csel)!=FSTypes_InodeType_File) {
	TRC(printf("NFS: FSDir: Open: not a file\n"));
	return FSTypes_RC_FAILURE;
    }

    /* First of all contact the server to open the file; we only allocate
       state if this succeeds. */
    rc=NFS$Open(st->fs, dir->csel, mode, options, &handle);
    
    if (rc != FSTypes_RC_OK) return rc; /* Failed */
    
    /* At this point, handle is a handle on the open file */
    
    *fileio = setup_fileio(st, handle);
    
    return FSTypes_RC_OK;
}
    
static FSTypes_RC FSDir_Link_m (
    FSDir_cl        *self,
    FSTypes_Name     name    /* IN */ )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;
    FSTypes_RC   rc;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    rc=NFS$Link(st->fs, dir->cwd, name, dir->csel);

    return rc;
}

static FSTypes_RC FSDir_ReadDir_m (
    FSDir_cl           *self
    /* RETURNS */,
    FSTypes_DirentSeq **entries )
{
    FSDir_st          *dir = self->st;
    FSClient_st       *st  = dir->st;
    FSTypes_DirentSeq *seq;
    FSTypes_RC         rc;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    /* Check that inode is a directory */
    if (NFS$InodeType(st->fs, dir->csel)!=FSTypes_InodeType_Dir)
	return FSTypes_RC_FAILURE;

    rc       = NFS$ReadDir(st->fs, dir->csel, &seq);
    *entries = seq;
    return rc;
}

static FSTypes_RC FSDir_ReadLink_m (
    FSDir_cl        *self
    /* RETURNS */,
    FSTypes_Name    *contents )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;
    FSTypes_RC   rc;
    FSTypes_Name link;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    /* Check that the inode is a symlink */
    if (NFS$InodeType(st->fs, dir->csel)!=FSTypes_InodeType_Link)
	return FSTypes_RC_FAILURE;

    rc=NFS$Readlink(st->fs, dir->csel, &link);
    *contents = link;

    return rc;
}





/*--------------------------------------------- IDCOffer Entry Points ----*/


static  IDCOffer_Type_fn                IDCOffer_Type_m;
static  IDCOffer_PDID_fn                IDCOffer_PDID_m;
static  IDCOffer_GetIORs_fn             IDCOffer_GetIORs_m;
static  IDCOffer_Bind_fn                IDCOffer_Bind_m;

PUBLIC IDCOffer_op     fsoffer_ms = {
    IDCOffer_Type_m,
    IDCOffer_PDID_m,
    IDCOffer_GetIORs_m,
    IDCOffer_Bind_m
};


static Type_Code IDCOffer_Type_m(IDCOffer_cl *self)
{
    return FSClient_clp__code;
}

static ProtectionDomain_ID IDCOffer_PDID_m(IDCOffer_cl *self)
{
    return VP$ProtDomID(Pvs(vp));  /* because there is no IDC really */
}

static IDCOffer_IORList *IDCOffer_GetIORs_m(IDCOffer_cl *self)
{
    printf("NFS::IDCOffer_GetIORs_m: ERROR: UNIMPLEMENTED\n");

    return NULL;
}

static IDCClientBinding_clp IDCOffer_Bind_m (
    IDCOffer_cl     *self,
    Gatekeeper_clp   gk     /* IN */,
    Type_Any        *cl     /* OUT */ )
{
    IDCOffer_st *st = self->st;
    FSClient_st *fs_st;
    int 	 i;

    TRC(eprintf("Offer$Bind: self=%p, gk=%p, cl=%p.\n", self, gk, cl));

    fs_st       = Heap$Malloc(Pvs(heap), sizeof(*fs_st));
    CL_INIT(fs_st->cl, &fsclient_ms, fs_st);
    fs_st->heap = Pvs(heap);

    fs_st->fs   = nfs_init(&st->nfsaddr, &st->rootfh, 
			   st->blocksize, st->readonly, st->debug, 
			   st->server, st->mountpoint);

    fs_st->fmod = NAME_FIND("modules>FIFOMod", FIFOMod_clp);


    /* XXX AND: where's the memory supposed to come from? */
    for(i=0; i<CACHE_NFIXED; i++)
    {
	fs_st->cache[i].data = Heap$Malloc(fs_st->heap, CACHE_LINESZ);
	fs_st->cache[i].file = NULL;	/* line not in use */
    }

    /* XXX SMH: init our client surrogate and return it to client. */
    ANY_INIT(&fs_st->srgt, FSClient_clp, &fs_st->cl);
    ANY_COPY(cl, &fs_st->srgt);

    return NULL; 
}


/*------------------------------------- internel helper routines ---- */

/* writeback the cache line "c", if it is dirty. Returns 0 for
 * success, 1 for error.  */
static int cache_flushline(cacheline_t *c)
{
    FSTypes_RC	ok;
    NFS_clp	nfs;
    int		ret = 0;

    /* if we don't have a file associated, then there's nothing to do */
    if (!c->file)
	return 0;

    nfs = c->file->st->fs;

    if (c->dirty)
    {
	CTRC(eprintf(" + %p dirty, flushing...", c));

	ok = NFS$PutBlock(nfs, c->file->handle, c->nfsblock, c->data);

	if (ok == FSTypes_RC_OK) {
	    c->dirty = False;
	    CTRC(eprintf("ok\n"));
	} else {
	    ret++;	/* error return */
	    CTRC(eprintf("failed (%d)\n", ok));
	}
    }

    return ret;
}

/* returns the number of cachelines that failed to be correctly
 * flushed; ie 0 for success, != 0 for failure */
static int cache_flushlines(cacheline_t *c, int nlines)
{
    int		i;
    int		ret = 0;

    CTRC(eprintf("cache_flushlines(%p, %d) called\n", c, nlines));

    for(i=0; i < nlines; i++)
    {
	ret += cache_flushline(c);
	c++;
    }

    CTRC(eprintf("cache_flushlines done (%d errs)\n", ret));
    return ret;
}


/* Write one FILEIO_BUFSZ block from the buffer "buf" into an
 * appropriate cache line.  Return 1 for failure, 0 for success. */
static int cache_putblock(FileIO_st *st, FileIO_Block b, void *buf)
{
    cacheline_t *c;
    uint8_t	*dest;

    CTRC(eprintf("cache_putblock(%p, %d)\n", st, (int)b));

    /* Find a line for the block to go into.  May drag whole line in from
     * underlying media. */
    c = cache_findline(st, b);
    if (!c) return 1;

    dest = c->data + (b - c->blockbase)*FILEIO_BUFSZ;
    memcpy(dest, buf, FILEIO_BUFSZ);

    c->dirty = True;

    return 0;
}


/* Read one FILEIO_BUFSZ block "b" and copy it into the buffer
 * "buf".  Return 1 for failure, 0 for success.  */
static int cache_getblock(FileIO_st *st, FileIO_Block b, void *buf)
{
    cacheline_t *c;
    uint8_t	*src;

    CTRC(eprintf("cache_getblock(%p, %d)\n", st, (int)b));
    CSTAT(st->lookups++);

    c = cache_findline(st, b);
    if (!c) return 1;

    CSTAT(st->lasthit = c);

    src = c->data + (b - c->blockbase)*FILEIO_BUFSZ;
    memcpy(buf, src, FILEIO_BUFSZ);

#ifdef CACHE_STATS
    if (!(st->lookups % 50))
    {
	printf("CSTAT: %p: hits=%d/%d (%d%%), other=%d (%d%%)\n",
	       st, st->hits, st->lookups, st->hits * 100 / st->lookups,
	       st->otherhits, st->otherhits * 100 / st->lookups);
    }
#endif /* CACHE_STATS */

    return 0;
}



static INLINE cacheline_t *
cache_hunt(FileIO_st *st, FileIO_Block b, cacheline_t *c, int nlines)
{
    int i;

    CTRC(eprintf("cache_hunt(%p, %d)\n", st, (int)b));

    for(i=0; i < nlines; i++)
    {
	/* skip lines for different files */
	if (c->file != st)
	    continue;

	/* is "b" within this line's range? */
	if ((b >= c->blockbase) && (b < (c->blockbase + c->nblocks)))
	{
	    CTRC(eprintf(" + found at line %p [%d-%d]\n",
			c, (int)c->blockbase, (int)c->blockbase+c->nblocks-1));
	    return c;
	}

	c++;
    }

    return NULL;
}



/* Pick a good cacheline to load stuff into. */
static INLINE cacheline_t *
cache_findvictim(FileIO_st *st)
{
    int		start;
    int		pass;
    cacheline_t	*c;

    CTRC(eprintf("cache_findvictim(%p)\n", st));

    /* make two passes, the first time marking blocks as vulnerable, 
     * the second time actually picking something */
    for(pass=0; pass < 2; pass++)
    {
	CTRC(eprintf(" + pass %d, starting from pos %d...\n",
		    pass, st->cache_pos));

	start = st->cache_pos;

	do {
	    /* pick local or global as dictated by cache_pos */
	    if (st->cache_pos < CACHE_NFILEIO)
		c = &st->cache[st->cache_pos];
	    else
		c = &st->st->cache[st->cache_pos - CACHE_NFILEIO];

	    CTRC(if (c->file)
		 eprintf(" +    %p: file=%p [%d-%d], dirty=%s used=%d ?\n",
			c, c->file, (int)c->blockbase, 
			 ((int)c->blockbase) + c->nblocks - 1,
			 c->dirty?"yes":" no", c->used);
		 else
		 eprintf(" +    %p: file=%p ?\n", c, c->file);
		 );
		 

	    /* step on and maybe wrap around */
	    st->cache_pos++;
	    if (st->cache_pos >= CACHE_NFILEIO + CACHE_NFIXED)
		st->cache_pos = 0;

	    /* we always prefer empty lines */
	    if (!c->file)
	    {
		CTRC(eprintf(" +   found: empty\n"));
		return c;
	    }

	    if (c->used == 0)
	    {
		CTRC(eprintf(" +   found: used == 0\n"));
		return c;
	    }

	    /* this line has been reprieved, but next time, it's going out */
	    c->used = 0;

	} while (st->cache_pos != start);
    }

    printf("FileIO(NFS):cache_findvictim: nothing found, can't happen!\n");
    return st->cache;	/* bogus, but who cares */
}


/* Return the cacheline containing block "b" from file "st", or NULL
 * if there was any read error. */
static cacheline_t *cache_findline(FileIO_st *st, FileIO_Block b)
{
    cacheline_t	*c;

    CTRC(eprintf("cache_findline(%p, %d)\n", st, b));

    /* stage (1): do we already have the block in a line somewhere? */

    /* local, then global */
    c = cache_hunt(st, b, st->cache, CACHE_NFILEIO);
    if (!c)
	c = cache_hunt(st, b, st->st->cache, CACHE_NFIXED);

    if (c)
    {
	c->used++;
	CTRC(eprintf("cache_findline: hit: used=%d\n", c->used));
#ifdef CACHE_STATS
	st->hits++;
	if (c != st->lasthit)
	    st->otherhits++;
#endif CACHE_STATS
	return c;
    }


    /* stage (2): we don't have the line, so we need to make room to
       read it in. */

    c = cache_findvictim(st);
    /* flush the line if there's a file associated with it */
    if (c->file)
	if (cache_flushline(c))
	    return NULL;	/* failure */


    /* stage (3): load new data */
    {
	FSTypes_RC	ok;
	uint32_t	nbsz = NFS$BlockSize(st->st->fs, st->handle);
	FileIO_Block	nfsblock = b * FILEIO_BUFSZ / nbsz;

	CTRC(eprintf("cache_findline: loading nfs block %d to line %p\n",
		     (int)nfsblock, c));
	ok = NFS$FetchBlock(st->st->fs, st->handle, nfsblock, c->data);

	/* on failure, we assume that c->data hasn't been overwritten,
	 * so can re-use cache line we were about to replace. */
	if (ok != FSTypes_RC_OK)
	    return NULL;	

	/* fill in new cache line info */
	c->file = st;
	c->blockbase = nfsblock * nbsz / FILEIO_BUFSZ;	/* FileIO block num */
	c->nblocks = nbsz / FILEIO_BUFSZ;
	c->nfsblock = nfsblock;
	c->dirty = False;
	c->used = 1;
    }

    return c;
}


/*
 * End 
 */
