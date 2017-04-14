/*
 * Rpc additions to <nemesis.h>
 */

#ifndef __RPC_TYPES_H__
#define __RPC_TYPES_H__

#include "nemesis.h"

#define u_char  unsigned char
#define u_short unsigned short
#define u_int   unsigned int
#define u_long  unsigned long
#define caddr_t addr_t

#define enum_t int
#define	FALSE	False
#define	TRUE	True
#define __dontcare__	-1
#ifndef NULL
#define NULL 0
#endif

/* Definitions used for NFS V3 and handy for others */
typedef unsigned long uint64;
typedef unsigned long u_longlong_t;

typedef long int64;
typedef long longlong_t;


#define SOCKADDR struct sockaddr_in

extern void *malloc();
#define mem_alloc(bsize) malloc(bsize)
#define mem_free(a,b) free(a)

#endif __RPC_TYPES_H__



