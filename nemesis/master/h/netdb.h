/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      netdb.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Database of network-related data
** 
** ENVIRONMENT: 
**
**      User space (provided by libsockets.a)
** 
** ID : $Id: netdb.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef _netdb_h_
#define _netdb_h_

/* A few bits 'n' bobs lifted from linux's netdb.h */

/* A hostent structure */
struct hostent
{
    char    *h_name;        /* official name of host */
    char    **h_aliases;    /* alias list */
    int     h_addrtype;     /* host address type */
    int     h_length;       /* length of address */
    char    **h_addr_list;  /* list of addresses from name server */
#define h_addr  h_addr_list[0]  /* address, for backward compatiblity */
};

struct  protoent {
    char    *p_name;        /* official protocol name */
    char    **p_aliases;    /* alias list */
    int     p_proto;        /* protocol # */
};

/* These functions are provided by the sockets library. Don't use
 * them: they leak memory. They are only for backwards compatability,
 * mainly for the sunrpc library. */
extern struct hostent *gethostbyname(const char *);
extern struct protoent *getprotobyname(const char *);

#endif /* _netdb_h_ */
