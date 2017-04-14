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
**      Nemesis client rendering window system.
** 
** FUNCTIONAL DESCRIPTION:
** 
**      FB driver interface
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: fb.c 1.2 Wed, 28 Apr 1999 11:42:49 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <time.h>
#include <ecs.h>

#include <IOMacros.h>
#include <IDCMacros.h>

#include <stdio.h>

#include <FB.ih>

#include "WSSvr_st.h"

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif
 
#define DB(_x)  _x


/*
 * State 
 */
void FBInit ( WSSvr_st *wst )
{
  FB_st *fbst = &wst->fb_st;
  FB_clp NOCLOBBER fb = NULL;
  uint32_t w, h, xgrid, ygrid, psize, pbits;
  uint64_t protos;
  Type_Any fb_any;

  TRC(printf ("WS: FBInit: binding to dev>FB\n"));

  while(!Context$Get(Pvs(root), "dev>FB", &fb_any))
      PAUSE(MILLISECS(100));

  TRY {
      fb = IDC_OPEN ("dev>FB", FB_clp);
  } CATCH_ALL {
      fprintf (stderr, "WS: open of dev>FB failed.\n");
      Threads$Exit(Pvs(thds));
  } ENDTRY;

  TRC(printf ("WS: FBInit: done\n"));

  w = FB$Info(fb, &h, &xgrid, &ygrid, &psize, &pbits, &protos);
  TRC(printf ("WS: FBInit: fb is %dx%d\n", w, h));

  fbst->fb     = fb;
  fbst->width  = w;
  fbst->height = h;
  fbst->depth = psize;
  fbst->pbits = pbits;
  fbst->cursor = 0;
  fbst->xgrid = xgrid;
  fbst->ygrid = ygrid;
  fbst->protos = protos;
  fbst->xppwmask = ~(xgrid - 1);
  fbst->yppwmask = ~(ygrid - 1);
}

