/*
   SICS adaptation
   ID: $Id: icmp_var.h 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
*/

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *
 *	@(#)icmp_var.h	8.1 (Berkeley) 6/10/93
 * Id: icmp_var.h,v 1.4.8.1 1997/08/25 16:33:02 wollman Exp 
 */

#ifndef _NETINET_ICMP_VAR_H_
#define _NETINET_ICMP_VAR_H_

/*
 * Variables related to this implementation
 * of the internet control message protocol.
 */
struct	icmpstat {
/* statistics related to icmp packets generated */
	uint32_t	icps_error;		/* # of calls to icmp_error */
	uint32_t	icps_oldshort;		/* no error 'cuz old ip too short */
	uint32_t	icps_oldicmp;		/* no error 'cuz old was icmp */
	uint32_t	icps_outhist[ICMP_MAXTYPE + 1];
/* statistics related to input messages processed */
 	uint32_t	icps_badcode;		/* icmp_code out of range */
	uint32_t	icps_tooshort;		/* packet < ICMP_MINLEN */
	uint32_t	icps_checksum;		/* bad checksum */
	uint32_t	icps_badlen;		/* calculated bound mismatch */
	uint32_t	icps_reflect;		/* number of responses */
	uint32_t	icps_inhist[ICMP_MAXTYPE + 1];
	uint32_t	icps_bmcastecho; 	/* b/mcast echo requests dropped */
	uint32_t	icps_bmcasttstamp; 	/* b/mcast tstamp requests dropped */
};

/*
 * Names for ICMP sysctl objects
 */
#define	ICMPCTL_MASKREPL	1	/* allow replies to netmask requests */
#define	ICMPCTL_STATS		2	/* statistics (read-only) */
#define ICMPCTL_MAXID		3

#define ICMPCTL_NAMES { \
	{ 0, 0 }, \
	{ "maskrepl", CTLTYPE_INT }, \
	{ "stats", CTLTYPE_STRUCT }, \
}

#ifdef KERNEL
extern struct	icmpstat icmpstat;
#endif

#endif
