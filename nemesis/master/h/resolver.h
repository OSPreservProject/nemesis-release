/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Glasgow Department of Computing Science   *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	resolver.h
**
** FUNCTIONAL DESCRIPTION:
**
**	contains useful details for resolver.c and DNS.c
**
** ENVIRONMENT: 
**
**      
**
** ID : $Id: resolver.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
**
*/

/*
 * ++Copyright++ 1985, 1993
 * -
 * Copyright (c) 1985, 1993
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#include <socket.h>
#include <nemesis.h>

/* defined error codes */

#define NOERROR  0       /* No error */
#define FORMERR  1       /* Format error */
#define SERVFAIL 2       /* Server failure */
#define NXDOMAIN 3       /* Non existent domain */
#define NOTIMP   4       /* Not implimented */
#define REFUSED  5       /* Query refused */

/* various constants that are needed */

#define NAMESERVER_PORT 53  /* port for nameserver */
#define PACKETSZ 512        /* Maximum packet size */
#define HFIXEDSZ 12         /* no. bytes of fixed data in header */
#define QFIXEDSZ 4          /* no. bytes of fixed data in query */
#define RRFIXEDSZ 10        /* no. bytes of fixed data in r record */
#define INT16SZ 2           /* for systems without 16 bit ints */
#define INT32SZ 4           /* for systems without 32 bit ints */  
#define MAX_RETRY 16        /* Maximum number of retries */  
#define INDIR_MASK  0xc0    /* for compressed domain names */
#define MAXLABEL 63         /* max lenght of domain label */
#define MAXPACKET 1024      /* maximum packet size */
#define INADDRSZ 4          /* size of inaddr */
#define MAXCDNAME 255       /* max compression domain name */
#define MAXNS    3          /* Max no. Nameservers searched */
#define MAXDNAME 256        /* maximum domain name */
#define INADDR_ANY (uint32_t)0x00000000

/* Structure for the query header, Byte ordering relies on the
 * defined macros
 */

typedef struct {
    unsigned   id:16 ;      /* query identification number */
#ifdef BIG_ENDIAN
/* fields in the 3rd byte */ 
    unsigned   qr:1 ;       /* response flag */
    unsigned   opcode:4 ;   /* purpose of message */
    unsigned   aa:1 ;       /* authoritive answer */
    unsigned   tc:1 ;       /* truncated message */
    unsigned   rd:1 ;       /* recursion desired */
/* fields in the 4th byte */
    unsigned   ra:1 ;       /* recursion available */
    unsigned   pr:1 ;       /* primary server req. */
    unsigned   unused:2 ;   /* unused bits */
    unsigned   rcode:4 ;    /* response code */
#endif /* big endian */

#ifdef LITTLE_ENDIAN
/* fields in the 3rd byte */
    unsigned rd:1 ;         /* recursion desired */
    unsigned tc:1 ;         /* truncated message */
    unsigned aa:1 ;         /* authoritive answer */
    unsigned opcode:4 ;     /* purpose of message */
    unsigned qr:1 ;         /* response flag */
/* fields in the 4th byte */
    unsigned rcode:4 ;      /* response code */
    unsigned unused:2 ;     /* unused bits */
    unsigned pr:1 ;         /* primary server req. */
    unsigned ra:1 ;         /* recursion available */
#endif /* little endian */
    
/* remaining bytes */
    unsigned qdcount:16 ;   /* number of question entries */
    unsigned ancount:16 ;   /* number of answer entries */
    unsigned nscount:16 ;   /* number of authority entries */
    unsigned arcount:16 ;   /* number of resource entries */
} header;

/* struct for the reply */

struct rrec {
    int16_t r_zone;       /* zone number */
    int16_t r_class;      /* class number */
    int16_t r_type;       /* type number */
    uint32_t       r_ttl;        /* time to live */
    int       r_size;       /* size of data area */
    char      *r_data;      /* pointer to data */
};


/* defined opcodes (based on RFC`s) that are used at the mo (may change) */

#define QUERY    0x0        /* standard query */
#define IQUERY   0x1        /* inverse query */

/* type values for the machines being serched (this is put into the type field) */

#define T_A             1               /* Host address */
#define T_NS            2               /* Authoritative server */
#define T_MD            3               /* Mail destination */
#define T_MF            4               /* Mail Forwarder */
#define T_CNAME         5               /* Connonical name */
#define T_SOA           6               /* Start of Autority zone */
#define T_MB            7               /* Mailbox domain name */
#define T_MG            8               /* Mail group member */
#define T_MR            9               /* Mail rename name */
#define T_NULL          10              /* Null resource record */
#define T_WKS           11              /* Well known service */
#define T_PTR           12              /* Domain name pointer */
#define T_HINFO         13              /* Host information */
#define T_MINFO         14              /* Mail information */
#define T_MX            15              /* Mail routing information */
#define T_TXT           16              /* text strings */
#define T_RP            17              /* responsible person */
#define T_AFSDB         18              /* AFS cell database */
#define T_X25           19              /* X_25 calling address */
#define T_ISDN          20              /* ISDN calling address */
#define T_RT            21              /* router */
#define T_NSAP          22              /* NSAP address */
#define T_NSAP_PTR      23              /* reverse NSAP lookup (deprecated) */
#define T_SIG           24              /* security signature */
#define T_KEY           25              /* security key */
#define T_PX            26              /* X.400 mail mapping */
#define T_GPOS          27              /* geographical position (withdrawn) */
#define T_AAAA          28              /* IP6 Address */
#define T_LOC           29              /* Location Information */
#define T_NXT           30              /* Next Valid Name in Zone */
#define T_EID           31              /* Endpoint identifier */
#define T_NIMLOC        32              /* Nimrod locator */
#define T_SRV           33              /* Server selection */
#define T_ATMA          34              /* ATM Address */
#define T_NAPTR         35              /* Naming Authority PoinTeR */
#define T_IXFR          251             /* incremental zone transfer */
#define T_AXFR          252             /* transfer zone of authority */
#define T_MAILB         253             /* transfer mailbox records */
#define T_MAILA         254             /* transfer mail agent records */
#define T_ANY           255             /* wildcard match */

/* values required for inserting into the class field */


#define C_IN            1               /* the arpa internet */
#define C_CHAOS         3               /* for chaos net (MIT) */
#define C_HS            4               /* for Hesiod name server (MIT) (XXX) */
#define C_ANY           255             /* wildcard match */






