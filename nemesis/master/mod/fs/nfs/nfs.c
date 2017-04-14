/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/fs/nfs
**
** FUNCTIONAL DESCRIPTION:
** 
**      Middl wrapper around nfs operations over sunrpc over udp.
** 
** ENVIRONMENT: 
**
**      User space
** 
*/

#include <nemesis.h>
#include <contexts.h>
#include <exceptions.h>
#include <ecs.h>
#include <links.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>

#include <nfs_st.h>

#include <NFS.ih>

#define DB(_x) _x

/* define this if you want large overview tracing */
#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

/* define this if you want to see all data operations */
#ifdef DEBUG_MORE
#define DATATRC(_x) _x
#define DATATRACING
#else
#define DATATRC(_x)
#define fmt_fh(_x, _y)
#endif


static const struct timeval CTRL_FAIL_TIMEOUT  = { 
    (NFSCTRL_FAIL_TIMEOUT_MS / 1000), 
    ((NFSCTRL_FAIL_TIMEOUT_MS % 1000) * 1000) 
};

static const struct timeval DATA_FAIL_TIMEOUT  = { 
    (NFSDATA_FAIL_TIMEOUT_MS / 1000), 
    ((NFSDATA_FAIL_TIMEOUT_MS % 1000) * 1000) 
}; 

static const struct timeval CTRL_RETRY_TIMEOUT = { 
    (NFSCTRL_RETRY_TIMEOUT_MS / 1000), 
    ((NFSCTRL_RETRY_TIMEOUT_MS % 1000) * 1000) 
};

static const struct timeval DATA_RETRY_TIMEOUT = { 
    (NFSDATA_RETRY_TIMEOUT_MS / 1000), 
    ((NFSDATA_RETRY_TIMEOUT_MS % 1000) * 1000) 
};




/* NFS interface */

static  NFS_Free_fn             NFS_Free_m;
static  NFS_GetPath_fn          NFS_GetPath_m;
static  NFS_Root_fn             NFS_Root_m;
static  NFS_InodeType_fn        NFS_InodeType_m;
static  NFS_BlockSize_fn        NFS_BlockSize_m;
static  NFS_Size_fn             NFS_Size_m;
static  NFS_NLinks_fn           NFS_NLinks_m;
static  NFS_ATime_fn            NFS_ATime_m;
static  NFS_MTime_fn            NFS_MTime_m;
static  NFS_CTime_fn            NFS_CTime_m;
static  NFS_Open_fn             NFS_Open_m;
static  NFS_Create_fn           NFS_Create_m;
static  NFS_Delete_fn           NFS_Delete_m;
static  NFS_FetchBlock_fn       NFS_FetchBlock_m;
static  NFS_PutBlock_fn         NFS_PutBlock_m;
static  NFS_SetLength_fn        NFS_SetLength_m;
static  NFS_Lookup_fn           NFS_Lookup_m;
static  NFS_Link_fn             NFS_Link_m;
static  NFS_MkDir_fn            NFS_MkDir_m;
static  NFS_RmDir_fn            NFS_RmDir_m;
static  NFS_Rename_fn           NFS_Rename_m;
static  NFS_ReadDir_fn          NFS_ReadDir_m;
static  NFS_Symlink_fn          NFS_Symlink_m;
static  NFS_Readlink_fn         NFS_Readlink_m;
static  NFS_Stat_fn             NFS_Stat_m;

static  NFS_op  nfs_ms = {
    NFS_Free_m,
    NFS_GetPath_m,
    NFS_Root_m,
    NFS_InodeType_m,
    NFS_BlockSize_m,
    NFS_Size_m,
    NFS_NLinks_m,
    NFS_ATime_m,
    NFS_MTime_m,
    NFS_CTime_m,
    NFS_Open_m,
    NFS_Create_m,
    NFS_Delete_m,
    NFS_FetchBlock_m,
    NFS_PutBlock_m,
    NFS_SetLength_m,
    NFS_Lookup_m,
    NFS_Link_m,
    NFS_MkDir_m,
    NFS_RmDir_m,
    NFS_Rename_m,
    NFS_ReadDir_m,
    NFS_Symlink_m,
    NFS_Readlink_m,
    NFS_Stat_m,
};

/* An array mapping some errnos to messages.
 * These values come from /usr/include/asm/errno.h from a linux box
 * There are probably way too many of these, but hopefully the common
 * ones will be included. */
static const char * const errno_tbl[] =
{
    "Success (can't happen!)",		/*  0 */
    "Operation not permitted",		/*  1 */
    "No such file or directory",	/*  2 */
    "No such process",			/*  3 */
    "Interrupted system call",		/*  4 */
    "I/O error",			/*  5 */
    "No such device or address",	/*  6 */
    "Arg list too long",		/*  7 */
    "Exec format error",		/*  8 */
    "Bad file number",			/*  9 */
    "No child processes",		/* 10 */
    "Try again",			/* 11 */
    "Out of memory",			/* 12 */
    "Permission denied",		/* 13 */
    "Bad address",			/* 14 */
    "Block device required",		/* 15 */
    "Device or resource busy",		/* 16 */
    "File exists",			/* 17 */
    "Cross-device link",		/* 18 */
    "No such device",			/* 19 */
    "Not a directory",			/* 20 */
    "Is a directory",			/* 21 */
    "Invalid argument",			/* 22 */
    "File table overflow",		/* 23 */
    "Too many open files",		/* 24 */
    "Not a typewriter",			/* 25 */
    "Text file busy",			/* 26 */
    "File too large",			/* 27 */
    "No space left on device",		/* 28 */
    "Illegal seek",			/* 29 */
    "Read-only file system",		/* 30 */
    "Too many links",			/* 31 */
    "Broken pipe",			/* 32 */
    "Math argument out of domain",	/* 33 */
    "Math result out of range",		/* 34 */
    "Resource deadlock would occur",	/* 35 */
    "File name too long",		/* 36 */
    "No record locks available",	/* 37 */
    "Function not implemented",		/* 38 */
    "Directory not empty",		/* 39 */
    "Too many symbolic links encountered",	/* 40 */
    "Operation would block"		/* 41 */
};

/* Internal NFS methods */
static bool_t nfs_lookup(nfs_st *st, 
			 nfs_fh *root, char *refresh_path,
			 char *filename, int depth,
			 nfs_fh *fh, fattr *attributes);
static char *nfs_readlink(nfs_st *st, nfs_fh *fh, char *refresh_path,
			  const char *filename);
static bool_t nfs_list(nfs_st *st, nfs_fh *fh, char *refresh_path,
		       FSTypes_DirentSeq *names);
static bool_t nfs_bread(nfs_st *nfs, handle_st *nf, unsigned bnum,addr_t addr);
static bool_t nfs_write(nfs_st *nfs, handle_st *nf, unsigned bnum,addr_t addr);
static FSTypes_RC nfs_create(nfs_st *st, nfs_fh *dir, char *refresh_path,
			     char   *filename, sattr  *mode, 
			     nfs_fh *fh, fattr  *attributes);
static bool_t nfs_delete(nfs_st *st, nfs_fh *dir, char *refresh_path,
			 char *filename, u_int op);
static bool_t nfs_link(nfs_st *st, nfs_fh *dir, char *refresh_path,
		       char *name, nfs_fh *from, char *from_refresh_path);
static bool_t nfs_mkdir(nfs_st *st, nfs_fh *dir, char *refresh_path,
			char *name, sattr *mode);
static bool_t nfs_rename(nfs_st *st, nfs_fh *fromdir, char *from_refresh_path,
			 char *from,
			 nfs_fh *todir, char *to_refresh_path,
			 char *to);
static bool_t nfs_symlink(nfs_st *st, nfs_fh *dir, char *refresh_path,
			  char *name, char *path, sattr *attr);
static char *join(const char *s1, const char *s2);
static bool_t refresh_fh(nfs_st *st, nfs_fh *fh, char *refresh_path,
			 const char *opname);





NFS_clp nfs_init(struct sockaddr_in *svr, nfs_fh *rootfh, 
		 uint32_t bsize, bool_t ro, bool_t debug, 
		 char *server, char *mountpoint) 
{
    int     sock = RPC_ANYSOCK;
    nfs_st *st; 


    st = Heap$Calloc(Pvs(heap), 1, sizeof(*st)); 
    
    memcpy(&st->rootfh, rootfh, sizeof(nfs_fh));

    st->blocksize  = bsize;
    st->readonly   = ro;
    st->debug      = debug;
    st->datamode   = False;

    st->server     = strdup(server); 
    st->mountpoint = strdup(mountpoint);

    /* XXX We assume handles are inited enough by our calloc */

    MU_INIT(&st->call_lock); 

    st->heap   = Pvs(heap); 

    st->client = clntudp_create(svr, NFS_PROGRAM, NFS_VERSION, 
				CTRL_RETRY_TIMEOUT, &sock);

    if (st->client == (CLIENT *)NULL) {
	DB(eprintf("nfs_init: failed to create rpc socket.\n"));
	close(sock);
	Heap$Free(Pvs(heap), st);
	return (NFS_cl *)NULL;
    }

    CL_INIT(st->cl, &nfs_ms, st);
    return &st->cl;
}


/* Utility functions */

static NFS_Handle gethandle(nfs_st *st)
{
    uint32_t i;
    uint32_t filehandle = -1;

    /* Before proceeding, find a free file slot. If there isn't one, fail */

    /* Probably need a mutex here if we ever have more than one thread in
       the entry. */
    for(i=0; i < MAX_HANDLES; i++) {
	if (!st->handles[i].used) {
	    filehandle = i;
	    break;
	}
    }

    if(filehandle != -1) 
    {
	st->handles[filehandle].used = True;
	
	/* Clear fields */
	st->handles[filehandle].open = False;
	st->handles[filehandle].path = NULL;
    }

    return filehandle;
}

static bool_t check_handle(nfs_st *st, NFS_Handle handle)
{
    if (handle >= MAX_HANDLES) return False;

    if (!st->handles[handle].used) return False;

    return True;
}

static bool_t check_inode_handle(nfs_st *st, NFS_Handle handle)
{
    if (!check_handle(st,handle)) return False;

    if (st->handles[handle].open) return False;

    return True;
}

static bool_t check_file_handle(nfs_st *st, NFS_Handle handle)
{
    if (!check_handle(st,handle)) return False;

    if (!st->handles[handle].open) return False;

    return True;
}

static bool_t check_dir(nfs_st *st, NFS_Handle handle)
{
    if (!check_inode_handle(st,handle)) return False;
    return F_ISDIR(st->handles[handle].attributes.type);
}


/* order here is the same as in nfs_prot.x ftype enum */
static const char * const filetype[] = {
    "non-file",
    "REG",
    "DIR",
    "BLK",
    "CHR",
    "LNK",
    "SOCK",
    "bad",	/* unused */
    "FIFO"
};

static void UNUSED dump_attr(fattr *a)
{
    const char *t;
    char        buf[8];

    if (a->type >=0 && a->type < (sizeof(filetype)/sizeof(char *))) {
	t = filetype[a->type];
    } else {
	sprintf(buf, "(%d)", a->type);
	t = buf;
    }

    printf("%s: mode=%o nlink=%d ids=%d/%d sz=%d blksz=%d rdev=%d blocks=%d "
	   "fsid=%d fileid=%d\n",
	   t, a->mode, a->nlink, a->uid, a->gid, a->size, a->blocksize,
	   a->rdev, a->blocks, a->fsid, a->fileid);
}


/*------------------------------------------------ NFS Entry Points ----*/


static bool_t NFS_Free_m (
    NFS_cl         *self,
    NFS_Handle      handle  /* IN */ )
{
    nfs_st  *st = self->st;

    if (!check_handle(st, handle)) return False;

    st->handles[handle].used=False;
    if (st->handles[handle].path) FREE(st->handles[handle].path);
    st->handles[handle].path = NULL;
    /* Don't need to do anything else? */

    return True;
}

static string_t NFS_GetPath_m (
    NFS_cl       *self,
    NFS_Handle    handle /* IN */ )
{
    nfs_st  *st = self->st;

    if (!check_dir(st,handle)) return "invalid handle";

    return st->handles[handle].path;
}

static FSTypes_RC NFS_Root_m (
    NFS_cl          *self
    /* RETURNS */,
    NFS_Handle      *root )
{
    nfs_st     *st = self->st;
    NFS_Handle  h;

    TRC(printf("NFS: Root called\n"));

    h = gethandle(st);
    if (h == -1) return FSTypes_RC_FAILURE; /* Out of handles */

    /* Copy root filehandle into struct */
    memcpy(&st->handles[h].fh, &st->rootfh, sizeof(st->rootfh));
    st->handles[h].path = strdup("/");   /* XXX bit simplisitc, but we 
					    have no mount points */

    *root = h;

    /* XXX should do something to fix up attributes */
    memset(&st->handles[h].attributes, 0, sizeof(fattr));
    st->handles[h].attributes.type = NFDIR;
    
    return FSTypes_RC_OK;
}

static FSTypes_InodeType NFS_InodeType_m (
    NFS_cl         *self,
    NFS_Handle      inode   /* IN */ )
{
    nfs_st            *st   = self->st;
    FSTypes_InodeType  type = FSTypes_InodeType_Special;
    ftype              it;

    if (!check_inode_handle(st, inode)) return type; /* Failed */

    it = st->handles[inode].attributes.type;

    if (F_ISREG(it))       type = FSTypes_InodeType_File;
    else if (F_ISDIR(it))  type = FSTypes_InodeType_Dir;
    else if (F_ISLINK(it)) type = FSTypes_InodeType_Link;

    return type;
}

static uint32_t NFS_BlockSize_m (
    NFS_cl     *self,
    NFS_Handle  inode )
{
    nfs_st *st = self->st;

    if (!check_handle(st, inode)) return 0;

    /* Read from attributes */
    return BLOCKSIZE(st->handles[inode].attributes.blocksize);
}

static FileIO_Size NFS_Size_m (
    NFS_cl         *self,
    NFS_Handle      inode   /* IN */ )
{
    nfs_st *st = self->st;

    if (!check_handle(st, inode)) return 0; /* Failed */

    return st->handles[inode].attributes.size;
}

static FSTypes_NLinks NFS_NLinks_m (
    NFS_cl         *self,
    NFS_Handle      inode   /* IN */ )
{
    nfs_st *st = self->st;
    
    if (!check_inode_handle(st, inode)) return 0; /* Failed */

    return st->handles[inode].attributes.nlink;
}

static FSTypes_Time NFS_ATime_m (
    NFS_cl         *self,
    NFS_Handle      inode   /* IN */ )
{
    nfs_st *st = self->st;

    if (!check_handle(st, inode)) return 0;

    return st->handles[inode].attributes.atime.seconds;
}

static FSTypes_Time NFS_MTime_m (
    NFS_cl         *self,
    NFS_Handle      inode   /* IN */ )
{
    nfs_st *st = self->st;

    if (!check_handle(st, inode)) return 0;

    return st->handles[inode].attributes.mtime.seconds;
}

static FSTypes_Time NFS_CTime_m (
    NFS_cl         *self,
    NFS_Handle      inode   /* IN */ )
{
    nfs_st *st = self->st;

    if (!check_handle(st, inode)) return 0;

    return st->handles[inode].attributes.ctime.seconds;
}

static FSTypes_RC NFS_Open_m (
    NFS_cl         *self,
    NFS_Handle      file    /* IN */,
    FSTypes_Mode    mode    /* IN */,
    FSTypes_Options options /* IN */
    /* RETURNS */,
    NFS_Handle     *handle )
{
    nfs_st     *st = self->st;
    NFS_Handle  h;

    /* Check that the handle is valid */
    if (!check_inode_handle(st, file)) return FSTypes_RC_FAILURE;

    /* Check that it's a file */
    if (!F_ISREG(st->handles[file].attributes.type))
	return FSTypes_RC_FAILURE;

    /* Perform access checks, etc. */
    if (mode==FSTypes_Mode_Write && st->readonly) {
	TRC(printf("NFS: Open: client tried to open file RW on RO fs\n"));
	return FSTypes_RC_DENIED;
    }

    /* Allocate new handle for open file */
    if((h = gethandle(st)) == -1)
	return FSTypes_RC_FAILURE;

    /* Copy filehandle and path */
    memcpy(&st->handles[h].fh, &st->handles[file].fh, sizeof(nfs_fh));
    st->handles[h].path = strdup(st->handles[file].path);
    /* ...and attributes */
    memcpy(&st->handles[h].attributes, &st->handles[file].attributes,
	   sizeof(fattr));

    /* Flag the new handle as open */
    st->handles[h].open     = True;
    st->handles[h].writable = (mode == FSTypes_Mode_Write);

    *handle = h;

    return FSTypes_RC_OK;
}

static FSTypes_RC NFS_Create_m (
    NFS_cl         *self,
    NFS_Handle      dir,
    FSTypes_Name    name,
    FSTypes_Options options,
    /* RETURNS */
    NFS_Handle     *handle )
{
    nfs_st    *st = self->st;
    NFS_Handle h;
    FSTypes_RC res; 
    sattr      na;

    /* Check that dir is a directory */
    if (!check_dir(st, dir)) return FSTypes_RC_FAILURE;

    /* Can't create on a readonly filesystem */
    if (st->readonly) {
	TRC(printf("NFS: Create: readonly filesystem\n"));
	return FSTypes_RC_DENIED;
    }

    /* First allocate the handle */
    if((h = gethandle(st)) == -1)
	return FSTypes_RC_FAILURE;

    TRC(printf("NFS: Create: going for it\n"));
    na.mode = 0666;

    /* SMH: we use root.system and expect that the server will 
       squash it to whatever. This needs to be replaced with some
       sensible uid/gid once we have a proper 'posix' personality */
    na.uid  = 0; 
    na.gid  = 0; 

    na.size = 0;
    na.atime.seconds  = 0;  
    na.atime.useconds = 0;  
    na.mtime.seconds  = 0;  
    na.mtime.useconds = 0;  

    res = nfs_create(st, &st->handles[dir].fh, st->handles[dir].path,
		    name, &na, &st->handles[h].fh, &st->handles[h].attributes);

    if (res != FSTypes_RC_OK) {
	st->handles[h].used = False;
	return res; 
    }

    /* Update path for new handle */
    st->handles[h].path = join(st->handles[dir].path,name);

    *handle=h;

    return res; 
}

static FSTypes_RC NFS_Delete_m (
    NFS_cl       *self,
    NFS_Handle    dir,
    FSTypes_Name  name )
{
    nfs_st *st = self->st;
    bool_t  ok;

    /* Check dir is a directory */
    if (!check_dir(st, dir)) return FSTypes_RC_FAILURE;

    /* Can't do it on a readonly filesystem */
    if (st->readonly) {
	TRC(printf("NFS: Delete: readonly filesystem\n"));
	return FSTypes_RC_DENIED;
    }

    /* Give it a whirl */
    ok=nfs_delete(st, &st->handles[dir].fh, st->handles[dir].path,
		  name,NFSPROC_REMOVE);

    return ok?FSTypes_RC_OK:FSTypes_RC_FAILURE;
}

static FSTypes_RC NFS_FetchBlock_m (
    NFS_cl         *self,
    NFS_Handle      handle          /* IN */,
    FileIO_Block    fileblock       /* IN */,
    addr_t          location        /* IN */ )
{
    nfs_st *st = self->st;
    bool_t  ok;

    /* Check handle */
    if (!check_file_handle(st, handle)) return FSTypes_RC_FAILURE;

    /* Check mode */

    /* Ok, it's open so let's do a bread */
    ok = nfs_bread(st, &st->handles[handle], fileblock, location);

    return ok?FSTypes_RC_OK:FSTypes_RC_FAILURE;
}

static FSTypes_RC NFS_PutBlock_m (
    NFS_cl         *self,
    NFS_Handle      handle          /* IN */,
    FileIO_Block    fileblock       /* IN */,
    addr_t          location        /* IN */ )
{
    nfs_st *st = self->st;
    bool_t  ok;

    /* Check handle */
    if (!check_file_handle(st, handle)) return FSTypes_RC_FAILURE;

    /* Check mode */
    if (!st->handles[handle].writable) {
	TRC(printf("NFS: PutBlock: attempt to write to file opened for "
		   "reading only\n"));
	return FSTypes_RC_DENIED;
    }

    /* Do the write */
    ok = nfs_write(st, &st->handles[handle], fileblock, location);

    return ok?FSTypes_RC_OK:FSTypes_RC_FAILURE;
}

static FSTypes_RC NFS_SetLength_m (
    NFS_cl         *self,
    NFS_Handle      handle  /* IN */,
    FileIO_Size     length  /* IN */ )
{
    nfs_st *st = self->st;

    if (!check_file_handle(st, handle)) return FSTypes_RC_FAILURE;

    /* Check that it's writable */
    if (!st->handles[handle].writable) {
	TRC(printf("NFS: SetLength: attempt to set length of file "
		   "opened readonly\n"));
	return FSTypes_RC_DENIED;
    }

    /* Set length */
    st->handles[handle].attributes.size = length;
    /* XXX We should actually do the truncation if necessary */

    return FSTypes_RC_OK;
}

static FSTypes_RC NFS_Lookup_m (
    NFS_cl         *self,
    NFS_Handle      dir     /* IN */,
    FSTypes_Name    path    /* IN */,
    bool_t          follow  /* IN */
    /* RETURNS */,
    NFS_Handle     *inode )
{
    nfs_st    *st = self->st;
    bool_t     ok;
    nfs_fh     nfh;
    fattr      attrib;
    NFS_Handle h;

    /* Ok, this is the interesting one. First check that dir is a directory */
    if (!check_dir(st, dir)) {
	TRC(printf("NFS: Lookup: %d is not a valid directory handle\n",dir));
	return FSTypes_RC_FAILURE;
    }

    /* Try to do the lookup */
    ok = nfs_lookup(st, &st->handles[dir].fh, st->handles[dir].path,
		    path, 0, &nfh, &attrib);

    if (!ok) {
	TRC(printf("NFS: Lookup: nfs_lookup not ok\n"));
	return FSTypes_RC_FAILURE;
    }

    if((h = gethandle(st)) == -1)
	return FSTypes_RC_FAILURE;

    memcpy(&st->handles[h].fh, &nfh, sizeof(nfh));
    st->handles[h].attributes = attrib;

    st->handles[h].path     = join(st->handles[dir].path, path);
    st->handles[h].dir_read = False;
    if (!st->handles[h].path)
    {
	/* It's not working */
	st->handles[h].used = False;
	TRC(printf("NFS: Lookup: new path is null\n"));
	return FSTypes_RC_FAILURE;
    }

    *inode = h;

    return FSTypes_RC_OK;
}

static FSTypes_RC NFS_Link_m (
    NFS_cl         *self,
    NFS_Handle      dir     /* IN */,
    FSTypes_Name    name    /* IN */,
    NFS_Handle      i       /* IN */ )
{
    nfs_st *st = self->st;
    bool_t ok;

    if (!check_dir(st,dir)) return FSTypes_RC_FAILURE;

    if (!check_inode_handle(st,i)) return FSTypes_RC_FAILURE;

    if (st->readonly) {
	TRC(printf("NFS: Link: readonly filesystem\n"));
	return FSTypes_RC_DENIED;
    }

    ok=nfs_link(st, &st->handles[dir].fh, st->handles[dir].path,
		name, &st->handles[i].fh, st->handles[i].path);

    return ok?FSTypes_RC_OK:FSTypes_RC_FAILURE;
}

static FSTypes_RC NFS_MkDir_m (
    NFS_cl         *self,
    NFS_Handle      dir     /* IN */,
    FSTypes_Name    path    /* IN */ )
{
    nfs_st *st = self->st;
    bool_t  ok;
    sattr   na;

    if (!check_dir(st,dir)) return FSTypes_RC_FAILURE;

    if (st->readonly) {
	TRC(printf("NFS: MkDir: readonly filesystem\n"));
	return FSTypes_RC_DENIED;
    }

    na.mode = 0777;

    na.uid  = 0x0f0;       /* These two are set by the packet filter. */
    na.gid  = 0xf00; 

    na.size = 0;
    na.atime.seconds  = 0; /* XXX These should not be hard-coded */
    na.atime.useconds = 0;
    na.mtime.seconds  = 0;
    na.mtime.useconds = 0;
    ok = nfs_mkdir(st, &st->handles[dir].fh, st->handles[dir].path,
		   path, &na);
    
    return ok?FSTypes_RC_OK:FSTypes_RC_FAILURE;
}

static FSTypes_RC NFS_RmDir_m (
    NFS_cl         *self,
    NFS_Handle      dir     /* IN */,
    FSTypes_Name    path    /* IN */ )
{
    nfs_st *st = self->st;
    bool_t  ok;

    if (!check_dir(st,dir)) return FSTypes_RC_FAILURE;

    if (st->readonly) {
	TRC(printf("NFS: RmDir: readonly filesystem\n"));
	return FSTypes_RC_DENIED;
    }

    ok = nfs_delete(st, &st->handles[dir].fh, st->handles[dir].path,
		    path,NFSPROC_RMDIR);

    return ok?FSTypes_RC_OK:FSTypes_RC_FAILURE;
}

static FSTypes_RC NFS_Rename_m (
    NFS_cl         *self,
    NFS_Handle      fromdir  /* IN */,
    FSTypes_Name    fromname /* IN */,
    NFS_Handle      todir    /* IN */,
    FSTypes_Name    toname   /* IN */ )
{
    nfs_st *st = self->st;
    bool_t  ok;

    if (!check_dir(st,fromdir)) return FSTypes_RC_FAILURE;
    if (!check_dir(st,todir)) return FSTypes_RC_FAILURE;

    if (st->readonly) {
	TRC(printf("NFS: Rename: readonly filesystem\n"));
	return FSTypes_RC_DENIED;
    }

    ok=nfs_rename(st, &st->handles[fromdir].fh, st->handles[fromdir].path,
		  fromname,
		  &st->handles[todir].fh, st->handles[todir].path,
		  toname);

    return ok?FSTypes_RC_OK:FSTypes_RC_FAILURE;
}

static FSTypes_RC NFS_ReadDir_m (
    NFS_cl                   *self,
    NFS_Handle                dir     /* IN */
    /* RETURNS */,
    FSTypes_DirentSeq       **entries )
{
    nfs_st            *st = self->st;
    FSTypes_DirentSeq *seq;
    bool_t             ok;

    /* Create the sequence here (always required, or marshalling code
       blows up) */
    seq      = SEQ_NEW(FSTypes_DirentSeq,0,st->heap);
    *entries = seq;

    /* Check the handle */
    if (!check_inode_handle(st, dir)) return FSTypes_RC_FAILURE;
    /* Make sure it's a directory */
    if (!F_ISDIR(st->handles[dir].attributes.type)) return FSTypes_RC_FAILURE;
    
    /* Only return the contents once. (XXX THIS IS A HACK) */
    if (st->handles[dir].dir_read) return FSTypes_RC_OK;

    /* Do the call */
    ok = nfs_list(st, &st->handles[dir].fh, st->handles[dir].path, seq);

    st->handles[dir].dir_read = True;

    return ok?FSTypes_RC_OK:FSTypes_RC_FAILURE;
}

static FSTypes_RC NFS_Symlink_m (
    NFS_cl         *self,
    NFS_Handle      dir     /* IN */,
    FSTypes_Name    name    /* IN */,
    FSTypes_Name    path    /* IN */ )
{
    nfs_st *st = self->st;
    bool_t  ok;
    sattr   na;

    if (!check_dir(st,dir)) return FSTypes_RC_FAILURE;

    if (st->readonly) {
	TRC(printf("NFS: Symlink: readonly filesystem\n"));
	return FSTypes_RC_DENIED;
    }

    na.mode = 0666;

    na.uid  = 0x0f0;  /* These two are set by the packet filter. */
    na.gid  = 0xf00; 

    na.size = 0;
    na.atime.seconds  = 0; /* XXX These should not be hard-coded. */
    na.atime.useconds = 0;
    na.mtime.seconds  = 0;
    na.mtime.useconds = 0;
    ok=nfs_symlink(st, &st->handles[dir].fh, st->handles[dir].path,
		   name, path, &na);

    return ok?FSTypes_RC_OK:FSTypes_RC_FAILURE;
}

static FSTypes_RC NFS_Readlink_m (
    NFS_cl         *self,
    NFS_Handle      link    /* IN */
    /* RETURNS */,
    FSTypes_Name   *contents )
{
    nfs_st      *st = self->st;
    FSTypes_Name l;

    if (!check_inode_handle(st, link)) return FSTypes_RC_FAILURE;

    if (!F_ISLINK(st->handles[link].attributes.type))
	return FSTypes_RC_FAILURE; /* Not a symlink */

    l = nfs_readlink(st, &st->handles[link].fh, st->handles[link].path,
		     "(NFS_ReadLink_m)");

    if (!l) return FSTypes_RC_FAILURE;

    *contents = l;

    return FSTypes_RC_OK;
}

static FSTypes_Info *NFS_Stat_m (
    NFS_cl  *self )
{
    nfs_st       *st = self->st;
    FSTypes_Info *i;
    
    i        = Heap$Malloc(st->heap, sizeof(*i));
    i->label = NULL; /* NFS volumes are unlabelled */
    i->id    = Heap$Malloc(st->heap, 
			   strlen(st->server) + strlen(st->mountpoint) + 2);
    
    strcpy(i->id,st->server);
    strcat(i->id,":");
    strcat(i->id,st->mountpoint);
    i->used     = 0;
    i->free     = 0;
    i->iused    = 0;
    i->ifree    = 0;
    i->overhead = 0;

    return i;
}


/* Internal NFS routines **************************************************/


#ifdef DATATRACING
static void fmt_fh(nfs_fh *fh, char *buf)
{
    const char *hex = "0123456789ABCDEF";
    char       *fhp = (char *)fh;
    int         i; 

    for (i = 0; i < sizeof(*fh); i++) {
	*buf++ = hex[(*fhp & 0xf0) >> 4];
	*buf++ = hex[(*fhp++ & 0x0f)];
    }
    *buf++ = 0;
}
#endif /* TRACING */


/* At the moment we return all the entries in a directory. It should be
   possible to return only the results of one call at a time. */
static bool_t nfs_list(nfs_st *st, nfs_fh *fh, char *refresh_path,
		       FSTypes_DirentSeq *names)
{
    readdirargs    args;
    readdirres     _res;
    readdirres    *res = &_res;
    dirlist       *dl;
    entry         *entry;
    bool_t         eof = False;
    FSTypes_Dirent ent;

    SET_CTRL_MODE(st);

    memcpy(&args.dir, fh, sizeof(nfs_fh));
    memset(&args.cookie, 0, sizeof(args.cookie));
    /* restrict the result size to 1/2 a page, since we process it
     * by recursively xdring the linked list of directory entries */
    args.count = PAGE_SIZE / 2;
    memset(res, 0, sizeof(*res));

    while (!eof) {

	MU_LOCK(&st->call_lock);
	if (clnt_call(st->client, NFSPROC_READDIR, 
		      xdr_readdirargs, &args, 
		      xdr_readdirres, res, 
		      CTRL_FAIL_TIMEOUT) != RPC_SUCCESS) {
	    clnt_perror(st->client, "NFS readdir");
	    MU_RELEASE(&st->call_lock);
	    return False;
	}
	MU_RELEASE(&st->call_lock);

	if (res->status == NFS_OK) {
	    dl    = &res->readdirres_u.reply;
	    
	    eof   = dl->eof;
	    entry = dl->entries;
	    while (entry) {
		DATATRC(printf("ID: %x NAME:%s COOKIE:%x\n", 
			       entry->fileid, entry->name, 
			       *(uint32_t *)entry->cookie));
		ent.ino  = entry->fileid;
		ent.name = strdup(entry->name); /* Will be freed by marshalling
						 * code - hopefully */
		SEQ_ADDH(names, ent);

		/* Save the cookie of the last entry */
		if (!eof && entry->nextentry == NULL) {
		    memcpy(&args.cookie,&entry->cookie, sizeof(entry->cookie));
		}
		entry = entry->nextentry;
	    } 
	} else {
	    /* server returned error */
	    if (res->status == ESTALE_LINUX || res->status == ESTALE_OSF1 ||
		res->status == ENOTDIR)
	    {
		/* ENOTDIR is amd messing with things under our feet,
		 * and unmounting the directory, I believe */
		if (!refresh_fh(st, fh, refresh_path, "list"))
		    return False;
		return nfs_list(st, fh, refresh_path, names); /* retry */
	    }
	    /* otherwise just print an error message */
	    if (res->status >= 0 &&
		res->status < sizeof(errno_tbl)/sizeof(char *)) {
		printf("NFS: list: readdir failed: %s\n",
		       errno_tbl[res->status]);
	    } else {
		printf("NFS: list: readdir failed, errno=%d\n", res->status);
	    }
	    return False;
	}
	clnt_freeres(st->client, xdr_readdirres, res);
    }
    return True;
}

/* Get NFS filehandle for a file */
static bool_t nfs_lookup(nfs_st	*st,
			 nfs_fh *cwd,
			 char	*refresh_path,
			 char	*filename_in,
			 int	 depth,
			 nfs_fh	*fh,
			 fattr	*attributes)
{  
    char       *seg;  /* single segment of the full path */
    nfs_fh      my_fh;
//    char        fhstring[65];
    fattr       my_attributes;
    diropargs   args;
    diropres    _res, *res = &_res;
    diropokres *ok;
    char        filename_a[512];
    char       *filename = filename_a;

    if (strlen(filename_in)>511) {
	printf("NFS: ERROR: internal filename limit exceeded (512 bytes).\n");
	return False;
    }

    strcpy(filename_a, filename_in);

    /* Start with either root or cwd filehandle */
    if (*filename == '/')
	memcpy(&my_fh, &st->rootfh, sizeof(my_fh));
    else
	memcpy(&my_fh, cwd, sizeof(my_fh));

    SET_CTRL_MODE(st);

    do {
	seg = strchr(filename, '/');
	if (seg == filename) {
	    filename++; 
	    continue;
	}
	if (seg) *seg = '\0';
	DATATRC(printf("NFS: invoking LOOKUP for \"%s\"\n", filename));
    
	memcpy(&args.dir, &my_fh, sizeof(my_fh));
	args.name = filename;
	memset(res, 0, sizeof(*res));

	MU_LOCK(&st->call_lock);
 	if (clnt_call(st->client, NFSPROC_LOOKUP, 
		      xdr_diropargs, &args, 
		      xdr_diropres, res, 
		      CTRL_FAIL_TIMEOUT) != RPC_SUCCESS) {
	    clnt_perror(st->client, "NFS lookup");
	    MU_RELEASE(&st->call_lock);
	    return False;
	}   
	MU_RELEASE(&st->call_lock);

	DATATRC(printf("NFS: lookup status %d\n", res->status));
	if (res->status == NFS_OK) {
	    ok = &res->diropres_u.diropres;

	    /* if its a symlink, need to restse on it */
	    if (ok->attributes.type == NFLNK)
	    {
		char	*linkname;
		TRC(printf("NFS: got back symlink. St depth=%d\n", depth));

		if (depth >= MAX_SYMLINKS)
		{
		    printf("NFS: lookup for `%s': "
			   "Too many symbolic links encountered\n",
			   filename);
		    clnt_freeres(st->client, xdr_diropres, res);
		    return False;
		}

		linkname = nfs_readlink(st, &ok->file, NULL, filename);
		/* note that the refresh_path for this readlink is
		 * NULL because there is no way nfs_lookup() can ever
		 * return the filehandle for the link.  This may
		 * change when we require lstat() */
		TRC(printf("NFS: readlink for `%s' returned `%s'\n",
			   filename, linkname? linkname : "(null)"));
		if (!linkname)
		{
		    clnt_freeres(st->client, xdr_diropres, res);
		    return False;
		}

		/* again, the refresh_path here is NULL, mainly
		 * because it would be a pain to work out the path,
		 * and anyway, the filehandle has only just been
		 * handed to us, so we don't really expect it to be
		 * stale already. */
		if (!nfs_lookup(st, &my_fh, NULL, linkname, depth+1,
				&my_fh, &my_attributes))
		{
		    clnt_freeres(st->client, xdr_diropres, res);	
		    Heap$Free(Pvs(heap), linkname);
		    return False;
		}

		Heap$Free(Pvs(heap), linkname);
		TRC(printf("NFS: done restsive lookup, depth=%d\n", depth));
	    }
	    else
	    {
		memcpy(&my_fh, &ok->file, sizeof(my_fh));
		memcpy(&my_attributes, &ok->attributes, sizeof(my_attributes));
	    }

	    clnt_freeres(st->client, xdr_diropres, res);


	} else {
	    /* server returned an error code */
	    if (res->status == ESTALE_LINUX || res->status == ESTALE_OSF1)
	    {
		if (!refresh_fh(st, cwd, refresh_path, "lookup"))
		    return False;

		/* restart the lookup using the new "cwd" filehandle */
		return nfs_lookup(st, cwd, refresh_path, filename, depth+1,
				  fh, attributes);
	    }		

	    /* otherwise just print an error message */
	    if (res->status >= 0 &&
		res->status < sizeof(errno_tbl)/sizeof(char *)) {
		TRC(printf("NFS: lookup of `%s' failed: %s\n",
			   filename, errno_tbl[res->status]));
	    } else {
		printf("NFS: lookup of `%s' failed, errno=%d\n",
		       filename, res->status);
	    }
	    return False;
	}
    
	if (seg) {
	    *seg = '/';   /* restore the pathname as we go, just in case */
	    filename = seg + 1;   /* move on to next segment */
	}
    } while (seg);

     TRC(printf("NFS: lookup: success\n")); 

    /* Return filehandle and attributes */
    memcpy(fh, &my_fh, sizeof(my_fh));
    memcpy(attributes, &my_attributes, sizeof(my_attributes));
    return True;
}


/* Do a readlink on "fh", and return its contents, or NULL for error */
static char *nfs_readlink(nfs_st *st, nfs_fh *fh, char *refresh_path,
			  const char *filename)
{
    readlinkres  _res, *res = &_res;
    char        *linkname   = NULL;

    SET_CTRL_MODE(st);

    bzero(res, sizeof(*res));

    MU_LOCK(&st->call_lock);
    if (clnt_call(st->client, NFSPROC_READLINK, 
		  xdr_nfs_fh, fh, 
		  xdr_readlinkres, res, 
		  CTRL_FAIL_TIMEOUT) != RPC_SUCCESS)
    {
	clnt_perror(st->client, "NFS readlink");
	MU_RELEASE(&st->call_lock);
	return NULL;
    }   
    MU_RELEASE(&st->call_lock);


    if (res->status == NFS_OK)
    {
	linkname = strdup(res->readlinkres_u.data);
	clnt_freeres(st->client, xdr_readlinkres, res);
	return linkname;
    }
    else
    {
	/* server returned an error code */
	if (res->status == ESTALE_LINUX || res->status == ESTALE_OSF1)
	{
	    if (!refresh_fh(st, fh, refresh_path, "readlink"))
		return NULL;
	    /* restart the readlink using the new "cwd" filehandle */
	    return nfs_readlink(st, fh, refresh_path, filename);
	}

	if (res->status >= 0 &&
	    res->status < sizeof(errno_tbl)/sizeof(char *))
	{
	    printf("NFS: readlink of `%s' failed: %s\n",
		   filename, errno_tbl[res->status]);
	} else {
	    printf("NFS: readlink of `%s' failed, errno=%d\n",
		   filename, res->status);
	}
	return NULL;
    }
}
    

/* Create a file */
static FSTypes_RC nfs_create(nfs_st *st,
			     nfs_fh *dir,
			     char   *refresh_path,
			     char   *filename,
			     sattr  *mode,
			     nfs_fh *fh,
			     fattr  *attributes)
{
    createargs  args;
    diropres    _res, *res = &_res;
    diropokres *ok;

    SET_CTRL_MODE(st);

    /* Set up args */
    memcpy(&args.where.dir, dir, sizeof(*dir));
    args.where.name = filename;
    memcpy(&args.attributes, mode, sizeof(*mode));

    MU_LOCK(&st->call_lock);
    if (clnt_call(st->client, NFSPROC_CREATE, 
		  xdr_createargs, &args, 
		  xdr_diropres, res, 
		  CTRL_FAIL_TIMEOUT) != RPC_SUCCESS) {
	clnt_perror(st->client, "NFS create");
	MU_RELEASE(&st->call_lock);
	return FSTypes_RC_FAILURE; 
    }   
    MU_RELEASE(&st->call_lock);

    if (res->status == NFS_OK) {
	ok = &res->diropres_u.diropres;
	/* Return filehandle and attributes */
	memcpy(fh, &ok->file, sizeof(*fh));
	memcpy(attributes, &ok->attributes, sizeof(*attributes));
	/* Free any large results */
	clnt_freeres(st->client, xdr_diropres, res);
    } else {
	if (res->status == ESTALE_LINUX || res->status == ESTALE_OSF1)
	{
	    if (!refresh_fh(st, dir, refresh_path, "create"))
		return False;
	    return nfs_create(st, dir, refresh_path, filename, mode,
			      fh, attributes);
	}

	if (res->status >= 0 &&
	    res->status < sizeof(errno_tbl)/sizeof(char *)) {
	    printf("NFS: create `%s' failed: %s\n",
		   filename, errno_tbl[res->status]);
	    /* Below is nasty, but we know 13 is permission denied ;-) */
	    if(res->status == 13) return FSTypes_RC_DENIED;
	} else {
	    printf("NFS: create `%s' failed\n", filename);
	}
	return FSTypes_RC_FAILURE; 
    }

    return FSTypes_RC_OK;
}

/* Delete a file or a directory */
static bool_t nfs_delete(nfs_st *st,
			 nfs_fh *dir,
			 char   *refresh_path,
			 char   *filename, u_int op)
{
    diropargs args;
    nfsstat   _res, *res = &_res;

    SET_CTRL_MODE(st);

    memcpy(&args.dir, dir, sizeof(*dir));
    args.name = filename;

    MU_LOCK(&st->call_lock);
    if (clnt_call(st->client, op,
		  xdr_diropargs, &args,
		  xdr_nfsstat, res,
		  CTRL_FAIL_TIMEOUT) != RPC_SUCCESS) {
	clnt_perror(st->client, "NFS delete");
	MU_RELEASE(&st->call_lock);
	return False;
    }
    MU_RELEASE(&st->call_lock);

    if (*res == NFS_OK) {
	/* Free any large results */
	clnt_freeres(st->client, xdr_nfsstat, res);
    } else {
	if (*res == ESTALE_OSF1 || *res == ESTALE_LINUX)
	{
	    if (!refresh_fh(st, dir, refresh_path, "delete"))
		return False;
	    return nfs_delete(st, dir, refresh_path, filename, op);
	}

	if (*res >= 0 &&
	    *res < sizeof(errno_tbl)/sizeof(char *)) {
	    printf("NFS: delete `%s' failed: %s\n",
		    filename, errno_tbl[*res]);
	} else {
	    printf("NFS: delete `%s' failed, errno=%d\n",
		   filename, *res);
	}
	return False;
    }
    return True;
}

/* Read the requested block from this file */
static bool_t nfs_bread(nfs_st *nfs, handle_st *nf,
			unsigned bnum, addr_t addr)
{
    readargs args;
    readres  _res;
    readres  *res = &_res;
    int      numbytes;
    uint32_t blocksize;

    SET_DATA_MODE(nfs);

    blocksize = BLOCKSIZE(nf->attributes.blocksize);
    
    /* Truncate read if necessary */
    numbytes = MIN(blocksize,
		   nf->attributes.size - (bnum * blocksize));

    if (numbytes == 0)
	return True;

tryagain:
    
    memcpy(&args.file, &nf->fh, sizeof(nfs_fh));
    args.offset     = bnum * blocksize;
    args.count      = numbytes;
    args.totalcount = numbytes;
    
    //    nf->length = numbytes;
    //    nf->bnum = bnum;
    
    memset(res, 0, sizeof(*res));
    res->readres_u.reply.data.data_val = addr;
    res->readres_u.reply.data.data_len = numbytes;  /* XXX ??? */

    while (1) {
	MU_LOCK(&nfs->call_lock);
	if (clnt_call(nfs->client, NFSPROC_READ, 
		      xdr_readargs, &args, 
		      xdr_readres, res, 
		      DATA_FAIL_TIMEOUT) != RPC_SUCCESS) {
	    TRC(clnt_perror(nfs->client, "NFS read"));
	    MU_RELEASE(&nfs->call_lock);
	    continue;
	}
	MU_RELEASE(&nfs->call_lock);
    
	if (res->status != NFS_OK) {
	    /* one more check; OSF NFS stale file handle errno. */
	    if (res->status == ESTALE_LINUX || res->status == ESTALE_OSF1)
	    {
		if (!refresh_fh(nfs, &nf->fh, nf->path, "bread"))
		    return False;
		goto tryagain;
	    }
	    if (res->status >= 0 &&
		res->status < sizeof(errno_tbl)/sizeof(char *))
	    {
		TRC(printf("NFS: bread: read failed: %s\n",
			   errno_tbl[res->status]));
	    } else {
		printf("NFS: bread: read failed (errno=%d)\n", 
		       res->status);
	    }
	    return False;
	}
	return True;
    }
}

/* Write current buffer back to NFS */
static bool_t nfs_write(nfs_st *nfs, handle_st *nf, unsigned bnum, addr_t addr)
{
    writeargs args;
    attrstat  _res, *res = &_res;
    uint32_t  bytecount;
    uint32_t  blocksize;

    blocksize = BLOCKSIZE(nf->attributes.blocksize);
    
    bytecount = MIN(blocksize,
		    nf->attributes.size-(bnum*blocksize));

tryagain:

    memcpy(&args.file, &nf->fh, sizeof(nf->fh));
    args.offset      = bnum * blocksize;
    args.beginoffset = bnum * blocksize;

    args.totalcount    = bytecount;
    args.data.data_len = bytecount;
    args.data.data_val = addr;
    memset(res, 0, sizeof(*res));

    DATATRC(printf("NFS: Writeback called.\n"));

    SET_DATA_MODE(nfs);

    while(1) {
	MU_LOCK(&nfs->call_lock);
	if (clnt_call(nfs->client, NFSPROC_WRITE, 
		      xdr_writeargs, &args, 
		      xdr_attrstat, res, 
		      DATA_FAIL_TIMEOUT) != RPC_SUCCESS) {
	    TRC(clnt_perror(nfs->client, "NFS write"));
	    MU_RELEASE(&nfs->call_lock);
	    continue;
	}
	MU_RELEASE(&nfs->call_lock);
	if (res->status == NFS_OK) {
#if 0
	    nf->total_length = res->attrstat_u.attributes.size;
#endif /* 0 */
	} else {

	    /* one more check; OSF NFS stale file handle errno. */
	    if (res->status == ESTALE_LINUX || res->status == ESTALE_OSF1)
	    {
		if (!refresh_fh(nfs, &nf->fh, nf->path, "write"))
		    return False;
		goto tryagain;
	    }
	    if (res->status >= 0 &&
		res->status < sizeof(errno_tbl)/sizeof(char *))
	    {
		printf("NFS write: write failed: %s\n",
			   errno_tbl[res->status]);
	    } else {
		printf("NFS writeback: write failed, errno=%d\n",
		       res->status);
	    }
	}
	clnt_freeres(nfs->client, xdr_attrstat, res);
	return True;
    }
}

static bool_t nfs_link(nfs_st *st, nfs_fh *dir, char *refresh_path,
		       char *name, nfs_fh *from, char *from_refresh_path)
{
    linkargs args;
    nfsstat  _res, *res=&_res;

    memcpy(&args.from, from, sizeof(nfs_fh));
    memcpy(&args.to.dir, dir, sizeof(nfs_fh));
    args.to.name=name;

    SET_CTRL_MODE(st);

    MU_LOCK(&st->call_lock);
    if (clnt_call(st->client, NFSPROC_LINK,
		  xdr_linkargs, &args,
		  xdr_nfsstat, res,
		  CTRL_FAIL_TIMEOUT) != RPC_SUCCESS) {
	clnt_perror(st->client, "NFS link");
	MU_RELEASE(&st->call_lock);
	return False;
    }
    MU_RELEASE(&st->call_lock);

    if (*res == NFS_OK) {
	/* Free any large results */
	clnt_freeres(st->client, xdr_nfsstat, res);
    } else {
	if (*res == ESTALE_OSF1 || *res == ESTALE_LINUX)
	{
	    if (!refresh_fh(st, dir, refresh_path, "link(dir)"))
		return False;
	    if (!refresh_fh(st, from, from_refresh_path, "link(from)"))
		return False;
	    return nfs_link(st, dir, refresh_path, name,
			    from, from_refresh_path);
	}

	if (*res >= 0 &&
	    *res < sizeof(errno_tbl)/sizeof(char *)) {
	    printf("NFS: link `%s' failed: %s\n",
		   name, errno_tbl[*res]);
	} else {
	    printf("NFS: link `%s' failed, errno=%d\n",
		   name, *res);
	}
	return False;
    }
    return True;
}
    
static bool_t nfs_mkdir(nfs_st *st, nfs_fh *dir, char *refresh_path,
			char *name, sattr *mode)
{
    createargs args;
    diropres   _res, *res = &_res;

    memcpy(&args.where.dir, dir, sizeof(nfs_fh));
    args.where.name = name;
    args.attributes = *mode;

    SET_CTRL_MODE(st);

    MU_LOCK(&st->call_lock);
    if (clnt_call(st->client, NFSPROC_MKDIR,
		  xdr_createargs, &args,
		  xdr_diropres, res,
		  CTRL_FAIL_TIMEOUT) != RPC_SUCCESS) {
	clnt_perror(st->client, "NFS mkdir");
	MU_RELEASE(&st->call_lock);
	return False;
    }
    MU_RELEASE(&st->call_lock);

    if (res->status == NFS_OK) {
	/* Free any large results */
	clnt_freeres(st->client, xdr_diropres, res);
    } else {
	if (res->status == ESTALE_LINUX || res->status == ESTALE_OSF1)
	{
	    if (!refresh_fh(st, dir, refresh_path, "mkdir"))
		return False;
	    return nfs_mkdir(st, dir, refresh_path, name, mode);
	}

	if (res->status >= 0 &&
	    res->status < sizeof(errno_tbl)/sizeof(char *)) {
	    printf("NFS: mkdir `%s' failed: %s\n",
		   name, errno_tbl[res->status]);
	} else {
	    printf("NFS: mkdir `%s' failed, errno=%d\n",
		   name, res->status);
	}
	return False;
    }
    return True;
}

static bool_t nfs_rename(nfs_st *st, nfs_fh *fromdir, char *from_refresh_path,
			 char *from,
			 nfs_fh *todir, char *to_refresh_path,
			 char *to)
{
    printf("NFS: rename: unimplemented\n");
    return False;
}

static bool_t nfs_symlink(nfs_st *st, nfs_fh *dir, char *refresh_path,
			  char *name, char *path, sattr *attr)
{
    symlinkargs args;
    nfsstat     _res, *res=&_res;
    
    memcpy(&args.from.dir, dir, sizeof(nfs_fh));
    args.from.name  = name;
    args.to         = path;
    args.attributes = *attr;

    SET_CTRL_MODE(st);

    MU_LOCK(&st->call_lock);
    if (clnt_call(st->client, NFSPROC_SYMLINK,
		  xdr_symlinkargs, &args,
		  xdr_nfsstat, res,
		  CTRL_FAIL_TIMEOUT) != RPC_SUCCESS) {
	clnt_perror(st->client, "NFS symlink");
	MU_RELEASE(&st->call_lock);
	return False;
    }
    MU_RELEASE(&st->call_lock);

    if (_res == NFS_OK) {
	/* Free any large results */
	clnt_freeres(st->client, xdr_nfsstat, res);
    } else {
	if (_res == ESTALE_LINUX || _res == ESTALE_OSF1)
	{
	    if (!refresh_fh(st, dir, refresh_path, "symlink"))
		return False;
	    return nfs_symlink(st, dir, refresh_path, name, path, attr);
	}

	if (_res >= 0 &&
	    _res < sizeof(errno_tbl)/sizeof(char *)) {
	    printf("NFS: symlink `%s' to `%s' failed: %s\n",
		   name, path, errno_tbl[_res]);
	} else {
	    printf("NFS: symlink `%s' to `%s' failed, errno=%d\n",
		   name, path, _res);
	}
	return False;
    }
    return True;
}

/* Return the string concatentation of:
 *  s1 + "/" + s2
 * If s1 is NULL, ignore it.
 */
static char *join(const char *s1, const char *s2)
{
    int lens1 = s1? strlen(s1) : 0;
    int len = lens1 + 1/*slash*/ + strlen(s2) + 1/*terminate*/;
    char *d;
    
    d = Heap$Malloc(Pvs(heap), sizeof(char) * len);
    if (!d)
	return NULL;

    d[0] = '\000';
    if (s1)
	strcat(d, s1);
    strcat(d, "/");
    strcat(d, s2);

    return d;
}


/* Refresh "fh" from "refresh_path".  Returns True for success, False
 * otherwise.  */
static bool_t refresh_fh(nfs_st *st, nfs_fh *fh, char *refresh_path,
			 const char *opname)
{
    bool_t	ok;
    fattr	junk_attr;

    /* Try to refresh the filehandle.  If we were in the middle
     * of a refresh, then the error is (probably) for real */
    if (refresh_path == NULL)
    {
	printf("NFS %s: stale filehandle and no refresh path\n", opname);
	return False;
    }

    TRC(printf("NFS %s: ESTALE, refreshing...\n", opname));
    printf("NFS %s: ESTALE, refreshing...\n", opname);

    /* do a lookup on "refresh_path" to refresh the filehandle */
    ok = nfs_lookup(st, &st->rootfh, NULL, refresh_path, 0,
		    fh, &junk_attr);
    if (!ok)
    {
	printf("NFS %s: refresh of stale filehandle failed\n", opname);
	return False;
    }
    TRC(printf("NFS %s: ...refresh ok\n", opname));
    printf("NFS %s: ...refresh ok\n", opname);

    return True;
}

/* End of nfs.c */
