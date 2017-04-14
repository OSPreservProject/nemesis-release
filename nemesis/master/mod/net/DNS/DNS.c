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
**	DNS.c
**
** FUNCTIONAL DESCRIPTION:
**
**	resolves DNS enquires
**
** ENVIRONMENT: 
**
**     
**
** ID : $Id: DNS.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
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


#include <nemesis.h>
#include <exceptions.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <bstring.h>
#include <DNSMod.ih>
#include <DNS.ih>
#include <Net.ih>
#include <socket.h>
#include <ctype.h>
#include <netorder.h>
#include <resolver.h>


//#define DEBUG

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);

typedef struct {
    DNS_cl cl;
    int retrans;
    int retry;
    int nscount;
    struct sockaddr_in nsaddr_list[MAXNS];
    int id;
} DNS_st;

static	DNS_ResQuery_fn 		DNS_ResQuery_m;
static	DNS_ResIQuery_fn 		DNS_ResIQuery_m;

static	DNS_op	DNS_ms = {
  DNS_ResQuery_m,
  DNS_ResIQuery_m
};

static DNSMod_New_fn                    New_m;

static DNSMod_op  ms = {
    New_m
};

static const	DNSMod_cl	cl = { &ms, NULL }; 

CL_EXPORT (DNSMod, DNSMod, cl); 

#ifdef DEBUG
static void pkt_dump(header *p);
#endif


/* Other functions that are needed */

static int32_t res_mkquery(int, const char *, int, int, const char *, int, const char *, char *, int, DNS_st *);
static int res_send(const char *, int, char *, int, DNS_st *);
static int dn_comp(const char *, char *, int, char **, char **); 
static int dn_find(char *, char *, char **, char **);
static int mklower(int);
static void putshort(uint16_t, char *);
static void putlong(uint32_t, char *);
static uint16_t getshort(const char *);
static int our_server(const struct sockaddr_in *, DNS_st *);
static int queries_match(const char *, const char *, const char *, const char *);
static int name_in_query(const char *, int, int, const char *, const char *);
static int dn_expand(const char *, const char *, const char *, char *, int);

/*---------------------------------------------------- Entry Points ----*/

static DNS_clp New_m (
    DNSMod_clp  self,
    const Net_IPHostAddr    *hosts   /* IN */ )
{

    DNS_clp dnsp;
    DNS_st *st = Heap$Malloc(Pvs(heap), sizeof(*st));
    
    dnsp = (DNS_clp) st;

    CLP_INIT(dnsp, &DNS_ms, st);

/* initialise variables */
    
    st->retrans = 5;
    st->retry = 4;
    st->nsaddr_list[0].sin_addr.s_addr = INADDR_ANY;
    st->nsaddr_list[0].sin_family = AF_INET;
    st->nsaddr_list[0].sin_port = htons(NAMESERVER_PORT);
    st->nscount = 0;


    TRC(printf("address = %I\n", hosts->a));
    st->nsaddr_list[0].sin_addr.s_addr = hosts->a;
    st->nscount++;
    TRC(printf("nscount = %i\n", st->nscount));

    /* pick a "random" start id */
    st->id = (uint32_t) NOW();

    return(dnsp);
}


static int32_t DNS_ResQuery_m (
    DNS_cl	*self,
    string_t	name	/* IN */,
    uint32_t	class	/* IN */,
    uint32_t	type	/* IN */, 
    DNS_OctetSeq *answer /* INOUT */ )
{


    DNS_st	*st = self->st;
    char *buf = Heap$Malloc(Pvs(heap), MAXPACKET); 
    int buflen = MAXPACKET;
    int n;

    char *ans = Heap$Malloc(Pvs(heap), MAXPACKET);
    header *hp = (header *) ans;
    int anssize = MAXPACKET; 
    int i;
    char *cp;

    hp->rcode = NOERROR;   /* default */

/* create the query packet */

    n = res_mkquery(QUERY, name, class, type, NULL, 0, NULL, buf, buflen, st);
    if (n <= 0)
    {
	TRC(printf("mkquery failure\n"));
	Heap$Free(Pvs(heap), ans);
	Heap$Free(Pvs(heap), buf);
	return(-1);
    }
    TRC(printf("made query\n"));

    TRC(pkt_dump((void*)buf));

/* Send the packet and await the response */

    n = res_send(buf, buflen, ans, anssize, st);
    TRC(printf("res_send returned\n"));
    if (n < 0)
    {
	TRC(printf("res_query:send error\n"));
	Heap$Free(Pvs(heap), ans);
	Heap$Free(Pvs(heap), buf);
	return(-1);
    }

/* check the resonse for errors */

#ifdef DEBUG
    printf("return code=%d, count=%d\n", hp->rcode, ntohs(hp->ancount));
    pkt_dump(hp);
#endif

    if (hp->rcode != NOERROR || ntohs(hp->ancount)==0)
    {
	TRC(printf("rcode = %d, ancount = %d\n", hp->rcode, hp->ancount));
	switch (hp->rcode)
	{
	case NXDOMAIN:
	    TRC(printf("Host Not Found\n"));
	    break;
	case SERVFAIL:
	    TRC(printf("Try again\n"));
	    break;
	case NOERROR:
	    TRC(printf("No Data\n"));
	    break;
	case FORMERR:
	case NOTIMP:
	case REFUSED:
	default:
	    TRC(printf("unrecoverable error\n"));
	    break;
	}
	Heap$Free(Pvs(heap), ans);
	Heap$Free(Pvs(heap), buf);
	return(-1);
    }
    SEQ_INIT (answer, anssize+1, Pvs(heap));
    cp = ans;
    for(i = 0 ; i < SEQ_LEN(answer) ; i++)
    {
	SEQ_ELEM(answer, i) = *cp;
	cp++;
    }
    Heap$Free(Pvs(heap), ans);
    Heap$Free(Pvs(heap), buf);
    return(1);
}

static string_t DNS_ResIQuery_m (
    DNS_cl	*self,
    const Net_IPHostAddr	*addr	/* IN */,
    uint32_t	class	/* IN */,
    uint32_t	type	/* IN */ 
    /* RETURNS */,
    int32_t *namelen )
{
    /* DNS_st	*st = self->st; */
    
    UNIMPLEMENTED;

    return NULL; /* and keep gcc happy */
}

static int32_t res_mkquery(
    int op,
    const char *dname,
    int class, 
    int type,
    const char *data,
    int datalen,
    const char *newrr_in,
    char *buf,
    int buflen,
    DNS_st *st 
    )
{
    register header *hp;
    register char *cp;
    register int n = 0;
    char *dnptrs[20], **dpp, **lastdnptr;
    
/* initialise header fields */

    if ((buf == NULL) || (buflen < HFIXEDSZ))
	return (-1);
    
    bzero(buf, HFIXEDSZ);
    hp = (header *) buf;    
    hp->id = htons(st->id);
    st->id +=1 ;
    hp->opcode = op;
    hp->pr = 1;
    hp->rd = 1;
    hp->rcode = NOERROR;
    cp = buf + HFIXEDSZ;
    dpp = dnptrs;
    *dpp++ = buf;
    *dpp++ = NULL;
    lastdnptr = dnptrs + sizeof dnptrs / sizeof dnptrs[0];
/* perform op codes */
    
    
    switch(op) {
    case QUERY:
	if ((buflen -= QFIXEDSZ) < 0)
	    return(-1);

/* compress domain names */
	
	if (( n = dn_comp(dname, cp, buflen, dnptrs, lastdnptr)) < 0)  
	    return(-1); 
	cp += n;
	buflen -= n;  
	putshort(type, cp);
	cp += INT16SZ;
	putshort(class, cp);
	cp += INT16SZ;
	hp->qdcount = htons(1);

/* check for data (ie it is an inverse query(!!! not implimented yet!)) */ 
	if(op == QUERY || data == NULL)
	{
	    break;
	} 
	buflen -= RRFIXEDSZ;
	n = dn_comp((char *)data, cp, buflen, dnptrs, lastdnptr);
	if (n < 0) 
	    return(-1);
	cp += n;
	buflen -= n;
	putshort(T_NULL, cp);
	cp += INT16SZ;
	putshort(class, cp);
	cp += INT16SZ;
	putlong(0, cp);
	cp += INT32SZ;
	putshort(0, cp);
	cp += INT16SZ;
	hp->arcount = htons(1);
	break;
    };
    return (cp - buf);
}

static int res_send(
    const char *buf,
    int buflen,
    char *ans,
    int anssiz,
    DNS_st *st
    )
{
    header *hp = (header *) buf;
    header *anhp = (header *) ans;
    int s= -1;
    fd_set dsmask;
    struct timeval tv;
    struct sockaddr_in from;
    register int n;
    int try, resplen, fromlen, ns;
    int port = 1025;
    struct sockaddr_in ourside;

    TRC(printf("st->nscount = %i\n", st->nscount));

/* set up socket for our side */
        
    ourside.sin_family = AF_INET;
    ourside.sin_port = htons(port);
    ourside.sin_addr.s_addr = INADDR_ANY;

/* for each retry and name server in the list, try and get an answer */
    
    for (try = 0 ; try < st->retry ; try++) 
    {
	for (ns = 0 ; ns < st->nscount ; ns++) 
	{
	    struct sockaddr_in *nsap = &st->nsaddr_list[ns];
	same_ns:    
	    s = socket(AF_INET, SOCK_DGRAM, 0);
	    if (s < 0)
	    {
		TRC(printf("socket() failed!\n"));
		return(-1);
	    }
	    while (bind (s, (struct sockaddr *)&ourside, sizeof(struct sockaddr))<0)
	      {
		port+=1;
		ourside.sin_port = htons(port);
	      }
	    if (sendto(s, buf, buflen, 0, (struct sockaddr *)nsap, sizeof(struct sockaddr)) != buflen)  
	    {
		TRC(printf("Send error!!\n"));
		if (shutdown(s,1)< 0)
		{
		    TRC(printf("error closing socket"));
		}
		return(-1);
	    }
	    TRC(printf("sent message\n"));

/* timeout stuff */
	   
	    tv.tv_sec = (st->retrans << try);
	    if (try > 0)
		tv.tv_sec /= st->nscount;
	    if((long)tv.tv_sec <=0)
		tv.tv_sec = 1;
	    tv.tv_usec = 0;
	wait:	      
	    FD_ZERO(&dsmask);
	    FD_SET(s, &dsmask);

/* check for answers */

	    n = select(s+1, &dsmask, (fd_set *) NULL, (fd_set *) NULL, &tv);
	    if (n < 0)
	    {
		TRC(printf("select error!!!\n"));
		goto next_ns;
	    }
	    if (n == 0)
	    {
		TRC(printf("timed-out!\n"));
		goto next_ns;
	    }

/* we got an answer! */

	    fromlen = sizeof(struct sockaddr_in);
	    resplen = recvfrom(s, ans, anssiz, 0, (struct sockaddr *)&from, &fromlen);
	    TRC(printf("recieved answer\n"));
	    TRC(printf("erro code %x\n", anhp->rcode));
	    TRC(printf("ancount %i\n", ntohs(anhp->ancount)));

/* check that the answer is correctly formed and contains no intial errors */

	    if (resplen <= 0)
	    {
		TRC(printf("resplen error!\n"));
		goto next_ns;
	    }
	    if (hp->id != anhp->id)
	    {
		TRC(printf("old answer! ignoring\n"));
		goto wait;
	    }
	    if (!our_server(&from, st))
	    {
		TRC(printf("Not our server!!!\n"));
		goto wait;
	    }
 	    if (!queries_match(buf, buf + buflen, ans, ans + anssiz)) 
 	    { 
 		TRC(printf("wrong query name!!\n")); 
 		goto wait; 
	    }
	    if (anhp->rcode == SERVFAIL ||
		anhp->rcode == NOTIMP ||
		anhp->rcode == REFUSED)
	    {
		TRC(printf("server rejected query!!\n"));
		goto next_ns;
	    }
	    if(anhp->tc)
	    {
		TRC(printf("truncated answer!\n"));
		goto same_ns;
	    }

/* close the socket after correct use and return */

	    if (shutdown(s,1)< 0)
	    {
		TRC(printf("error closing socket"));
	    }
	    return(resplen);

/* error on this nameserver, close and move to next nameserver */

	next_ns:
	    if (shutdown(s,1)< 0)
	    {
		TRC(printf("error closing socket"));
	    }
	}   
    }
    return(-1);
}


static int dn_comp(
    const char *exp_dn,
    char *comp_dn,
    int length,
    char **dnptrs,
    char **lastdnptr )
{
    register char *cp, *dn;
    register int c, l;
    char **cpp, **lpp, *sp, *eob;
    char *msg;

/* compress the domain name taken from glibc libraries */
    
    dn = (char *) exp_dn;
    cp = comp_dn;
    eob = cp + length;
    lpp = cpp = NULL;
    if (dnptrs != NULL)
    {
	if ((msg = *dnptrs++) != NULL)
	{
	    for (cpp = dnptrs ; *cpp != NULL ; cpp++)
		;
	    lpp = cpp;
	}
    } else
	msg = NULL;
    for (c = *dn++ ; c != '\0'; )
    {
	if (msg != NULL)
	{
	    if ((l = dn_find(dn-1, msg, dnptrs, lpp)) >= 0)
	    {
		if (cp+1 >= eob)
		    return(-1);
		*cp++ = (l >> 8) | INDIR_MASK;
		*cp++ =  l % 256 ; 
		return(cp - comp_dn);
	    }
	    if (lastdnptr != NULL && cpp < lastdnptr - 1)
	    {
		*cpp++ = cp;
		*cpp = NULL;
	    }
	}
	sp = cp++;
	do
	{
	    if (c == '.')
	    {
		c = *dn++;
		break;
	    }
	    if (c == '\\')
	    {
		if ((c = *dn++) == '\0')
		    break;
	    }
	    if (cp >= eob)
	    {
		if (msg != NULL)
		    *lpp = NULL;
		return(-1);
	    }
	    *cp++ = c;
	} while ((c = *dn++) != '\0');
	if ((l = cp - sp - 1) == 0 && c == '\0')
	{
	    cp--;
	    break;
	}
	if (l <= 0 || l > MAXLABEL)
	{
	    if (msg !=NULL)
		*lpp = NULL;
	    return(-1);
	}
	*sp = l;
    }
    if (cp >= eob)
    {
	if (msg != NULL)
	    *lpp = NULL;
	return(-1);
    }
    *cp++ = '\0';
    return(cp - comp_dn);
}

static int dn_find(
    char *exp_dn,
    char *msg,
    char **dnptrs,
    char **lastdnptr )
{
    register char *dn, *cp, **cpp;
    register int n;
    char *sp;
    
    for (cpp = dnptrs ; cpp < lastdnptr ; cpp ++)
    {
	dn = exp_dn;
	sp = cp = *cpp;
	while ((n = *cp++))
	{
	    switch (n & INDIR_MASK)
	    {
	    case 0:
		while (--n >= 0)
		{
		    if (*dn == '.')
			goto next;
		    if (*dn == '\\')
			dn++;
		    if (mklower(*dn++) != mklower(*cp++))
			goto next;
		}
		if ((n = *dn++) == '\0' && *cp == '\0')
		    return(sp - msg);
		if (n == '.')
		    continue;
		goto next;
	    case INDIR_MASK:
		cp = msg + (((n & 0x3f) << 8) | *cp);
		break;
	    default:
		return(-1);
	    }
	}
	if (*dn == '\0')
	    return(sp - msg);
    next:  ;
    }
    return(-1);
}

static int mklower(
    int ch )
{
    if (isalpha(ch) && isupper(ch))
	return (tolower(ch));
    return(ch);
}

static uint16_t getshort(
    const char *cp)
{
    register char *t_cp = (char *)(cp);
    
    cp += INT16SZ;
    return (((uint16_t)t_cp[0] <<8) | ((uint16_t)t_cp[1]));
}

static void putshort(
    uint16_t s,
    char *cp )
{
    register uint16_t t_s = (uint16_t)(s);
    register char *t_cp = (char *) cp;
    *t_cp++ = t_s >> 8;
    *t_cp = t_s;
    (cp) += INT16SZ;
}

static void putlong(
    uint32_t l,
    char *cp )
{
    register uint32_t t_l = l;
    register char *t_cp = cp;

    *t_cp++ = t_l >> 24;
    *t_cp++ = t_l >> 16;
    *t_cp++ = t_l >> 8;
    *t_cp = t_l;
    (cp) += INT32SZ;
}

static int our_server(
    const struct sockaddr_in *inp,
    DNS_st *st
    )
{
    struct sockaddr_in ina;
    register int ns, ret;
    
    ina = *inp;
    ret = 0;
    for(ns = 0 ; ns < st->nscount ; ns++)
    {
	register const struct sockaddr_in *svr = &st->nsaddr_list[ns];
	
	if(svr->sin_family == ina.sin_family &&
	   svr->sin_port == ina.sin_port &&
	   (svr->sin_addr.s_addr == ina.sin_addr.s_addr))
	{
	    ret++;
	    break;
	}
    }
    return(ret);
}

static int queries_match(
    const char *buf1, 
    const char *eom1,
    const char *buf2, 
    const char *eom2 )
{
    register const char *cp = buf1 + HFIXEDSZ;
    int qdcount = ntohs(((header *)buf1)->qdcount);
    
    TRC(printf("qdcount from question = %i\n", qdcount));

    if (qdcount != ntohs(((header*)buf2)->qdcount))
    {
	TRC(printf("qdcount error!!\n"));
	TRC(printf("qdcount from answer = %i\n", (ntohs(((header*)buf2)->qdcount))));
	return(0);
    }
    while(qdcount-- > 0)
    {
	char tname[MAXDNAME+1];
	register int n, ttype, tclass;
	
	n = dn_expand(buf1, eom1, cp, tname, sizeof tname);
	if (n < 0)
	{
	    TRC(printf("dn_expand error\n"));
	    return(-1);
	}
	cp += n;
	ttype = getshort(cp);
	cp += INT16SZ;
	tclass = getshort(cp);
	cp += INT16SZ;
	if(!name_in_query(tname, ttype, tclass, buf2, eom2))
	{
	    TRC(printf("name in query error\n"));
	    return(0);
	}
    }
    return(1);
}

static int name_in_query(
    const char *name, 
    int type, 
    int class, 
    const char *buf, 
    const char *eom )
{
    register const char *cp = buf + HFIXEDSZ;
    int qdcount = ntohs(((header *)buf)->qdcount);
    
    while (qdcount-- > 0)
    {
	char tname[MAXDNAME + 1];
	register int n, ttype, tclass;
	
	n = dn_expand(buf, eom, cp, tname, sizeof tname);
	if (n < 0)
	    return(-1);
	cp += n;
	ttype = getshort(cp);
	cp += INT16SZ;
	
	tclass = getshort(cp);
	cp += INT16SZ;
      	if (ttype == type &&
	    tclass == class &&
	    strcasecmp(tname, name) == 0)
	    return(1);
    }
    return(0);
}

static int dn_expand(
    const char *msg,
    const char *eomorig,
    const char *comp_dn,
    char *exp_dn,
    int length)
{
    register const char *cp;
    register char *dn;
    register int n, c;
    char *eom;
    int len = -1, checked = 0;

/* expand the domain name from the response */

    dn = exp_dn;
    cp = comp_dn;
    eom = exp_dn + length;
    
    while ((n = *cp++))
    {
	switch (n & INDIR_MASK)
	{
	case 0:
	    if (dn != exp_dn)
	    {
		if (dn >= eom)
		    return(-1);
		*dn++ = '.';
	    }
	    if (dn + n >= eom)
		return(-1);
	    checked += n + 1;
	    while (--n >= 0)
	    {
		if (((c = *cp++) == '.') || (c == '\\'))
		{
		    if (dn + n + 2 >= eom)
			return(-1);
		    *dn++ = '\\';
		}
		*dn++ = c;
		if (cp >= eomorig)
		    return (-1);
	    }
	    break;
	case INDIR_MASK:
	    if(len < 0)
		len = cp - comp_dn + 1;
	    cp = msg + (((n & 0x3f) << 8) | (*cp & 0xff));
	    if (cp < msg || cp >= eomorig)
		return(-1);
	    checked += 2;
	    if (checked >= eomorig - msg)
		return (-1);
	    break;
	default:
	    return(-1);
	}
    }
    *dn = '\0';
    if (len < 0)
	len = cp - comp_dn;
    return (len);
}



#ifdef DEBUG
static void pkt_dump(header *p)
{
    unsigned char *c;

    printf("resolver packet dump:\n");
    printf(" + id=%d query/resp=%c opcode=%d authAns=%d\n",
	   ntohs(p->id), p->qr? 'R' : 'Q', p->opcode, p->aa);
    printf(" + trunc=%d, recurDesired=%d, recurAvail=%d, primaryServ=%d, "
	   "unused=%d, rcode=%d\n",
	   p->tc, p->rd, p->ra, p->pr, p->unused, p->rcode);
    printf(" + queryCount=%d, answerCount=%d, authCount=%d, addRsrcCount=%d\n",
	   ntohs(p->qdcount), ntohs(p->ancount),
	   ntohs(p->nscount), ntohs(p->arcount));

    for(c = (char *)p; c < p+1; c++)
    {
	printf("%02x ", *c);
	if (((unsigned long)c & 15) == 15)
	    printf("\n");
    }
}
#endif /* DEBUG */


/*
 * End 
 */
