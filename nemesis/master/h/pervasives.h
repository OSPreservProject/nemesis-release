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
**      pervasives.h
** 
** DESCRIPTION:
** 
**      Standard definitions for Nemesis.
**      
** 
** ID : $Id: pervasives.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef _pervasives_h_
#define _pervasives_h_

/*
 * Pervasives - the definition of __pvs is in nemesis_tgt.h
 */ 

#include <Pervasives.ih>

#define _PvsMember(_mem)	(((Pervasives_Rec *)__pvs)->_mem)
#define PVS()			__pvs
#define Pvs(_mem)		_PvsMember(_mem)
#endif /* _pervasives_h_ */

