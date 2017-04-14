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
**      common-fs.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Common filesystem emulation functions private to libc
** 
** ENVIRONMENT: 
**
**      Libc private functions
** 
** ID : $Id: common-fs.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <stdio.h>

#include <FSClient.ih>
#include <FSDir.ih>

#include "common-fs.h"

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

static FSDir_clp import_fs(const char *fs)
{
    NOCLOBBER IDCOffer_clp offer;
    Type_Any offerany, clientany, dirany, *diranyp, tmpany;
    FSClient_clp client = NULL;
    FSDir_clp dir = NULL;
    FSTypes_RC rc;
    char name[64];
    string_t NOCLOBBER certificate;

    if (!Context$Get(Pvs(root),"fs",&tmpany)) {
	TRC(fprintf(stderr,"common-fs: creating fs context in root...\n"));
	CX_NEW("fs");
    }

    strcpy(name,"mounts>");
    strcat(name,fs);

    if (!Context$Get(Pvs(root),name,&offerany)) {
        return NULL; /* No filesystem of this name */
    }
    TRY {
        offer = NARROW(&offerany, IDCOffer_clp);
    }
    CATCH_ALL {
        fprintf(stderr,"common-fs: mounts>%s is not an IDCOffer\n",fs);
        offer = NULL;
    }
    ENDTRY;

    if (!offer) return NULL;

    TRY {
        ObjectTbl$Import(Pvs(objs), offer, &clientany);
    }
    CATCH_ALL {
        fprintf(stderr,"common-fs: failure binding to filesystem %s\n",fs);
        offer = NULL;
    }
    ENDTRY;

    if (!offer) return NULL;
    TRY {
        client = NARROW(&clientany, FSClient_clp);
    }
    CATCH_ALL {
        fprintf(stderr,"common-fs: filesystem %s not compatible "
                "with FSClient\n", fs);
        client = NULL;
    }
    ENDTRY;

    if (!client) return NULL; /* Something bogus going on */

    /* Fetch a certificate to allow us to access the filesystem.
       XXX hack: this is a string, the initial working directory, for now */
    strcpy(name,"env>fs_cert>");
    strcat(name,fs);
    if (!Context$Get(Pvs(root),name,&tmpany) || !ISTYPE(&tmpany,string_t)) {
	certificate="";
    } else {
	certificate=NARROW(&tmpany,string_t);
    }

    TRC(printf("common-fs: using certificate \"%s\"\n",certificate));

    /* Obtain the root directory of the filesystem */
    rc = FSClient$GetDir(client, "", True, &diranyp);

    dirany=*diranyp;
    FREE(diranyp);

    if (rc!=FSTypes_RC_OK) {
        fprintf(stderr,"common-fs: could not get root of filesystem %s\n",
                fs);
        return NULL;
    }
    
    /* Ok, we've got it, now make sure it's compatible with FSDir */
    TRY {
        dir = NARROW(&dirany, FSDir_clp);
    }
    CATCH_ALL {
        fprintf(stderr,"common-fs: root of filesystem %s not compatible "
                "with FSDir\n", fs);
        dir=NULL;
    }
    ENDTRY;
    if (!dir) return NULL;

    /* XXX hack: CD to the certificate */
    FSDir$Lookup(dir, certificate, True);
    FSDir$CWD(dir);

    /* That's it; register the client and dir in the local namespace */
    strcpy(name,"fs>client_");
    strcat(name,fs);
    Context$Add(Pvs(root),name,&clientany);
    
    strcpy(name,"fs>cwd_");
    strcat(name,fs);
    Context$Add(Pvs(root),name,&dirany);

    return dir;
}

FSDir_clp _fs_getfscwd(const char *fs)
{
    NOCLOBBER FSDir_clp dir = NULL;
    char name[64];

    strcpy(name,"fs>cwd_");
    strcat(name,fs);

    TRY {
        dir = NAME_FIND(name, FSDir_clp);
    }
    CATCH_Context$NotFound(x) {
        dir = import_fs(fs);
    }
    ENDTRY;

    return dir;
}

FSDir_clp _fs_getcwd(void)
{
    NOCLOBBER FSDir_clp dir = NULL;
    Type_Any any;

    TRY {
        dir = NAME_FIND("fs>cwd", FSDir_clp);
    }
    CATCH_Context$NotFound(x) {
        dir = NULL;
    }
    ENDTRY;

    if (!dir) {
        /* Try the "init" filesystem */
        dir = _fs_getfscwd("init");

        if (!dir) return NULL; /* No filesystems at all */

        ANY_INIT(&any, FSDir_clp, dir);

        Context$Add(Pvs(root), "fs>cwd", &any);
    }

    return dir;
}

/* End of common-fs.c */
