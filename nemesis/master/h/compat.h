/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      h/compat.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      compatibility hacks
** 
** ENVIRONMENT: 
**
**      Any domain.
** 
** ID : $Id: compat.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _compat_h_
#define _compat_h_


/* XXX - compatibility hacks */

#define malloc(x)	Heap$Malloc (Pvs(heap), (x))
#define free(x)		FREE(x)

#define fseek(stream,offset,whence)				\
Rd$Seek ((stream)->rd, (offset) +				\
	 (((whence) == SEEK_SET) ? 0 :				\
	  ((whence) == SEEK_CUR) ? Rd$Index ((stream)->rd) :	\
	  ((whence) == SEEK_END) ? Rd$Length ((stream)->rd) : 0))

#define fread(ptr,sizeofPtr,nmemb,stream) \
(Rd$GetChars((stream)->rd, (int8_t *)(ptr), (nmemb)*(sizeofPtr)) / (sizeofPtr))


#endif /* _compat_h_ */


