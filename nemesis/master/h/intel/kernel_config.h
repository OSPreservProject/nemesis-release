/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      Configuration parameters
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Kernel configuration parameters for Alpha Nemesis
** 
** ENVIRONMENT: 
**
**      Kernel and beyond
** 
** ID : $Id: kernel_config.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
**
*/

#ifndef _kernel_config_h_
#define _kernel_config_h_

#define CFG_NUM_EPS	(64)	/* No. of channel end points 	*/
#define CFG_EVFIFO_SIZE CFG_NUM_EPS 
#define CFG_EVFIFO_MASK (CFG_NUM_EPS-1)

#define CFG_CONTEXTS	(32)	/* Number of domain's contexts		*/
#define CFG_STACKSIZE	(8192)	/* Default user stack size in bytes	*/

#define CFG_DOMAINS	(64)	/* Max. number of domains	*/
#define CFG_UNBLOCK_QUEUE_SIZE	CFG_DOMAINS

#endif /* _kernel_config_h_ */
