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
 *	@(#)$RCSfile: tcpip.h,v $ $Revision: 1.1 $ (DEC) $Date: Thu, 18 Feb 1999 14:19:49 +0000 $
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
 *	Base:	tcpip.h	7.3 (Berkeley) 6/29/88
 *	Merged:	tcpip.h	7.4 (Berkeley) 6/28/90
 */

#include "ip_var.h"
#include "ip.h"

#ifndef _NETINET_TCPIP_H_
#define _NETINET_TCPIP_H_
/*
 * Tcp+ip header, after ip options removed.
 */



struct tcpiphdr {
    union {
	uint8_t     ethhdr[16];
	struct {
	    addr_t next;
	    addr_t prev;
	} link;
    } ti_eth;
    struct 	ip     ti_i;		/* overlaid ip structure */
    struct	tcphdr ti_t;		/* tcp header */
};

#define ti_next         ti_eth.link.next
#define ti_prev         ti_eth.link.prev

#define	ti_x1		ti_i.ip_ttl
#define	ti_pr		ti_i.ip_p
#define	ti_len	        ti_i.ip_sum
#define	ti_src		ti_i.ip_src
#define	ti_dst		ti_i.ip_dst
#define	ti_sport	ti_t.th_sport
#define	ti_dport	ti_t.th_dport
#define	ti_seq		ti_t.th_seq
#define	ti_ack		ti_t.th_ack
/*#if	defined(_KERNEL) || defined(_NO_BITFIELDS) || (__STDC__ == 1)*/
#if 0
#define	ti_xoff		ti_t.th_xoff
#else
#define	ti_x2		ti_t.th_x2
#define	ti_off		ti_t.th_off
#endif
#define	ti_flags	ti_t.th_flags
#define	ti_win		ti_t.th_win
#define	ti_sum		ti_t.th_sum
#define	ti_urp		ti_t.th_urp
#endif

/*  RS The following is a horrible hack to allow tcp_reass to work properly. */
#ifdef ALPHA
#define TI_LEN ti_dport
#undef ti_prev
#define ti_prev ti_x1
#else
#define TI_LEN ti_len
#endif
