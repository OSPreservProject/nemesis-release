/*
 * *****************************************************************
 * *                                                               *
 * *    Copyright (c) Digital Equipment Corporation, 1991, 1995    *
 * *                                                               *
 * *   All Rights Reserved.  Unpublished rights  reserved  under   *
 * *   the copyright laws of the United States.                    *
 * *                                                               *
 * *   The software contained on this media  is  proprietary  to   *
 * *   and  embodies  the  confidential  technology  of  Digital   *
 * *   Equipment Corporation.  Possession, use,  duplication  or   *
 * *   dissemination of the software and media is authorized only  *
 * *   pursuant to a valid written license from Digital Equipment  *
 * *   Corporation.                                                *
 * *                                                               *
 * *   RESTRICTED RIGHTS LEGEND   Use, duplication, or disclosure  *
 * *   by the U.S. Government is subject to restrictions  as  set  *
 * *   forth in Subparagraph (c)(1)(ii)  of  DFARS  252.227-7013,  *
 * *   or  in  FAR 52.227-19, as applicable.                       *
 * *                                                               *
 * *****************************************************************
 */
/*
 * HISTORY
 */
/*	
 *	@(#)$RCSfile: ip_var.h,v $ $Revision: 1.1 $ (DEC) $Date: Thu, 18 Feb 1999 14:19:49 +0000 $
 */ 
/*
 */
/*
 * (c) Copyright 1990, OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED
 */
/*
 * OSF/1 Release 1.0
 */
/*
 * Copyright (C) 1988,1989 Encore Computer Corporation.  All Rights Reserved
 *
 * Property of Encore Computer Corporation.
 * This software is made available solely pursuant to the terms of
 * a software license agreement which governs its use. Unauthorized
 * duplication, distribution or sale are strictly prohibited.
 *
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	Base:	ip_var.h	7.6 (Berkeley) 9/20/89
 *	Merged:	ip_var.h	7.7 (Berkeley) 6/28/90
 */
#ifndef	_NETINET_IP_VAR_H
#define	_NETINET_IP_VAR_H


#define IPOPT_OFFSET 2

/*
 * Overlay for ip header used by other protocols (tcp, udp).
 * For 64-bit pointers, the next and previous fields are stored in the
 * mbuf header instead of in the ipovly.
 */
struct ipovly {
/*****
#ifdef __alpha
*****/
#ifdef __ALPHA__
	u_int	fill[2];		/* filler */
#else /* __alpha */
	addr_t	ih_next, ih_prev;	/* for protocol sequence q's */
#endif /* __alpha */
	u_char	ih_x1;			/* (unused) */
	u_char	ih_pr;			/* protocol */
	u_short	ih_len;			/* protocol length */
	struct	in_addr ih_src;		/* source internet address */
	struct	in_addr ih_dst;		/* destination internet address */
};




#ifdef _KERNEL
ndecl_lock_data(extern, ip_frag_lock)
#define IPFRAG_LOCKINIT()	ulock_setup(&ip_frag_lock, ip_frag_li, TRUE)
#define IPFRAG_LOCK()		ulock_write(&ip_frag_lock)
#define IPFRAG_UNLOCK()		ulock_done(&ip_frag_lock)

/* flags passed to ip_output as fourth parameter */
#define	IP_FORWARDING		0x1		/* most of ip header exists */
#define	IP_ROUTETOIF		SO_DONTROUTE	/* bypass routing tables */
#define	IP_ALLOWBROADCAST	SO_BROADCAST	/* can send broadcast packets */

extern	CONST u_char inetctlerrmap[];
extern	struct	ipstat	ipstat;
extern	struct	ipq	ipq;			/* ip reass. queue */
extern	u_int	ip_id;				/* ip packet ctr, for ids */
#if NETSYNC_LOCK
#define GET_NEXT_IP_ID() \
	(htons((u_short)((lockmode == 0) ? ip_id++ : atomic_incl(&ip_id))))
#else
#define GET_NEXT_IP_ID() (htons((u_short)ip_id++))
#endif /* NETSYNC_LOCK */

#endif /* _KERNEL */
#endif /* _NETINET_IP_VAR_H */
