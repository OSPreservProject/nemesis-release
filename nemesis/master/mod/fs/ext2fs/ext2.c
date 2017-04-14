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
**      mod/fs/ext2fs
**
** FUNCTIONAL DESCRIPTION:
** 
**      Server implementation for ext2 filesystems
** 
** ENVIRONMENT: 
**
**      Module.
** 
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <bstring.h>
#include <ntsc.h>

#include <time.h>
#include <ecs.h>

#include <Ext2.ih>

#include "ext2mod.h"

/* #define CONFIG_LOGGING */

#ifdef DEBUG_EXT2
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef DEBUG_HANDLES
#define HTRC(_x) _x
#else
#define HTRC(_x)
#endif

#define DB(_x) _x

#define MAX_SYMLINK_FOLLOW 8

#define UNIMPLEMENTED  printf("%s: UNIMPLEMENTED\n", __FUNCTION__);

/* The Ext2 interface is internal to the ext2fs code. The rest of the world
   sees the FSClient interface, which is implemented in fsclient.c */

static  Ext2_GetIO_fn           GetIO_m;
static  Ext2_BlockSize_fn       BlockSize_m;
static  Ext2_CurrentQoS_fn      CurrentQoS_m;
static  Ext2_AdjustQoS_fn       AdjustQoS_m;
static  Ext2_Free_fn            Free_m;
static  Ext2_Root_fn            Root_m;
static  Ext2_InodeType_fn       InodeType_m;
static  Ext2_Size_fn            Size_m;
static  Ext2_NLinks_fn          NLinks_m;
static  Ext2_ATime_fn           ATime_m;
static  Ext2_MTime_fn           MTime_m;
static  Ext2_CTime_fn           CTime_m;
static  Ext2_Open_fn            Open_m;
static  Ext2_Create_fn          Create_m;
static  Ext2_Translate_fn       Translate_m;
static  Ext2_SetLength_fn       SetLength_m;
static  Ext2_Lookup_fn          Lookup_m;
static  Ext2_Link_fn            Link_m;
static  Ext2_Unlink_fn          Unlink_m;
static  Ext2_MkDir_fn           MkDir_m;
static  Ext2_RmDir_fn           RmDir_m;
static  Ext2_Rename_fn          Rename_m;
static  Ext2_ReadDir_fn         ReadDir_m;
static  Ext2_Symlink_fn         Symlink_m;
static  Ext2_Readlink_fn        Readlink_m;
static  Ext2_Unmount_fn         Unmount_m;

Ext2_op fs_ms = {
    GetIO_m,
    BlockSize_m,
    CurrentQoS_m,
    AdjustQoS_m,
    Free_m,
    Root_m,
    InodeType_m,
    Size_m,
    NLinks_m,
    ATime_m,
    MTime_m,
    CTime_m,
    Open_m,
    Create_m,
    Translate_m,
    SetLength_m,
    Lookup_m,
    Link_m,
    Unlink_m,
    MkDir_m,
    RmDir_m,
    Rename_m,
    ReadDir_m,
    Symlink_m,
    Readlink_m,
    Unmount_m
};

static Ext2_Handle gethandle(Client_st *cur)
{
    uint32_t i;
    uint32_t filehandle=-1;

    /* Before proceeding, find a free file slot. If there isn't one, fail */
    /* Probably need a mutex here */
    for (i=0; i<MAX_CLIENT_HANDLES; i++) {
	if (!cur->handles[i].used) {
	    filehandle=i;
	    break;
	}
    }
    if (filehandle!=-1) {
	cur->handles[filehandle].used     = True;
	/* Clear fields */
	cur->handles[filehandle].open     = False;
	cur->handles[filehandle].ino      = NULL;
	cur->handles[filehandle].flags    = 0;
	cur->handles[filehandle].dirblock = 0;
    }
    return filehandle;
}

PUBLIC bool_t check_handle(Client_st *cur, Ext2_Handle handle)
{
    HTRC(printf(__FUNCTION__": entered, handle=%d.\n", handle));

    if (handle>=MAX_CLIENT_HANDLES) return False;

    HTRC(printf(__FUNCTION__": stay on target, almost there...\n"));

    if (!cur->handles[handle].used) return False;

    HTRC(printf(__FUNCTION__": leaving, sucess.\n"));

    return True;
}

PUBLIC bool_t check_inode_handle(Client_st *cur, Ext2_Handle handle)
{
    HTRC(printf(__FUNCTION__": entered, handle=%d.\n", handle));

    if (!check_handle(cur,handle)) return False;

    HTRC(printf(__FUNCTION__": stay on target, almost there...\n"));

    if (cur->handles[handle].open) return False;

    HTRC(printf(__FUNCTION__": leaving, sucess.\n"));

    return True;
}

PUBLIC bool_t check_file_handle(Client_st *cur, Ext2_Handle handle)
{
    if (!check_handle(cur,handle)) return False;

    if (!cur->handles[handle].open) return False;

    return True;
}

/*----------------------------------------------- Ext2 Entry Points ----*/

static IDCOffer_clp GetIO_m (
    Ext2_cl     *self )
{
    Client_st *cur = self->st;

    /* Return the IDCOffer for the client's connection to the USD */
    return cur->usd;
}

static uint32_t BlockSize_m (
    Ext2_cl	*self,
    /* RETURNS */
    uint32_t    *diskblocksize )
{
    Client_st *cur = self->st;
    ext2fs_st *st  = cur->fs;

    *diskblocksize = st->disk.phys_block_size;

    return st->fsc.block_size;
}

static FSTypes_QoS *CurrentQoS_m (
    Ext2_cl	*self)
{
    Client_st *cur = self->st;

    return &cur->qos;
}

static bool_t AdjustQoS_m (
    Ext2_cl	*self,
    FSTypes_QoS	*qos	/* INOUT */ )
{
    Client_st *cur = self->st;
    ext2fs_st *st  = cur->fs;
    USD_QoS    usdqos;

    usdqos.p        = qos->p;
    usdqos.s        = qos->s;
    usdqos.l        = qos->l;
    usdqos.x        = qos->x;

    TRC(printf(__FUNCTION__ ": (%ld,%ld,%ld,%ld)\n", 
	       usdqos.p, usdqos.s, usdqos.l, usdqos.x));
    if(USDCtl$AdjustQoS(st->disk.usdctl, cur->usd_stream, &usdqos) 
       == USDCtl_Error_None) {
	bcopy(qos, &cur->qos, sizeof(FSTypes_QoS));
	return True;
    } 
    
    return False;
}

static bool_t Free_m (
    Ext2_cl        *self,
    Ext2_Handle     handle  /* IN */,
    bool_t          sync    /* IN */ )
{
    Client_st *cur = self->st;
    ext2fs_st *st  = cur->fs;

    TRC(printf(__FUNCTION__": entered.\n"));

    /* Verify handle is valid */
    if (!check_handle(cur,handle)) return False;

    

    if (cur->handles[handle].open) {
	/* Do something about removing TLB entries in USD */
    }

    /* Free handle */
    release_inode(st, cur->handles[handle].ino);
    cur->handles[handle].used=False;

    return True;
}

static FSTypes_RC Root_m (
    Ext2_cl         *self
    /* RETURNS */,
    Ext2_Handle     *root )
{
    Client_st  *cur = self->st;
    ext2fs_st  *st  = cur->fs;
    Ext2_Handle r;

    TRC(printf(__FUNCTION__": entered.\n"));

    r=gethandle(cur);

    if (r==-1) {
	TRC(printf(__FUNCTION__": returning, failed.\n"));
    
	return FSTypes_RC_FAILURE;
    }
    
    cur->handles[r].ino=get_inode(st, EXT2_ROOT_INO);
    
    *root=r;
    
    TRC(printf(__FUNCTION__": returning, success.\n"));
    return FSTypes_RC_OK;
}

static FSTypes_InodeType InodeType_m(Ext2_cl *self, Ext2_Handle inode)
{
    Client_st        *cur = self->st;
    struct inode     *ino;
    FSTypes_InodeType rval;

    if (!check_inode_handle(cur,inode)) return FSTypes_InodeType_Special;

    ino=cur->handles[inode].ino;

    rval=FSTypes_InodeType_Special;

    if (S_ISREG(ino->i_mode))
	rval = FSTypes_InodeType_File;

    else if (S_ISDIR(ino->i_mode)) 
	rval = FSTypes_InodeType_Dir;
    
    else if (S_ISLNK(ino->i_mode)) 
	rval = FSTypes_InodeType_Link;


    return rval;
}

static FileIO_Size Size_m (
    Ext2_cl	*self,
    Ext2_Handle	 inode	/* IN */ )
{
    Client_st    *cur = self->st;
    struct inode *ino = NULL;

    /* Can be called on an inode or on an open file */
    if (!check_handle(cur,inode)) return 0;

    ino = cur->handles[inode].ino;

    return ino->i_size;
}

static FSTypes_NLinks NLinks_m (
    Ext2_cl	*self,
    Ext2_Handle	 inode	/* IN */ )
{
    Client_st    *cur = self->st;
    struct inode *ino = NULL;

    if (!check_handle(cur,inode)) return 0;

    ino=cur->handles[inode].ino;

    return ino->i_nlink;
}

static FSTypes_Time ATime_m (
    Ext2_cl	*self,
    Ext2_Handle	 inode	/* IN */ )
{
    Client_st    *cur = self->st;
    struct inode *ino = NULL;

    if (!check_handle(cur,inode)) return 0;

    ino = cur->handles[inode].ino;

    return ino->i_atime;
}

static FSTypes_Time MTime_m (
    Ext2_cl	*self,
    Ext2_Handle	 inode	/* IN */ )
{
    Client_st    *cur = self->st;
    struct inode *ino = NULL;

    if (!check_handle(cur,inode)) return 0;

    ino=cur->handles[inode].ino;

    return ino->i_mtime;
}

static FSTypes_Time CTime_m (
    Ext2_cl	*self,
    Ext2_Handle	 inode	/* IN */ )
{
    Client_st    *cur = self->st;
    struct inode *ino = NULL;

    if (!check_handle(cur,inode)) return 0;

    ino=cur->handles[inode].ino;

    return ino->i_ctime;
}

static FSTypes_RC Open_m (
    Ext2_cl	       *self,
    Ext2_Handle		file	/* IN */,
    FSTypes_Mode	mode	/* IN */,
    FSTypes_Options	options /* IN */,
    /* RETURNS */
    Ext2_Handle	       *handle,
    USD_RequestID      *id )
{
    Client_st *cur = self->st;
    ext2fs_st *st  = cur->fs;
    uint32_t   filehandle;

    TRC(printf("Ext2$Open entered: session=%p, file=%d\n", cur, file));

    /* Check that file is a valid handle */
    if (!check_inode_handle(cur,file)) {
	printf("ext2fs: %d is not a valid inode handle\n");
	return FSTypes_RC_FAILURE;
    }

#ifdef CONFIG_LOGGING
    { 
	/* XXX PRB: LOGGING HACK */
	USD_Extent e;
	e.base = S_ISREG(cur->handles[file].ino->i_mode) ? 4 : 5;
	e.len  = cur->handles[file].ino->i_ino;
	USDCtl$Request(st->disk.usdctl, &e, FileIO_Op_Sync, NULL);
    }
#endif

    /* XXX check principal and access type, etc */
    /* Check inode type is correct */

#if 0 /* XXX PRB: we now allow opening of directories for read as well */
    if(!S_ISREG(cur->handles[file].ino->i_mode)) {
	TRC(printf("Ext2$Open: inode does not represent a file.\n"));
	return FSTypes_RC_FAILURE; 
    }
#endif 0

#if 0
    if (not_allowed) {
	cur->handles[filehandle].used=False;
	return FSTypes_RC_DENIED;
    }
#endif /* 0 */

    /* Get a new handle for the open file */
    filehandle = gethandle(cur);
    if (filehandle == -1) {
	printf("ext2fs: out of file handles for client %p\n", cur);
	return FSTypes_RC_FAILURE;
    }
    TRC(printf("ext2fs: using file handle %d for client %p\n", filehandle));

    cur->handles[filehandle].open = True;
    cur->handles[filehandle].ino  = 
	get_inode(st, cur->handles[file].ino->i_ino);

    if (!cur->handles[filehandle].ino) {
	printf("ext2fs: internal problem: couldn't re-lock inode\n");
	return FSTypes_RC_FAILURE;
    }

    *handle = filehandle;
    *id     = filehandle;

    TRC(printf("ext2fs: Open succeeded\n"));

    return FSTypes_RC_OK;
}

static FSTypes_RC Create_m (
    Ext2_cl         *self,
    Ext2_Handle      dir,
    FSTypes_Name     name,
    FSTypes_Options  options,
    /* RETURNS */
    Ext2_Handle     *handle )
{
    return FSTypes_RC_FAILURE;
}

static FSTypes_RC Translate_m (
    Ext2_cl	       *self,
    Ext2_Handle         handle,
    FileIO_Block        fileblock,
    bool_t              allocate,
    FileIO_Block       *fsblock   /* OUT */,
    uint32_t           *runlength /* OUT */ )
{
    Client_st    *cur = self->st;
    ext2fs_st    *st  = cur->fs;
    struct inode *ino;
    FileIO_Block  out;
    
    if (!check_file_handle(cur, handle))
	return FSTypes_RC_FAILURE;

    ino = cur->handles[handle].ino;

    out = ext2_bmap(st, ino, fileblock);
    TRC(printf("Translate: inode %d fileblock %d fsblock %d\n",
	       ino->i_ino, fileblock, out));
    *fsblock   = out;
    *runlength = 1;

    return FSTypes_RC_OK;
}

static FSTypes_RC SetLength_m (
    Ext2_cl     *self,
    Ext2_Handle  handle,
    FileIO_Size  length )
{
    Client_st *cur = self->st;

    if (!check_file_handle(cur, handle))
	return FSTypes_RC_FAILURE;

    /* Check open mode, etc. */

    return FSTypes_RC_FAILURE;
}

static INLINE uint32_t dname_eq(ext2_dir_t *dir, char *name)
{
    int i        = 0; 
    bool_t match = True;

    while((i < dir->name_len) && match) {
	if(dir->name[i] != name[i])
	    match = False;
	i++;
    }

    if (!match || (name[i] != '\0'))
	return EXT2_BAD_INO;

    return dir->inode;
}


/*
** Define below to start dlookup from where we last stopped. 
** Good if you're doing a lot of sequential accesses. 
*/
#undef DLOOKUP_LAST


/*
** dlookup(st,name,ino): 
**    lookup the simple name "name" within the directory described by "ino"
**    returns the inode associated with "name" if successful, or EXT2_BAD_INO
**    on failure. 
*/
static INLINE uint32_t dlookup(ext2fs_st    *st, 
			       char         *name, 
			       struct inode *ino)
{
    uint32_t            res;
    uint32_t            i, nb, offset;
    char               *dptr, *data;
    struct buffer_head *buf;
    uint32_t            blockno;
    uint32_t            start;

    /* "i_blocks" gives the number of blocks in 512 byte chunks. Hence
       we need to convert it to a number of _logical_ blocks as below. */
    nb = LOGICAL_BLKS(st, ino->i_blocks);
    /* XXX what is that value anyway? Physical blocks allocated to the file?
       Length rounded up to multiple of 512? Hmm. */

#ifdef DLOOKUP_LAST
    /* PRB: we can start directory lookup where we left off */
    offset = start = ino->i_dcur;
#else
    offset = start = 0; 
#endif

again:
    for(i = 0; i < nb && offset < ino->i_size; 
	i++, offset += st->fsc.block_size) {
	TRC(printf("%d", i));
	blockno = ext2_bmap(st, ino, i);
	TRC(printf("->%d\n", blockno));
	buf = bread(st, blockno, False);

#ifdef DLOOKUP_LAST
	/* If recording where we are, store dcur now */ 
	ino->i_dcur = i;
#endif

	
	if (!buf) {
	    ext2_error(st, "dlookup", "hole in directory\n");
	    continue;
	}
	/* scan through the dir for 'comp' */
	dptr = data = buf->b_data;
	while(dptr < (data + st->fsc.block_size)) {
	    if((res = dname_eq((ext2_dir_t *)dptr, name)) != EXT2_BAD_INO) {
		TRC(printf("Found '%s' (inode is %d)\n", name, res));
		brelse(buf);
		return res;
	    }
	    dptr += ((ext2_dir_t *)dptr)->rec_len;
	}

	brelse(buf);
    }
#ifdef DLOOKUP_LAST
    /* If we started in the middle of the directory, go back to start */
    if (start) {
	nb = start; start = 0; goto again;
    }
#endif

    return EXT2_BAD_INO;
}

static FSTypes_Name readlink(ext2fs_st *st, struct inode *ino)
{
    uint32_t            block;
    struct buffer_head *buf;
    FSTypes_Name        contents;

    if (!S_ISLNK(ino->i_mode)) return NULL;
    /* If number of blocks is 0 then symlink data is stored in
       inode itself */
    if (ino->i_blocks==0) {
	contents = Heap$Malloc(st->heap, ino->i_size+1);
	contents[ino->i_size]=0;
	memcpy(contents, &ino->u.ext2_i.i_data[0], ino->i_size);
    } else {
	/* Assume symlink data fits entirely within first block (may even
	   be true) */
	block = ext2_bmap(st, ino, 0);
	TRC(printf("readlink: ext2_bmap returns %d\n",block));
	buf   = bread(st, block, False);
	if (!buf) {
	    ext2_error(st, "readlink", "no data for link, inode %d\n",
		       ino->i_ino);
	    return NULL;
	}
	contents              = Heap$Malloc(st->heap, ino->i_size+1);
	contents[ino->i_size] = 0;
	memcpy(contents, buf->b_data, ino->i_size);
	brelse(buf);
    }

    TRC(printf("readlink: contents=\"%s\"\n",contents));

    return contents;
}

static FSTypes_RC Lookup_m (
    Ext2_cl	       *self,
    Ext2_Handle	        dir	/* IN */,
    FSTypes_Name	path	/* IN */,
    bool_t              follow  /* IN */
    /* RETURNS */,
    Ext2_Handle	       *inode )
{
    Client_st    *cur                = self->st;
    ext2fs_st    *st                 = cur->fs;
    struct inode *ino                = NULL;
    struct inode *lu;
    char         *comp, buf[256];
    char         *fullpath, *save, *join;
    uint32_t      symlinks_remaining = MAX_SYMLINK_FOLLOW;
    Ext2_Handle   rval;
    uint32_t      res;
    FSTypes_RC    rc;
    
    TRC(printf(__FUNCTION__": entered, path=\"%s\"\n", path));

    /* Check that the handle is a valid directory inode */
    if (!check_inode_handle(cur,dir)) return FSTypes_RC_FAILURE;

    TRC(printf("."));

    if (!S_ISDIR(cur->handles[dir].ino->i_mode)) return FSTypes_RC_FAILURE;

    TRC(printf("."));

    /* Allocate a new handle - this is for the return value */
    rval=gethandle(cur);
    if (rval==-1) return FSTypes_RC_FAILURE;

    TRC(printf("."));

    /* We could probably mess with path if we liked; we will always be
       called through stubs. Let's be careful, anyway. */
    save = fullpath = strduph(path, st->heap);

    TRC(printf("."));

    /* Duplicate the starting inode */
    if (fullpath[0]=='/') {
	/* We start at the root */
	ino = get_inode(st, EXT2_ROOT_INO);
	fullpath++;
    } else {
	ino  = get_inode(st, cur->handles[dir].ino->i_ino);
    }

    rc   = FSTypes_RC_FAILURE;

#ifdef CONFIG_LOGGING
    { 
	/* XXX PRB: LOGGING HACK */
	USD_Extent e;
	e.base = 3;
	e.len  = cur->handles[dir].ino->i_ino;
	USDCtl$Request(st->disk.usdctl, &e, FileIO_Op_Sync, NULL);
    }
#endif

    TRC(printf("."));

    while(True) {
	TRC(printf("lookup: fullpath=\"%s\"\n",fullpath));
	if(!strlen(fullpath)) {
	    /* If there's nothing left here then we are definitely
	       finished; following of final symlinks is done
	       later on */
	    TRC(printf("fullpath is empty => returning success.\n"));
	    cur->handles[rval].ino = ino;
	    rc     = FSTypes_RC_OK;
	    break;
	}

	/* At this point there's something left on the path. "ino"
	   must be a directory; if is isn't we fail */

	if(!S_ISDIR(ino->i_mode)) {
	    release_inode(st, ino);
	    break;
	}


	/* At this point we know we're looking up a name in a directory */
	comp = (char *)buf;
	while(*fullpath && (*fullpath != '/')) 
	    *comp++ = *fullpath++;
	if (*fullpath) fullpath++;
	*comp = 0; comp = (char *)buf;

	TRC(printf("Ext2$Lookup: comp is \"%s\", "
		   "remaining fullpath is \"%s\"\n", comp, fullpath));
	if((res = dlookup(st, comp, ino)) == EXT2_BAD_INO) {
	    TRC(printf("lookup of \"%s\" failed => returning failure.\n",
		       comp));
	    break;
	}
	TRC(printf("."));

	    /* We fetch the inode we've just looked up so that we can check
	       whether it's a symlink */
	lu=get_inode(st, res);
	if (!lu) {
	    TRC(printf("lookup: failed to get inode %d\n",res));
	    release_inode(st, ino);
	    break;
	}

	/* If we're on a symlink then we consider following it;
	       if we still have work to do we definitely follow it, otherwise
	       we check the 'follow' parameter */
	if (S_ISLNK(lu->i_mode) && ((!*fullpath) || follow)) {
	    if (symlinks_remaining==0) {
		TRC(printf("lookup: too many symlinks. Aborting.\n"));
		release_inode(st, lu);
		release_inode(st, ino);
		break;
	    }
	    TRC(printf("lookup: following symlink\n"));
	    
	    join = readlink(st,lu);
	    /* We want to stick the symlink text in front of fullpath. */
	    comp = Heap$Malloc(st->heap,
			       strlen(join)+1+strlen(fullpath)+1);
	    strcpy(comp,join);
	    strcat(comp,"/");
	    strcat(comp,fullpath);
	    FREE(save);
	    save = fullpath = comp;
	    FREE(join);
	    /* Whew. Was that it? */
	    symlinks_remaining--;
	    release_inode(st, lu); /* We no longer need the symlink
				      inode */
	} else {
	    /* Release current inode, hop to next */
	    release_inode(st, ino);
	    ino=lu;
	}
	TRC(printf("|"));
	/* XXX do access checks here, abort if necessary */
    }
	    
    FREE(save);

#ifdef CONFIG_LOGGING
    { 
	/* XXX PRB: LOGGING HACK */
	USD_Extent e;
	e.base = 3;
	e.len  = cur->handles[rval].ino->i_ino;
	USDCtl$Request(st->disk.usdctl, &e, FileIO_Op_Sync, NULL);
    }
#endif

    if (rc==FSTypes_RC_OK)
	*inode = rval;
    else
	cur->handles[rval].used = False;

    TRC(printf("*"));
    TRC(PAUSE(MILLISECS(100)));
    return rc; 
}

static FSTypes_RC Link_m (
    Ext2_cl	       *self,
    Ext2_Handle		dir	/* IN */,
    FSTypes_Name	name	/* IN */,
    Ext2_Handle		i	/* IN */ )
{
    return FSTypes_RC_FAILURE;
}

static FSTypes_RC Unlink_m (
    Ext2_cl	       *self,
    Ext2_Handle		dir	/* IN */,
    FSTypes_Name	name	/* IN */ )
{
    return FSTypes_RC_FAILURE;
}

static FSTypes_RC MkDir_m (
    Ext2_cl	       *self,
    Ext2_Handle		dir	/* IN */,
    FSTypes_Name	path	/* IN */ )
{
    return FSTypes_RC_FAILURE;
}

static FSTypes_RC RmDir_m (
    Ext2_cl	       *self,
    Ext2_Handle		dir	/* IN */,
    FSTypes_Name	path	/* IN */ )
{
    return FSTypes_RC_FAILURE;
}

static FSTypes_RC Rename_m (
    Ext2_cl	       *self,
    Ext2_Handle		fromdir	        /* IN */,
    FSTypes_Name	fromname        /* IN */,
    Ext2_Handle		todir           /* IN */,
    FSTypes_Name	toname	        /* IN */ )
{
    return FSTypes_RC_FAILURE;
}

static FSTypes_RC ReadDir_m (
    Ext2_cl	       *self,
    Ext2_Handle		dir	/* IN */,
    /* RETURNS */
    FSTypes_DirentSeq **entries )
{
    Client_st *cur = self->st;
    ext2fs_st *st  = cur->fs;

    uint32_t            nblocks;           /* Number of logical blocks
					    * in directory */
    uint32_t            bmap_block, block; /* Absolute and relative
					    * block numbers */
    uint8_t            *dirent, *data;
    ext2_dir_t         *d;
    struct buffer_head *buf;
    FSTypes_DirentSeq  *list;
    FSTypes_Dirent      ent;

    struct inode       *ino = NULL;

    /* XXX PRB: Hopefully Unused (now read directories in client) */
    ntsc_dbgstop();

    /* No matter what happens, the sequence that we return has to be valid
       or else the stubs will crash. */
    list     = SEQ_NEW(FSTypes_DirentSeq, 0, st->heap);
    *entries = list;

    /* Check handle */
    if (!check_inode_handle(cur, dir)) {
	printf("ReadDir: bad handle\n");
	return FSTypes_RC_FAILURE;
    }

    ino = cur->handles[dir].ino;

    if(!S_ISDIR(ino->i_mode)) {
	printf("OpenDir: inode is not a directory\n");
	return FSTypes_RC_FAILURE;
    }

    /* XXX still not sure I want to do this */
    nblocks = LOGICAL_BLKS(st, ino->i_blocks);

    TRC(printf("handle %d size is %d, blocks %d (logical %d)\n",
	       dir, ino->i_size, ino->i_blocks, nblocks));

    /* The client handle stores the block number that we should read _next_.
       If this isn't valid, just return the empty sequence successfully. */
    block=cur->handles[dir].dirblock++;
    if (block>=nblocks) return FSTypes_RC_OK;

    TRC(printf("ext2fs: ReadDir_m: block %d\n",block));
    bmap_block=ext2_bmap(st, ino, block);
    TRC(printf("ext2fs: ReadDir_m: fsblock %d\n",bmap_block));
    buf=bread(st, bmap_block, False);
    TRC(printf("ext2fs: ReadDir_m: back from fetching block\n"));
    if (!buf) {
	ext2_error(st,"ReadDir_m", "hole in directory at block %d\n",
		   block);
	return FSTypes_RC_OK;
    }
    dirent = data = buf->b_data;
    while (dirent<(data+st->fsc.block_size)) {
	d                     = (ext2_dir_t *)dirent;
	ent.ino               = d->inode;
	ent.name              = Heap$Malloc(st->heap, d->name_len+1);
	ent.name[d->name_len] = 0;
	strncpy(ent.name, d->name, d->name_len);
	SEQ_ADDH(list, ent);
	dirent += ((ext2_dir_t *)dirent)->rec_len;
    }
    brelse(buf);

    TRC(printf("Finished dumping directory\n"));

    return FSTypes_RC_OK;
}

static FSTypes_RC Symlink_m (
    Ext2_cl	       *self,
    Ext2_Handle		dir	/* IN */,
    FSTypes_Name	name	/* IN */,
    FSTypes_Name	path	/* IN */ )
{
/*    Client_st *cur = self->st;
    ext2fs_st *st = cur->fs; */

    return FSTypes_RC_FAILURE;
}

static FSTypes_RC Readlink_m (
    Ext2_cl 	       *self,
    Ext2_Handle		link	/* IN */,
    /* RETURNS */
    FSTypes_Name       *contents )
{
    Client_st    *cur = self->st;
    ext2fs_st    *st  = cur->fs;
    struct inode *ino;

    if (!check_inode_handle(cur, link)) return FSTypes_RC_FAILURE;

    ino = cur->handles[link].ino;

    /* Check inode type */
    if(!S_ISLNK(ino->i_mode)) {
	printf("Readlink: inode is not a symlink\n");
	return FSTypes_RC_FAILURE;
    }

    *contents=readlink(st,ino);

    if (!*contents) {
	*contents = strdup("");
	return FSTypes_RC_FAILURE;
    }

    return FSTypes_RC_OK;
}

static FSTypes_RC Unmount_m(Ext2_cl *self, bool_t force)
{
    return FSTypes_RC_DENIED;
}
