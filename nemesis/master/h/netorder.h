/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	netorder.h
** 
** DESCRIPTION:
** 
**	Swapping endian to and from Network order.
**
**
** ID : $Id: netorder.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef __netorder_h__
#define __netorder_h__

#if	defined(BIG_ENDIAN)
#  include "bigEndian.h"
#elif	defined(LITTLE_ENDIAN)
#  include "littleEndian.h"
#else
#error YOU HAVE NOT DEFINED THE ENDIANISM!
#endif

/* some useful macros for network gumf on alpha */
#define ALIGN4(_x) (((word_t)(_x)+3)&(~3))
#define ALIGN_ETH(_x) (ALIGN4((word_t)_x)|2)

/* 
 * hysterical compatibility
 */
#define htons(x) hton16(x)
#define ntohs(x) ntoh16(x)
#define htonl(x) hton32(x)
#define ntohl(x) ntoh32(x)

#endif /* __netorder_h__ */
