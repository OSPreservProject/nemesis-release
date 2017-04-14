/*
*****************************************************************************
**                                                                          *
**  Copyright 1999, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	
**
** FUNCTIONAL DESCRIPTION:
**
**	
**
** ENVIRONMENT: 
**
**
**
** ID : $Id: RSSMonitorMod.c 1.1 Thu, 10 Jun 1999 14:42:18 +0100 dr10009 $
**
**
*/

#include <nemesis.h>

#include <RSSMonitorMod.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);


static	RSSMonitorMod_New_fn 		RSSMonitorMod_New_m;

static	RSSMonitorMod_op	RSSMonitorMod_ms = {
  RSSMonitorMod_New_m
};

/* export a stateless closure */
const static	RSSMonitorMod_cl	stateless_RSSMonitorMod_cl = { &RSSMonitorMod_ms, NULL };
CL_EXPORT (RSSMonitorMod, RSSMonitorMod, stateless_RSSMonitorMod_cl);


/*---------------------------------------------------- Entry Points ----*/

RSSMonitor_clp create(bool_t export);

static RSSMonitor_clp RSSMonitorMod_New_m (
	RSSMonitorMod_cl	*self,
	bool_t	export	/* IN */ )
{
  return create(export);
}

/*
 * End 
 */
