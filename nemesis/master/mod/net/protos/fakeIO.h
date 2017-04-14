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
**      fake IO header file
** 
** FUNCTIONAL DESCRIPTION:
** 
**      definitions common to all protocols, #include'ed by them.
** 
** ENVIRONMENT: 
**
**      Protocol modules
** 
** ID : $Id: fakeIO.h 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

#ifndef _FAKEIO_H_

#define _FAKEIO_H_

/* RX action codes: */
#define AC_TOCLIENT	0	/* send data towards client */
#define AC_DITCH	1	/* ditch data & recycle buffers */
#define AC_WAIT		2	/* wait for more data to arrive */

#endif /* _FAKEIO_H_ */
