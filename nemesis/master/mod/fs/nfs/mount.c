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
**      mounting code. Ho. 
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

#ifdef SECURE_NFS
#include <CSIDCTransport.ih>
#include <FlowMan.ih>
#include <FSClient.ih>
#include <IDCCallback.ih>
#endif

#include <MountRemote.ih>


#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif


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


/* Some useful macros */
static const struct timeval CTRL_FAIL_TIMEOUT  = { 
    (NFSCTRL_FAIL_TIMEOUT_MS / 1000), 
    ((NFSCTRL_FAIL_TIMEOUT_MS % 1000) * 1000) 
};

static const struct timeval CTRL_RETRY_TIMEOUT = { 
    (NFSCTRL_RETRY_TIMEOUT_MS / 1000), 
    ((NFSCTRL_RETRY_TIMEOUT_MS % 1000) * 1000) 
};


/* MountRemote interface */

static  MountRemote_Mount_fn            MountRemote_Mount_m;
static  MountRemote_SimpleMount_fn      MountRemote_SimpleMount_m;

static  MountRemote_op  mount_ms = {
    MountRemote_Mount_m,
    MountRemote_SimpleMount_m
};

static  const MountRemote_cl  cl = { &mount_ms, NULL };

CL_EXPORT (MountRemote, NFS, cl);


#ifdef SECURE_NFS
/* IDCCallback interface */

static  IDCCallback_Request_fn          IDCCallback_Request_m;
static  IDCCallback_Bound_fn            IDCCallback_Bound_m;
static  IDCCallback_Closed_fn           IDCCallback_Closed_m;

static  IDCCallback_op  callback_ms = {
    IDCCallback_Request_m,
    IDCCallback_Bound_m,
    IDCCallback_Closed_m
};

extern CSClientStubMod_cl stubmod_cl;
#endif


typedef struct _mremote mremote;

struct _mremote {
    Heap_clp		heap;
#ifdef SECURE_NFS
    IDCCallback_cl	callback;
    IDCService_clp	service;
    FlowMan_clp         fman;           /* For binding sockets, etc */
#endif
    IDCOffer_cl 	offer;

    uint32_t		blocksize;	/* Default blocksize (unused atm) */
    bool_t		debug;
    bool_t		readonly;

    string_t		server;
    string_t		mountpoint;	/* Remember who we are */

    struct sockaddr_in  nfsaddr;        /* sockaddr for the remote NFS svr   */

    CLIENT	       *client;	        /* client SunRPC binding to NFS svr  */
    nfs_fh		rootfh;		/* file hdl on the root of the svr   */
};


/* Internal NFS methods */
static void nfs_mount(mremote *st, const char *server, char *mountpoint);
static void inet_gethostaddr (const char *host, struct in_addr *sin_addr);



/*
** SMH: to do the security thing properly, we should have this server 
** allocate a secure port in the flowman on behalf of the client during
** the bind process. It should also install correct packet filters, etc. 
** For now, it is not necessary to do this, and is rather tedious. 
** So left as exercise for the reader...
*/
#undef SECURE_NFS
#ifdef SECURE_NFS
/*---------------------------------------- IDCCallback Entry Points ----*/
static bool_t IDCCallback_Request_m (
    IDCCallback_cl         *self,
    IDCOffer_clp            offer   /* IN */,
    Domain_ID               dom     /* IN */,
    ProtectionDomain_clp    pdom    /* IN */,
    const Binder_Cookies   *clt_cks /* IN */,
    Binder_Cookies         *svr_cks /* OUT */ )
{
    mremote          *st = self->st; 
    client_st        *cst; 
    FlowMan_SAP       lsap, rsap; 
    FlowManRouteInfo *ri; 
    FlowMan_Flow      flow; 
    char             *ifname; 

    eprintf("FSClient$Bind: from dom %qx\n", dom);
    cst = Heap$Malloc(st->heap, sizeof(*cst)); 

    /* Now sort out a (secure) UDP port for our client */
    lsap.tag          = FlowMan_PT_UDP;
    lsap.u.UDP.port   = 666 + (dom & 0xFF);   /* nasty */
    lsap.u.UDP.addr.a = 0;                    /* bind is for all ifaces */

    if(!FlowMan$Bind(st->fman, &sap)) {
	eprintf("FlowMan$Bind failed.\n");
	ntsc_dbgstop();
    }

    rsap.tag          = FlowMan_PT_UDP;
    rsap.u.UDP.port   = st->nfsaddr.sin_port;
    rsap.u.UDP.addr.a = st->nfsaddr.sin_addr.s_addr;

    ri = FlowMan$ActiveRoute(st->fman, &rsap);

    /* note the interface we're using so we can tell if it changes */
    ifname = strdup(ri->ifname);

    /* get some IO offers */
    flow = FlowMan$Open(st->fman, st->ifname, &txqos, Netif_TXSort_Socket,
			&rxoffer, &txoffer);


    return True;
}

static bool_t IDCCallback_Bound_m (
    IDCCallback_cl         *self,
    IDCOffer_clp            offer   /* IN */,
    IDCServerBinding_clp    binding /* IN */,
    Domain_ID               dom     /* IN */,
    ProtectionDomain_clp    pdom    /* IN */,
    const Binder_Cookies   *clt_cks /* IN */,
    Type_Any               *server /* INOUT */ )
{
    mremote *st = self->st; 

    /* XXX SMH: on bind, should install packet filter */
    eprintf("FSClient$Bound: dom %qx\n", dom);

    return True;
}

static void IDCCallback_Closed_m (
    IDCCallback_cl         *self,
    IDCOffer_clp            offer   /* IN */,
    IDCServerBinding_clp    binding /* IN */,
    const Type_Any         *server /* IN */ )
{
    mremote *st = self->st; 

    /* XXX SMH: on close, tidy up whatever we have */
    
    return;
}
#endif




/*-------------------------------------- MountRemote entry points */

static IDCOffer_clp MountRemote_SimpleMount_m (
    MountRemote_cl *self,
    string_t        host            /* IN */,
    string_t        mountpoint      /* IN */ )
{
    return MountRemote_Mount_m(self, 
			       host, 
			       mountpoint,
			       0, 
			       NULL);
}

static IDCOffer_clp MountRemote_Mount_m (
    MountRemote_cl         *self,
    string_t                host       /* IN */,
    string_t                mountpoint /* IN */,
    MountRemote_Options     options    /* IN */,
    Context_clp             settings   /* IN */ )
{
    mremote            *st;
    IDCOffer_clp        res;
    IDCOffer_st        *idc_st;
    extern IDCOffer_op  fsoffer_ms;

    st = Heap$Malloc(Pvs(heap), sizeof(*st));
    memset(st,0,sizeof(*st));
    st->heap = Pvs(heap);

    /* XXX check for blocksize parameter in settings */
    st->blocksize   = DEFAULT_NFS_BLOCKSIZE;

    if (SET_IN(options, MountRemote_Option_Debug)) st->debug=True;
    if (SET_IN(options, MountRemote_Option_ReadOnly)) st->readonly=True;

    st->server     = strduph(host, st->heap);
    st->mountpoint = strduph(mountpoint, st->heap);


#ifdef SECURE_NFS
    /* Get a binding to the flowman */
    st->fman = IDC_OPEN("svc>net>iphost-default", FlowMan_clp);
    CL_INIT(st->callback, &callback_ms, st);
#endif

    TRC(eprintf("NFS: Mount host '%s' path '%s' %s\n",
		host, mountpoint, st->readonly?"readonly":"read/write"));

    /* Now mount the filesystem and remember the root filehandle */
    nfs_mount(st, host, mountpoint);



    /* We create a custom offer for the client */

    idc_st = Heap$Malloc(st->heap, sizeof(*idc_st));
    CL_INIT(st->offer, &fsoffer_ms, idc_st);

    memcpy(&idc_st->nfsaddr, &st->nfsaddr, sizeof(struct sockaddr_in));
    memcpy(&idc_st->rootfh, &st->rootfh, sizeof(nfs_fh));
    idc_st->blocksize  = st->blocksize; 
    idc_st->debug      = st->debug; 
    idc_st->readonly   = st->readonly;
    idc_st->server     = strduph(st->server, st->heap); 
    idc_st->mountpoint = strduph(st->mountpoint, st->heap); 

    res = &st->offer; 

    TRC(eprintf("NFS: offer at %p\n",res));

    return res;
}


/*----------------------------------------------- Internal Routines -------- */

/* XXX Currently ignores the mount options */
static void nfs_mount(mremote *st, const char *server, char *mountpoint)
{
    struct pmap parms;
    int         sock;
    dirpath     dir = mountpoint;
    uint16_t    mountport;

    sock                   = RPC_ANYSOCK;
    st->nfsaddr.sin_family = AF_INET;
    st->nfsaddr.sin_port   = hton16(PMAPPORT);
    inet_gethostaddr(server, &(st->nfsaddr.sin_addr));
  
    TRC(printf("NFS: mount: contacting portmapper\n"));
    st->client = clntudp_create(&st->nfsaddr, PMAPPROG, PMAPVERS, 
				CTRL_RETRY_TIMEOUT, &sock);

    if (st->client == (CLIENT *)NULL) {
	printf("NFS: mount: clntudp_create for portmapper failed\n");
	RAISE_MountRemote$Failure(MountRemote_Problem_Server);
    }

    /* Don't close the socket on destroy, even though it was allocated
     * by sunrpc library */
    clnt_control(st->client, CLSET_FD_NCLOSE, NULL);

    parms.pm_prog = MOUNTPROG;
    parms.pm_vers = MOUNTVERS;
    parms.pm_prot = IPPROTO_UDP;
    parms.pm_port = 0;  /* not needed or used */

    if (CLNT_CALL(st->client, PMAPPROC_GETPORT, xdr_pmap, &parms,
		  xdr_u_short, &mountport, CTRL_FAIL_TIMEOUT) != RPC_SUCCESS){
	printf("NFS: mount: portmapper failure\n");
	CLNT_DESTROY(st->client);
	close(sock);
	RAISE_MountRemote$Failure(MountRemote_Problem_Server);
    } else if (mountport == 0) {
#if 0
	rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
#endif /* 0 */
    }
    CLNT_DESTROY(st->client);

    TRC(printf("NFS: mount: MOUNT port is %04x\n", mountport));
    if (!mountport) {
	printf("NFS: mount: couldn't find port for "
	       "mount service from portmapper\n");
	RAISE_MountRemote$Failure(MountRemote_Problem_Server);
    }

    /* SUNRPC didn't close the socket so we can use it again */
    st->nfsaddr.sin_port   = hton16(mountport); 
  
    TRC(printf("NFS: mount: clntudp_create MOUNT\n"));
    st->client = clntudp_create(&st->nfsaddr, MOUNTPROG, MOUNTVERS, 
			    CTRL_RETRY_TIMEOUT, &sock);
    
    TRC(printf("NFS: mount: clntudp_create returned %p\n", st->client));
    if (st->client == (CLIENT *)NULL) {
	printf("NFS: mount: clntudp_create for mount port failed");
	close(sock);
	RAISE_MountRemote$Failure(MountRemote_Problem_Server);
    }

    TRC(printf("NFS: mount: invoking MNT operation for \"%s\"\n", dir));
    {
	fhstatus _fhs, *fhs = &_fhs;

	bzero(fhs, sizeof(*fhs));

	if (clnt_call(st->client, MOUNTPROC_MNT, 
		      xdr_dirpath, &dir, 
		      xdr_fhstatus, fhs, 
		      CTRL_FAIL_TIMEOUT))
	{
	    clnt_perror(st->client, "NFS mount");
	    CLNT_DESTROY(st->client);
	    close(sock);
	    RAISE_MountRemote$Failure(MountRemote_Problem_Server);
	}

	if (!fhs->fhs_status) {
	    bcopy(&fhs->fhstatus_u.fhs_fhandle, &(st->rootfh),
		  sizeof(nfs_fh));
	} else {
	    printf("NFS: mount: mount of \"%s\" from \"%s\" failed\n",
		   mountpoint, server);
	    if (fhs)
		if (fhs->fhs_status >= 0 &&
		    fhs->fhs_status < sizeof(errno_tbl)/sizeof(char *)) {
		    printf("NFS: mount: %s\n", errno_tbl[fhs->fhs_status]);
		} else {
		    printf("NFS: mount: errno=%d\n", fhs->fhs_status);
		}
	    CLNT_DESTROY(st->client);
	    close(sock);
	    RAISE_MountRemote$Failure(MountRemote_Problem_Server);
	}
    }
    
    CLNT_DESTROY(st->client);

    /* test our NFS connection && setup st->nfsaddr correctly */
    st->nfsaddr.sin_port = hton16(NFS_PORT); 

    TRC(printf("NFS: mount: clntudp_create NFS\n"));
    st->client = clntudp_create(&st->nfsaddr, NFS_PROGRAM, NFS_VERSION, 
			    CTRL_RETRY_TIMEOUT, &sock);
    
    TRC(printf("NFS: mount: clntudp_create returned %p\n", st->client));
    if (st->client == (CLIENT *)NULL) {
	printf("NFS: mount: mounted server, "
	       "but clntudp_create for NFS port failed\n");
	close(sock);
	RAISE_MountRemote$Failure(MountRemote_Problem_Server);
    }

    /* ok, everything seems fine, so squirrel away the client side binding */
    return;
}

/* This is horrible. Use DNS code instead. */
static void inet_gethostaddr (const char *host, struct in_addr *sin_addr)
    /* RAISES Context_NotFound, TypeSystem_Incompatible, Net_BadAF */
{
    unsigned int a,b,c,d;

    if (sscanf(host, "%d.%d.%d.%d", &a,&b,&c,&d) != 4)
    {
	printf("NFS: inet_gethostaddr: bad hostname format `%s'\n", host);
	RAISE_MountRemote$Failure(MountRemote_Problem_Server);
    }

    sin_addr->s_addr = a | b<<8 | c<<16 | d<<24;
}



/* 
 * End of mount.c 
 */
