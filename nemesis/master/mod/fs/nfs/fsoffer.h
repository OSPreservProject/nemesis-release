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
**      Common state for the custom offer. 
** 
** ENVIRONMENT: 
**
**      User space
** 
*/
#ifndef __FSOFFER_H
#define __FSOFFER_H

#include <socket.h>
#include <netorder.h>
#include <nfs_prot.h>

#include <NFS.ih>
#include <IDCOffer.ih>

/* 
** For now, our custom offer doesn't actually require a bind to 
** the 'server' at all; instead it simply makes use of the information
** given within the offer to determine where and how to access the 
** nfs filesystem. 
** In the future (using SECURE_NFS), binding to the offer will instead
** cause the server to allocate a secure port and associated packet
** filter in the flowman, and for this to be returned during the bind
** and used by the client to build their rpc stack. 
*/

typedef struct _IDCOffer_st {
    struct sockaddr_in nfsaddr; 
    nfs_fh             rootfh;
    uint32_t           blocksize;	/* Default blocksize (unused atm) */
    bool_t             debug;
    bool_t             readonly;
    char              *server; 
    char              *mountpoint; 
} IDCOffer_st;

extern IDCOffer_op     fsoffer_ms;

extern NFS_clp nfs_init(struct sockaddr_in *svr, nfs_fh *rootfh, 
			uint32_t bsize, bool_t ro, bool_t debug, 
			char *server, char *mountpoint);

#endif __FSOFFER_H
