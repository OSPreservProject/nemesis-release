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
**      mod/fs/snfs
**
** FUNCTIONAL DESCRIPTION:
** 
**      Common state things for nfs. 
** 
** ENVIRONMENT: 
**
**      User space
** 
*/

#include <mutex.h>
#include <socket.h>
#include <netorder.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>

#include <mount.h>
#include <fsoffer.h>

#include <NFS.ih>

/* Default NFS blocksize */
#define DEFAULT_NFS_BLOCKSIZE 8192
#define MAX_NFS_BLOCKSIZE 8192
#define MIN_NFS_BLOCKSIZE  512

#define MAX_CLIENT_HANDLES 32

/* Work out what blocksize to report */
#define BLOCKSIZE(_x) (MAX(MIN((_x),MAX_NFS_BLOCKSIZE),MIN_NFS_BLOCKSIZE))

/* how many times to expand a symlink before failing */
#define MAX_SYMLINKS 8

/*
** SMH: we perform our nfs operations as sunrpc on top of udp, and 
** hence have two different timeouts - the "retry" timeout, which is 
** the time at which we decide to retry an operation in the case there 
** has been no reply, and the "fail" timeout, which is the time at which we
** decide there is no point retrying anymore, and just fail.
** 
** In order to try to optimise block reads/writes, but without penalising
** the safety of mounts et al, we define two sets of these timeouts: one
** for control operations and one for data ones. Defaults for these, in 
** milliseconds, are given below. 
** 
** Sites which have different network setups may wish to modify 
** these by adding lines to $(ROOT)/mk/local.rules.mk, e.g.
** 
**  DEFINES_mod/fs/nfs += -DNFSCTRL_RETRY_TIMEOUT_MS=1500 \ 
**    -DNFSCTRL_FAIL_TIMEOUT_MS=60000 -DNFSDATA_RETRY_TIMEOUT_MS=300 \
**    -DNFSDATA_FAIL_TIMEOUT_MS=10000
*/  

#ifndef NFSCTRL_RETRY_TIMEOUT_MS
#define NFSCTRL_RETRY_TIMEOUT_MS   1000
#endif

#ifndef NFSCTRL_FAIL_TIMEOUT_MS
#define NFSCTRL_FAIL_TIMEOUT_MS   10000
#endif

#ifndef NFSDATA_RETRY_TIMEOUT_MS
#define NFSDATA_RETRY_TIMEOUT_MS     50
#endif

#ifndef NFSDATA_FAIL_TIMEOUT_MS
#define NFSDATA_FAIL_TIMEOUT_MS    2500
#endif


#define SET_CTRL_MODE(st) \
do {								\
    if((st)->datamode) {					\
	clnt_control((st)->client,				\
		     CLSET_RETRY_TIMEOUT, &CTRL_RETRY_TIMEOUT);	\
	(st)->datamode = False;					\
    }								\
} while(0)

#define SET_DATA_MODE(st) \
do {									      \
    if(!(st)->datamode) {						      \
	clnt_control((st)->client, CLSET_RETRY_TIMEOUT, &DATA_RETRY_TIMEOUT); \
	(st)->datamode = True;						      \
    }									      \
} while(0)


/* Internal structures ******************************************************/


typedef struct _nfs_st nfs_st;
typedef struct _handle_st handle_st;

struct _handle_st {
    bool_t used;
    nfs_fh fh;
    char *path;
    bool_t open;
    bool_t writable;
    bool_t dir_read;
    fattr attributes;
};

#define MAX_HANDLES 128

struct _nfs_st {
    NFS_cl              cl;

    Heap_clp		heap;

    uint32_t		blocksize;	/* Default blocksize (unused atm) */
    bool_t		debug;
    bool_t		readonly;

    char               *server; 
    char               *mountpoint; 

    handle_st           handles[MAX_HANDLES];


    /* XXX SMH: later (SECURE_NFS) will want socket details too */

    nfs_fh		rootfh;		/* file hdl on the root of the svr   */
    CLIENT	       *client;	        /* client SunRPC binding to NFS svr  */
    mutex_t             call_lock;      /* lock on SunRPC xdr buffers        */
    bool_t		datamode;	/* current SunRPC timeouts in effect */
};

#define	ESTALE_LINUX		116	/* Stale NFS file handle, from linux */
#define	ESTALE_OSF1		 70	/* Stale NFS file handle, from OSF1 */
#define ENOTDIR			 20	/* amd unmounting something under us */

/* Some useful macros */
#define F_ISDIR(_a) ((_a)==NFDIR)
#define F_ISLINK(_a) ((_a)==NFLNK)
#define F_ISREG(_a) ((_a)==NFREG)


/* End of nfs_st.h */
