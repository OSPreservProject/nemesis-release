/*
*****************************************************************************
**                                                                          *
**  Copyright 1993, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      Carnage state record.
** 
** FUNCTIONAL DESCRIPTION:
** 
**      State for the Lance driver interrupt stub.
** 
** ENVIRONMENT: 
**
**      Internal to module Carnage
** 
** ID : $Id: Carnage_st.h 1.1 Tue, 20 Apr 1999 17:17:47 +0100 dr10009 $
** 
** LOG: $Log: Carnage_st.h,v $
** LOG: Revision 1.5  1997/04/18 13:25:16  pbm1001
** LOG: Use CRend
** LOG:
** LOG: Revision 1.4  1997/04/09 21:22:16  pbm1001
** LOG: Added lineDone array to state
** LOG:
** LOG: Revision 1.3  1997/01/15 13:57:57  pbm1001
** LOG: Show FPS, draw updates properly
** LOG:
** LOG: Revision 1.2  1997/01/05 18:26:40  pbm1001
** LOG: Removed static data, handled expose eventsd
** LOG:
 * Revision 1.1  1995/02/27  14:23:00  dme
 * Initial revision
 *
 * Revision 1.1  1995/02/20  14:42:02  prb12
 * Initial revision
 *
**
*/

#ifndef _Carnage_st_h_
#define _Carnage_st_h_

#include "geometry_types.h"

#include <CRend.ih>
#include <CRendDisplay.ih>
#include <CRendDisplayMod.ih>
#include <WS.ih>

/*
 * Carnage  state
 */

typedef struct {
  CRendDisplay_clp display;
  CRend_clp       window;
  uint32_t        frame;
  fix		  viper_scale;
  vector          viper_posv;
  rvector         viper_rotv;
  vector          camera_posv;
  rvector         camera_rotv;
  matrix          camera_rotm;
  CRend_Segment         segs[2][MAX_EDGES];
  uint32_t        nsegs[2];
  uint32_t        segbuffer;
  SRCThread_Mutex mu;
  vector          geom;
  uint32_t        corner_x, corner_y;
  word_t          frames_last_second;
  Time_T          last_second;
  int32_t         lineDone[MAX_VERTICES * MAX_VERTICES];
} Carnage_st;

#endif /* _Carnage_st_h_ */
