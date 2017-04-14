/*
*****************************************************************************
**									    *
**  Copyright 1997, University of Cambridge Computer Laboratory		    *
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
**	Ext2fs client side stubs
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
#include <stdio.h>

#include <ntsc.h>

#include <CSClientStubMod.ih>
#include <FSClient.ih>
#include <FSDir.ih>
#include <FSUtil.ih>
#include "ext2mod.h"

#ifdef DEBUG_FSCLIENT
#define TRC(_x) _x
#define ENTERED printf(__FUNCTION__": entered.\n.")
#else
#define TRC(_x)
#define ENTERED
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);


static	CSClientStubMod_New_fn		CSClientStubMod_New_m;
static	CSClientStubMod_Destroy_fn	CSClientStubMod_Destroy_m;

CSClientStubMod_op	stubmod_ms = {
    CSClientStubMod_New_m,
    CSClientStubMod_Destroy_m
};

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
static  FSDir_Delete_fn                 FSDir_Delete_m;
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

/* State for the whole client */
typedef struct {
    /* What we show to the world */
    FSClient_cl cl;

    /* A Heap for our own use */
    Heap_clp heap;

    /* Internal stuff; connection to filesystem server */
    Ext2_clp fs;

    /* Connection to USD */
    SRCThread_Mutex mu;		/* Protects access to usd */
    FileIO_clp usd;		/* Raw IO channel */
    FileIO_clp prefetch;	/* Inode Prefetch FileIO */

    /* thread to receive packets back from the usd */
    Thread_clp rxthread;

    /* Blocksize of the filesystem */
    uint32_t fs_blocksize;

    /* Blocksize of the USD */
    uint32_t usd_blocksize;

    /* Multiplier to get diskblocks from fsblocks (should be shift XXX) */
    uint32_t blockmult;
} FSClient_st;

/* Per-directory state */
typedef struct {
    FSDir_cl cl;
    FSClient_st *st;

    Ext2_Handle cwd;
    Ext2_Handle csel;		/* Current selection of inode, or -1 */
    
    Rd_cl     *rd;		/* Reader on the dir */

    FileIO_Request req;
    bool_t dir_done;
} FSDir_st;

struct rxentry {
    struct rxentry *next, *prev;
    IO_Rec	    recs[16];
    uint32_t	    nrecs;
    word_t	    value;
};
    

typedef struct {
    FileIO_cl cl;
    FSClient_st *st;
    Ext2_Handle handle;    /* Opaque thing we pass to Ext2 */
    USD_RequestID request; /* Opaque thing we pass to USD */

    /* Receive buffer for this file */
    SRCThread_Mutex mu;
    SRCThread_Condition cv;
    struct rxentry rxq;
    uint32_t pending; /* Number of recs still in transit */
} FileIO_st;

/*-------------------------------------------------- Receive Thread ----*/
static void RxThread(FSClient_st *st)
{
    IO_Rec          recs[16];
    word_t          value;
    uint32_t        nrecs;
    FileIO_Request *r;
    FileIO_st      *client;
    int             i;
    struct rxentry *entry;

    ENTERED;

    while(True) {
	IO$GetPkt((IO_clp)st->usd, 16, recs, FOREVER, &nrecs, &value);

	TRC(printf("fsclient: RxThread: got a packet\n"));

	r = recs[0].base;
	if (!r->user2) continue;

	/* Copy the information into an entry */
	entry = Heap$Malloc(st->heap, sizeof(*entry));
	for (i=0; i<nrecs; i++) {
	    entry->recs[i] = recs[i];
	}
	entry->nrecs = nrecs;
	entry->value = value;

	/* Ought to check length */

	/* Look at first rec to see which client this is for */
	client = (FileIO_st *)(word_t)(r->user2);

	/* Grab their mutex and add the entry to the queue, then
	   signal them */ 
	MU_LOCK(&client->mu);
	LINK_ADD_TO_TAIL(&client->rxq, entry);
	MU_RELEASE(&client->mu);
	SIGNAL(&client->cv);
    }

    /* Should do something about catching exceptions, dying, etc. */
}

/*--------------------------------------------- FileIO Entry Points ----*/

static bool_t FileIO_PutPkt_m (
    IO_cl	    *self,
    uint32_t	     nrecs   /* IN */,
    IO_Rec_ptr	     recs    /* IN */,
    word_t	     value   /* IN */,
    Time_T           until   /* IN */ )
{
    FileIO_st	   *st = self->st;
    FileIO_Request *ureq;
    bool_t          ok;

    ENTERED;

    TRC(printf("fsclient: in PutPkt\n"));

    /* This one's easy; just do the translation and push the recs down
       to the USD */
    ureq = recs[0].base;
    /* XXX should check len >= sizeof(FileIO_Request) */

    ureq->user1 = st->handle;    
    ureq->user2 = (uint64_t)(word_t)st; 
    /* So that we can signal when packet comes back up */

    TRC(printf("USD request: ureq=%p blockno=%d nblocks=%d\n\t"
	       "op=%qx user1=%qx user2=%qx\n", ureq,
	       ureq->blockno, ureq->nblocks, ureq->op,
	       ureq->user1, ureq->user2));

    MU_LOCK(&st->st->mu);
    if ((ok = IO$PutPkt((IO_clp)st->st->usd, nrecs, recs, value, until)))
	st->pending++;
    MU_RELEASE(&st->st->mu);

    TRC(printf("fsclient: leaving PutPkt\n"));
    return ok;
}

static bool_t FileIO_AwaitFreeSlots_m(IO_cl *self, 
				      uint32_t nslots, 
				      Time_T until)
{
    /* XXX This is probably wrong; fix it if you ever think you'll use it */
    return True;
}


static bool_t FileIO_GetPkt_m (
    IO_cl	   *self,
    uint32_t	    max_recs	    /* IN */,
    IO_Rec_ptr	    recs            /* IN */,
    Time_T          until           /* IN */,
    uint32_t       *nrecs,
    word_t	   *value          /* OUT */ )
{
    FileIO_st	   *st = self->st;
    struct rxentry *entry;
    int i;

    ENTERED;

    TRC(printf("FileIO: GetPkt: entered\n"));

    MU_LOCK(&st->mu);
    while(LINK_EMPTY(&st->rxq)) {
	if (until < NOW()) {
	    MU_RELEASE(&st->mu);
	    return False;
	}
	WAIT(&st->mu, &st->cv);
    }

    TRC(printf("FileIO: GetPkt: got one!\n"));

    entry = st->rxq.next;
    LINK_REMOVE(entry);
    st->pending--;
    MU_RELEASE(&st->mu);

    /* We ought to do something about translating the block number back.
       Um. Let's forget about that for now. XXX */

    /* XXX check against max_recs */
    for (i=0; i<entry->nrecs; i++) {
	recs[i] = entry->recs[i];
    }
    *value = entry->value;
    *nrecs = entry->nrecs;
    FREE(entry);

    return True;
}

static uint32_t FileIO_QueryDepth_m (
    IO_cl           *self )
{
    FileIO_st     *st = self->st;

    ENTERED;

    return IO$QueryDepth((IO_clp)st->st->usd);
}

static IO_Kind FileIO_QueryKind_m (
    IO_cl           *self )
{
    /* Almost has to be a Master, doesn't it? */
    ENTERED;

    return IO_Kind_Master;
}

static IO_Mode FileIO_QueryMode_m (
    IO_cl           *self )
{
    return IO_Mode_Dx;
}

static IOData_Shm *FileIO_QueryShm_m (
    IO_cl           *self )
{
    FileIO_st     *st = self->st;

    ENTERED;

    return IO$QueryShm((IO_clp)st->st->usd);
}

static void FileIO_QueryEndpoints_m (IO_clp self, Channel_Pair *pair) 
{
    FileIO_st    *st = self->st;
    
    ENTERED;

    IO$QueryEndpoints((IO_clp)st->st->usd, pair);
}

static void FileIO_Dispose_m (
    IO_cl	    *self )
{
    FileIO_st 	   *st = self->st;
    struct rxentry *entry;

    ENTERED;

    /* We need to dispose of all returned recs from the USD before
       deleting our data. */
    MU_LOCK(&st->mu);
    while (st->pending) {
	TRC(printf("FileIO: Dispose: %d packets pending\n",st->pending));
	while(LINK_EMPTY(&st->rxq)) {
	    WAIT(&st->mu, &st->cv);
	}

	entry = st->rxq.next;
	LINK_REMOVE(entry);
	st->pending--;
    }
    MU_RELEASE(&st->mu);

    /* Free the handle */
    Ext2$Free(st->st->fs, st->handle, False);

    CV_FREE(&st->cv);
    MU_FREE(&st->mu);

    FREE(st);
}

static uint32_t FileIO_BlockSize_m (
    FileIO_cl	    *self )
{
    FileIO_st	  *st = self->st;

    ENTERED;

    return st->st->fs_blocksize;
}

static FileIO_Size FileIO_GetLength_m (
    FileIO_cl *self )
{
    FileIO_st *st = self->st;

    ENTERED;

    return Ext2$Size(st->st->fs, st->handle);
}

static FSTypes_RC FileIO_SetLength_m (
    FileIO_cl	    *self,
    FileIO_Size      length  /* IN */ )
{

    ENTERED;

    return FSTypes_RC_DENIED;
}

/*------------------------------------------- FSClient Entry Points ----*/

static FSTypes_QoS *FSClient_CurrentQoS_m (
    FSClient_cl	    *self )
{
    FSClient_st	  *st = self->st;
    
    ENTERED;

    return Ext2$CurrentQoS(st->fs);
}

static bool_t FSClient_AdjustQoS_m (
    FSClient_cl	    *self,
    FSTypes_QoS	    *qos    /* INOUT */ )
{
    FSClient_st	  *st = self->st;

    ENTERED;

    return Ext2$AdjustQoS(st->fs, qos);
}

static FSTypes_RC FSClient_GetDir_m (
    FSClient_cl     *self,
    string_t         certificate     /* IN */,
    bool_t  root    /* IN */
    /* RETURNS */,
    Type_Any       **dir )
{
    FSClient_st   *st = self->st;
    FSDir_st      *d;
    Type_Any      *dir_r;

    ENTERED;

    /* Construct state */
    d       = Heap$Malloc(st->heap, sizeof(*d));
    CL_INIT(d->cl, &dir_ms, d);
    d->st   = st;
    d->csel = -1;
    d->rd   = NULL;

    if (Ext2$Root(st->fs, &d->cwd)!=FSTypes_RC_OK) {
	Heap$Free(st->heap, d);
	return FSTypes_RC_FAILURE; /* Probably out of handles */
    }

    dir_r = Heap$Malloc(st->heap, sizeof(*dir_r));
    ANY_INIT(dir_r, FSDir_clp, &d->cl);
    *dir  = dir_r;

    return FSTypes_RC_OK;
}

static FSTypes_Info *FSClient_Stat_m (
    FSClient_cl *self )
{

    ENTERED;

    return NULL;
}

static FSTypes_RC FSClient_Unmount_m (
    FSClient_cl     *self,
    bool_t           force   /* IN */ )
{
    FSClient_st   *st = self->st;

    ENTERED;

    /* Kaboom */
    return Ext2$Unmount(st->fs, force);
}

/*---------------------------------------------- FSDir Entry Points ----*/

/* 
** SDE: note that this always gets a reader on the inode that is
** _currently looked up_. Not much use if you're in the middle of a
** lookup anyway... 
*/
static FSTypes_RC getrd(FSDir_cl *self)
{
    FSDir_st   *dir = self->st;
    FSUtil_cl  *fsutil;
    FSTypes_RC  rc;
    FileIO_cl  *fio;
    Rd_cl      *rd;

    if (dir->rd) ntsc_dbgstop();

    rc  = FSDir$Open(self, 0, FSTypes_Mode_Read, 0, &fio);
    if (rc != FSTypes_RC_OK) return rc;

    fsutil = NAME_FIND("modules>FSUtil", FSUtil_clp);
    rd = FSUtil$GetRd(fsutil, fio, dir->st->heap, True);
    
    if (!rd) ntsc_dbgstop();

    dir->rd = rd;

    return FSTypes_RC_OK;
}

#if 0
static void prefetch_inode(FSDir_cl *self, 
			   uint32_t  ino)
{
    FSDir_st	   *dir   = self->st;
    FileIO_Request *ureq  = &dir->req;
    FileIO_st      *fiost = dir->st->prefetch->st;
    IO_Rec          recs[2];
    uint32_t        nrecs;
    word_t          value = 0;

    ureq->blockno = 0;
    ureq->nblocks = 0;
    ureq->op      = FileIO_Op_Read;

    fiost->handle = ino;
    /* So that we can signal when packet comes back up */
    TRC(printf("USD request: ureq=%p blockno=%d nblocks=%d\n\t"
	       "op=%qx user1=%qx user2=%qx\n", ureq,
	       ureq->blockno, ureq->nblocks, ureq->op,
	       ureq->user1, ureq->user2));

    recs[0].base = ureq;
    recs[0].len  = sizeof(FileIO_Request);
    recs[1].base = BADPTR;
    recs[1].len  = 0;

    IO$PutPkt((IO_clp)dir->st->prefetch, 2, recs, value, FOREVER);
    IO$GetPkt((IO_clp)dir->st->prefetch, 2, recs, FOREVER, &nrecs, &value);
}
#endif /* 0 */

static FSTypes_RC FSDir_Lookup_m (
    FSDir_cl        *self,
    FSTypes_Name     name    /* IN */,
    bool_t           follow  /* IN */ )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;
    Ext2_Handle  h;
    FSTypes_RC   rc;
#if 0
    ext2_dir_t   d;
    bool_t       again;
    char        *c;
#endif /* 0 */

    ENTERED;

    /* Free the currently selected handle, if there is one */
    Ext2$Free(st->fs, dir->csel, False);
    dir->csel = -1;
    if (dir->rd) {
	Rd$Close(dir->rd);
	dir->rd = NULL;
    }

    /* XXX PRB We would really like to be able to do the lookup
       oursleves, especially in simple cases.  This stops us having to
       wait around for the ext2 server to do it, thereby throwing away
       our remaining guaranteed disk access time.
       
       The problem is:
       We can do the lookup to get an inode number, but then there is 
       no way for the Ext2 server to work out how we got there.  We
       then can't do access control properly.
     
       All we are trying to do is make sure that the inode block is in
       the ext2server's cache - preferably with the IO accounted to
       the client.

       A quick hack could be to issue a prefetch hint which causes the
       ext2server to pull in the inode during the translation callback
       (This will be accounted to the client issuing the request)

       */

    goto fallback;
       
#if 0
    /* If name is not simple then goto fallback */
    c = name;
    while (*c) {
	if (*c == '/') goto fallback;
	c++;
    }

    /* See if we can do the lookup ourselves */
    if (!dir->rd) {
	rc = getrd(self); 
	if (rc != FSTypes_RC_OK) goto fallback;
    }

    /* Try reading at current position - everybody acceses 
       directories sequentially :-) 
       If we get to the end, we start again at the beginning. */

    again = (Rd$Index(dir->rd) != 0);
again:

#undef EOF
    while(!Rd$EOF(dir->rd)) {
	uint32_t pos = Rd$Index(dir->rd);
	Rd$GetChars(dir->rd, (char *)&d, 8);
	TRC(printf("inode %d rec_len %d name_len %d\n", 
		   d.inode, d.rec_len, d.name_len));
	Rd$GetChars(dir->rd, (char *)&d.name, d.name_len);
	d.name[d.name_len] = 0;
	Rd$Seek(dir->rd, pos + d.rec_len);
	TRC(printf("next name is '%s'\n", d.name));
	if (!strncmp(name, d.name, d.name_len)) {
	    TRC(printf("GOT IT!\n"));
	    prefetch_inode(self, d.inode);
	    goto fallback; 
			   
	}
    }
    if (again) {
	Rd$Seek(dir->rd, 0);
	again = False;
	goto again;
    }
       
#endif /* 0 */
fallback:
    /* Last resort: get server to do the lookup and store the result */
    rc = Ext2$Lookup(st->fs, dir->cwd, name, follow, &h);
    if (rc == FSTypes_RC_OK) {
	dir->csel=h;
	dir->dir_done=False;
    }

    return rc;
}

static FSTypes_RC FSDir_Create_m (
    FSDir_cl        *self,
    FSTypes_Name     name    /* IN */ )
{

    ENTERED;

    return FSTypes_RC_FAILURE;
}

static FSTypes_RC FSDir_MkDir_m (
    FSDir_cl        *self,
    FSTypes_Name     name    /* IN */ )
{

    ENTERED;

    return FSTypes_RC_FAILURE;
}

static FSTypes_RC FSDir_Rename_m (
    FSDir_cl        *self,
    FSTypes_Name     from    /* IN */,
    FSTypes_Name     to      /* IN */ )
{

    ENTERED;

    return FSTypes_RC_FAILURE;
}

static FSTypes_RC FSDir_Symlink_m (
    FSDir_cl        *self,
    FSTypes_Name     name    /* IN */,
    FSTypes_Name     path    /* IN */ )
{

    ENTERED;

    return FSTypes_RC_FAILURE;
}

static FSTypes_RC FSDir_Duplicate_m (
    FSDir_cl        *self,
    /* RETURNS */
    Type_Any       **fsdir )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;
    FSDir_st    *nd;
    Ext2_Handle  h;
    Type_Any    *fsdir_r;

    ENTERED;

    nd = Heap$Malloc(st->heap, sizeof(*nd));
    CL_INIT(nd->cl, &dir_ms, nd);
    nd->st = st;

    if (Ext2$Lookup(st->fs, dir->cwd, "", False, &h)!=FSTypes_RC_OK) {
	/* Out of handles */
	Heap$Free(st->heap, nd);
	return FSTypes_RC_FAILURE;
    }

    nd->cwd  = h;
    nd->csel = -1; /* Don't duplicate handle */
    nd->rd   = NULL;

    fsdir_r = Heap$Malloc(st->heap, sizeof(*fsdir_r));
    ANY_INIT(fsdir_r, FSDir_clp, &nd->cl);
    *fsdir  = fsdir_r;

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_Delete_m (
    FSDir_cl    *self,
    FSTypes_Name name )
{
    ENTERED;

    return FSTypes_RC_FAILURE;
}

static void FSDir_Close_m (
    FSDir_cl        *self )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    ENTERED;

    /* Free the CWD */
    Ext2$Free(st->fs, dir->cwd, False);

    /* Free the currently selected inode, if appropriate */
    if (dir->csel != -1)
	Ext2$Free(st->fs, dir->csel, False);
    
    /* If we have a reader on the dir, close it */
    if (dir->rd) Rd$Close(dir->rd);

    /* Free state */
    Heap$Free(st->heap, dir);

    return;
}

static string_t FSDir_Delegate_m (
    FSDir_cl        *self )
{

    ENTERED;

    return "unimplemented";
}

static void FSDir_Release_m (
    FSDir_cl        *self )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    ENTERED;

    /* Free the currently selected inode, if appropriate */
    if (dir->csel != -1)
	Ext2$Free(st->fs, dir->csel, False);

    return;
}

static FSTypes_RC FSDir_InodeType_m (
    FSDir_cl                *self
    /* RETURNS */,
    FSTypes_InodeType       *type )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    ENTERED;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    *type = Ext2$InodeType(st->fs, dir->csel);

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_CWD_m (
    FSDir_cl        *self )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    ENTERED;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    /* Check that the currently selected inode is a directory */
    if (Ext2$InodeType(st->fs, dir->csel)!=FSTypes_InodeType_Dir) {
	return FSTypes_RC_FAILURE;
    }

    /* Ok, we're happy. Free the cwd, swizzle the handles, and return */
    Ext2$Free(st->fs, dir->cwd, False);

    dir->cwd  = dir->csel;
    dir->csel = -1;

    /* Bin the reader (if initialised) */
    if (dir->rd) Rd$Close(dir->rd);
    dir->rd = NULL;

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_Size_m (
    FSDir_cl        *self
    /* RETURNS */,
    FileIO_Size     *size )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    ENTERED;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    *size = Ext2$Size(st->fs, dir->csel);

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_NLinks_m (
    FSDir_cl        *self
    /* RETURNS */,
    FSTypes_NLinks  *n )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    ENTERED;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    *n=Ext2$NLinks(st->fs, dir->csel);

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_NForks_m (
    FSDir_cl        *self
    /* RETURNS */,
    uint32_t        *n )
{
    FSDir_st      *dir = self->st;

    ENTERED;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    *n = 1; /* Constant for ext2fs */
    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_ATime_m (
    FSDir_cl        *self
    /* RETURNS */,
    FSTypes_Time    *atime )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    ENTERED;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    *atime = Ext2$ATime(st->fs, dir->csel);

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_MTime_m (
    FSDir_cl        *self
    /* RETURNS */,
    FSTypes_Time    *mtime )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    ENTERED;

    if (dir->csel==-1) return FSTypes_RC_FAILURE;

    *mtime = Ext2$MTime(st->fs, dir->csel);

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_CTime_m (
    FSDir_cl        *self
    /* RETURNS */,
    FSTypes_Time    *ctime )
{
    FSDir_st    *dir = self->st;
    FSClient_st *st  = dir->st;

    ENTERED;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    *ctime = Ext2$CTime(st->fs, dir->csel);

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_Open_m (
    FSDir_cl       *self,
    uint32_t        fork,
    FSTypes_Mode    mode    /* IN */,
    FSTypes_Options options /* IN */
    /* RETURNS */,
    FileIO_clp     *fileio )
{
    FSDir_st      *dir = self->st;
    FSClient_st   *st  = dir->st;
    Ext2_Handle    handle;
    USD_RequestID  request;
    FSTypes_RC     rc;
    FileIO_st     *fio;

    ENTERED;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    if (fork != 0) return FSTypes_RC_FAILURE;

#if 0 
    /* Check that the currently selected inode is a file */
    /* XXX PRB: No. We sneakily use this method to open a directory 
       to implement ReadDir in the client */ 
    if (Ext2$InodeType(st->fs, dir->csel)!=FSTypes_InodeType_File)
	return FSTypes_RC_FAILURE;
#endif

    /* First of all contact the server to open the file; we only allocate
       state if this succeeds. */
    rc=Ext2$Open(st->fs, dir->csel, mode, options, &handle, &request);

    if (rc != FSTypes_RC_OK) return rc; /* Failed */

    /* At this point, handle is a handle on the open file */

    fio          = Heap$Malloc(st->heap, sizeof(*fio));
    CL_INIT(fio->cl, &fileio_ms, fio);
    fio->st      = st;
    fio->handle  = handle;
    fio->request = request;
    fio->pending = 0;

    LINK_INIT(&fio->rxq);
    MU_INIT(&fio->mu);
    CV_INIT(&fio->cv);

    *fileio      = &fio->cl;

    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_Link_m (
    FSDir_cl        *self,
    FSTypes_Name     name    /* IN */ )
{
    FSDir_st      *dir = self->st;

    ENTERED;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    return FSTypes_RC_FAILURE;
}

static FSTypes_RC FSDir_ReadDir_m (
    FSDir_cl                 *self
    /* RETURNS */,
    FSTypes_DirentSeq       **entries )
{
    FSDir_st          *dir = self->st;
    FSClient_st       *st  = dir->st;
    uint32_t           len;
    char              *buf, *p;

    FSTypes_DirentSeq *list;
    FSTypes_Dirent     ent;
    ext2_dir_t        *d;

    ENTERED;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    if (dir->dir_done) {
	dir->dir_done = False;
	list          = SEQ_CLEAR(SEQ_NEW(FSTypes_DirentSeq,0,Pvs(heap)));
	*entries      = list;
	return FSTypes_RC_OK;
    }

    /* Check that inode is a directory */
    if (Ext2$InodeType(st->fs, dir->csel)!=FSTypes_InodeType_Dir)
	return FSTypes_RC_FAILURE;
    
    if (!dir->rd) {
	if (getrd(self) != FSTypes_RC_OK) ntsc_dbgstop();
    }

    Rd$Seek(dir->rd, 0);

    len = Rd$Length(dir->rd);

    buf = Heap$Malloc(Pvs(heap), len);

    if (Rd$GetChars(dir->rd, buf, len) != len) ntsc_dbgstop();
    Rd$Seek(dir->rd, 0);

    list = SEQ_CLEAR(SEQ_NEW(FSTypes_DirentSeq, 0, Pvs(heap)));
    p = buf;
    while (p < buf+len) {
 	d = (ext2_dir_t *)p;
	if (d->name_len == 0) { 
	    TRC(printf("directory's name is zero-length, leaving.\n"));
	    break;
	}
	ent.ino  = d->inode;
	ent.name = Heap$Malloc(Pvs(heap), d->name_len+1);
	if (!ent.name) printf("ext2 fsclient: out of memory\n");
	strncpy(ent.name, d->name, d->name_len);
	ent.name[d->name_len] = 0;
	TRC(printf("%8d %s\n", ent.ino, ent.name));
	SEQ_ADDH(list, ent);
	p += d->rec_len;
    }
    FREE(buf);
    TRC(printf("ext2 fsclient: getdir: about to return\n"));

    dir->dir_done = True;
    /* We don't close the reader since there is a fair chance it 
       can be used again by Lookup. */
    *entries = list;
    return FSTypes_RC_OK;
}

static FSTypes_RC FSDir_ReadLink_m (
    FSDir_cl        *self
    /* RETURNS */,
    FSTypes_Name    *contents )
{
    FSDir_st      *dir = self->st;

    ENTERED;

    if (dir->csel == -1) return FSTypes_RC_FAILURE;

    return FSTypes_RC_FAILURE;
}

/*------------------------------------ CSClientStubMod Entry Points ----*/

static Type_Any *CSClientStubMod_New_m (
    CSClientStubMod_cl	    *self,
    const Type_Any          *server )
{
    FSClient_st  *st;
    Type_Any     *rval;
    IDCOffer_clp  usdoffer;

    ENTERED;

    /* Right. We're running in the client, and we're in the middle of
       the Bind to the publicly-exported offer. We need to set up our
       private state. */

    TRC(printf("FSClient: building client stubs\n"));

    st       = Heap$Malloc(Pvs(heap), sizeof(*st));
    CL_INIT(st->cl, &fsclient_ms, st);
    st->heap = Pvs(heap);

    st->fs   = NARROW(server, Ext2_clp); /* XXX may raise exception
					  * which we don't catch, and
					  * don't document in the
					  * interface. */

    usdoffer = Ext2$GetIO(st->fs);
    st->usd  = IDC_BIND(usdoffer, FileIO_clp); /* XXX probably don't
						* want to do it
						* through the object
						* table */
    MU_INIT(&st->mu);

    /* Fork off a thread to receive packets from the USD */
    st->rxthread = Threads$Fork(Pvs(thds), RxThread, st, 0);

    /* Fetch a little information from the FS server */
    st->fs_blocksize = Ext2$BlockSize(st->fs, &st->usd_blocksize);

    st->blockmult = st->fs_blocksize / st->usd_blocksize;


    {
	FileIO_st *fio;

	fio          = Heap$Malloc(st->heap, sizeof(*fio));
	CL_INIT(fio->cl, &fileio_ms, fio);
	fio->st      = st;
	fio->handle  = 0;
	fio->request = 0;
	fio->pending = 0;
	
	LINK_INIT(&fio->rxq);
	MU_INIT(&fio->mu);
	CV_INIT(&fio->cv);

	st->prefetch = &fio->cl;
    }

    rval = Heap$Malloc(Pvs(heap),sizeof(*rval));
    ANY_INIT(rval, FSClient_clp, &st->cl);

    return rval;
}

static void CSClientStubMod_Destroy_m (
    CSClientStubMod_cl	    *self,
    const Type_Any          *stubs  /* IN */ )
{
    ENTERED;

    UNIMPLEMENTED;
}

/*
 * End 
 */
