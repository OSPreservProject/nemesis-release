/*
 * FILE		:	render.h
 * AUTHOR	:	Ralph Becket
 * DATE		:	Mon Apr  5 17:41:58 BST 1993

 * These functions renders a polyhedron into an array of line segments
 * Required information is
 *	a polyhedron structure
 *	a scaling factor
 *	a position vector for the polyhedron
 *	a local rotation vector for the polyhedron
 *	a position vector for the camera
 *	a rotation matrix for the camera view
 *      an array of segments to render into

 * If the polyhedron is visible, then it will be scaled, transformed and
 * have its visible faces drawn inside the X window with the given
 * graphics context.

 */



#ifndef __render_h__
#define __render_h__

#include "Carnage_st.h"

#include "geometry_types.h"		/* 3D stuff... */

/* Returns 1 if object was rendered, 0 if not (out of sight) */

int  RenderPoly(
                 Carnage_st             *st,
		 polyhedron		*poly,
		 fix			polyScale,
		 vector			*polyPosV,
		 rvector		*polyRotV,
		 vector			*cameraPosV,
		 matrix			*cameraRotM,
		 CRend_Segment                *segments,
		 vector                 *geom
	       );



#endif
