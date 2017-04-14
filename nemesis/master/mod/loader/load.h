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
**      h/load.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      loader library
** 
** ENVIRONMENT: 
**
**      Any domain.
** 
** ID : $Id: load.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef _load_h_
#define _load_h_

#include <stdio.h>
#include <Load.ih>

extern Stretch_clp Load_FromStream (FILE	*stream,
				    Context_clp  imps,
				    Context_clp  exps,
				    ProtectionDomain_ID new_pdom,
				    Stretch_clp *data);
/* RAISES Load_Bad */


#endif /* _LOAD_H_ */


