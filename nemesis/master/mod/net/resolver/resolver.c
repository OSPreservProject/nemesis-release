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
#include <contexts.h>
#include <resolver.h>
#include <ResolverMod.ih>
#include <Resolver.ih>
#include <DNSMod.ih>
#include <DNS.ih>
#include <Net.ih>
#include <Closure.ih>
#include <bstring.h>
#include <socket.h>
#include <stdio.h>

//#define DEBUG

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);


typedef struct {
    Resolver_cl cl;
    DNS_clp dnsp;
} Resolver_st;

static	Resolver_Resolve_fn 		Resolver_Resolve_m;

static	Resolver_op	Resolver_ms = {
  Resolver_Resolve_m
};

static ResolverMod_New_fn               New_m;

static ResolverMod_op   ms = {
    New_m
};

static const	ResolverMod_cl	cl = { &ms, NULL };

CL_EXPORT (ResolverMod, ResolverMod, cl);


/* Other functions that are needed */

static int dn_expand(const char *, const char *, const char *, char *, int);
static uint16_t getshort(const char *);
static uint32_t getlong(const char *);
static int getanswer(const char *, int, Net_IPHostAddrSeqP);
static const char *fname(const char *, const char *, int, char *, int);
/*---------------------------------------------------- Entry Points ----*/

static Resolver_clp New_m (
    ResolverMod_clp  self,
    const Net_IPHostAddr    *hosts   /* IN */ )
{
    Resolver_clp resolverp;
    DNSMod_clp dnsmp;
    Resolver_st *st = Heap$Malloc(Pvs(heap), sizeof(*st));

/* initialise closure and get DNS closure */
    
    resolverp = (Resolver_clp) st;
    CLP_INIT(resolverp, &Resolver_ms, st);
    dnsmp = NAME_FIND("modules>DNSMod", DNSMod_clp);
    st->dnsp = DNSMod$New(dnsmp, hosts);
    return(resolverp);
}
    

static bool_t Resolver_Resolve_m (
    Resolver_cl	*self,
    string_t	name	/* IN */
    /* RETURNS */,
    Net_IPHostAddrSeqP	*addr)
{
    Resolver_st	*st = self->st; 
    int answerno;
    int l; 
    Net_IPHostAddrSeqP addresses;
    NOCLOBBER bool_t success = False;
    char *ans = Heap$Malloc(Pvs(heap), MAXPACKET); 
    DNS_OctetSeq answer;
    char *cp;
    int i;

    {
	uint32_t a, b, c, d;
	/* See if it's a dotted quad address */
	
	/* Find a name in the form a.b.c.d followed by nothing (except
           perhaps white space), where each of a,b,c and d are no more
           than 8 bits */

	if((sscanf(name,"%d.%d.%d.%d%*s", &a, &b, &c, &d) == 4) &&
	   ((a|b|c|d) < 256)) {

	    *addr = SEQ_NEW(Net_IPHostAddrSeq, 1, Pvs(heap));
	    SEQ_ELEM(*addr, 0).a = a | (b << 8) | (c << 16) | (d << 24);

	    return True;
	}
    }
	    
    /* query with our name */

    TRY {
	l = DNS$ResQuery(st->dnsp, name, C_IN, T_A, &answer);
	TRC(printf("res_query returned\n"));
	if (l == -1) {
	    fprintf(stderr, "Resolver$Resolver: query for '%s' failed\n");
	} else {
	    success = True;
	}
    } CATCH_ALL {
	fprintf(stderr, "Resolve$Resolve: DNS$ResQuery for '%s' raised '%s'\n",
		name, exn_ctx.cur_exception);
    } ENDTRY;

    /* check for errors, if none then extract the answer */

    if(!success)
    {
	FREE(ans);
	return False;
    }

    addresses = SEQ_NEW(Net_IPHostAddrSeq, 0, Pvs(heap));
    
    cp = ans;
    
    for(i = 0 ; i < SEQ_LEN(&answer); i++)
    {
	*cp = SEQ_ELEM(&answer, i);
	cp++;
    }
    
    answerno = getanswer(ans, MAXPACKET, addresses);
    
    FREE(ans);
    
    if(answerno != -1) {
	*addr = addresses;
	return True;
    } else {
	SEQ_FREE(addresses);
	return False;
    }
}

static uint16_t getshort(
    const char *cp)
{
    register char *t_cp = (char *)(cp);
    
    cp += INT16SZ;
    return (((uint16_t)t_cp[0] <<8) | ((uint16_t)t_cp[1]));
}

static uint32_t getlong(
    const char *cp)
{
    register char *t_cp = (char *)(cp);

    (cp) +=INT32SZ;
    return (((uint32_t)t_cp[0] << 24) | ((uint32_t)t_cp[1] << 16) | ((uint32_t)t_cp[2] << 8) | ((uint32_t)t_cp[3]));
}

static int getanswer(
    const char *ans, 
    int len,
    Net_IPHostAddrSeqP addresses)
{
    register const char *cp, *endmark;
    register const header *hp;
    register int n;
    char *name = Heap$Malloc(Pvs(heap), MAXDNAME);
    int type, class, tmpttl, dlen, i;
    struct in_addr inaddr;
    Net_IPHostAddr address;

/* extracting the answer from the reply from the name server */

    hp = (header*) ans;
    cp = ans + HFIXEDSZ;
    endmark = ans + len;

/* move through the question field to get to where the answers are stored */
    
    if ((n = dn_expand(ans, ans+len, cp, name, MAXDNAME)) <0)
	{
	cp = NULL;
	TRC(printf("error expanding question name!\n"));
	Heap$Free(Pvs(heap), name);
	return(-1);
    }
    else
	cp += n;
    cp += INT16SZ;
    cp += INT16SZ;
    
/* got here, now get the answers */

    n = (ntohs(hp->ancount)); /* get no. answers records */
    SEQ_INIT (addresses, n, Pvs(heap));    
    i = 0;
    while (--n >= 0) /* for each answer record, extract the answer, and put it in the sequence */
    {
	cp = fname(cp, ans, MAXCDNAME, name, MAXDNAME);
	if (!cp)
	{
	    TRC(printf("error expanding answer name!\n"));
	    Heap$Free(Pvs(heap), name);
	    return(-1);
	}

/* need to get type and class details in case the answer was an alias type */

	type = getshort((char *)cp);
	cp += INT16SZ;
	TRC(printf("type = %i\n", type));
	class = getshort((char *)cp);
	cp += INT16SZ;
	TRC(printf("class = %i\n", class));
	tmpttl = getlong((char *)cp);
	cp += INT32SZ;
	dlen = getshort((char *)cp);
	cp += INT16SZ;
	switch (type) 
	{
	case T_A:  /* type was a proper answer */
	{
	    bcopy(cp, &inaddr, INADDRSZ);
	    if (dlen == 4)
		cp += dlen;
	    else if (dlen == 7)
	    {
		char protocol;
		
		cp += INADDRSZ;
		protocol = * (char *)cp;
		cp += sizeof(char);
		cp += INT16SZ;
	    }
	    TRC(printf("the address is %I\n", inaddr.s_addr));

/* put answer into seuqence */

	    address.a = inaddr.s_addr;
	    SEQ_ELEM(addresses, i++) = address;
	    break;
	}
	case T_CNAME: 
/* type was an alias name */
/* XXX Will produce an extra answer containing answer of 0.0.0.0 for each alias */
	{
	    cp = fname(cp, ans, MAXCDNAME, name, MAXDNAME);
	    if (!cp)
	    {
		TRC(printf("error expanding alias name\n"));
		Heap$Free(Pvs(heap), name);
		return(-1);
	    }
	    SEQ_REMH(addresses);
	}
	}
    }
    Heap$Free(Pvs(heap), name);
    return(1);
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
    
    dn = exp_dn;
    cp = comp_dn;
    eom = exp_dn + length;
    
    while ( (n = *cp++) )
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

static const char *fname(
    const char *cp, 
    const char *msg,
    int msglen,
    char *name,
    int namelen)
{
    int n, newlen;
    
    if ((n = dn_expand(msg, cp + msglen, cp, name, namelen)) < 0)
    {
	TRC(printf("error expanding name\n")); 
	return (NULL);
    }
    newlen = strlen(name);
    if (newlen == 0 || name[newlen - 1] != '.')
	if (newlen+1 >= namelen)
	    return (NULL);
	else
	    strcpy(name +newlen, ".");
    return (cp + n);
}


/*
 * End 
 */
