/*
 * FILE		:	geometry.h
 * AUTHOR	:	Ralph Becket
 * DATE		:	Mon Mar 15 15:15:31 GMT 1993

 * Geometry routines for 3D rendering.

 * Routines for rotation matrix generation, matrix multiplication,
 * matrix-vertex multiplication, translation, scaling, projection
 * and clipping are provided.

 */



#ifndef __geometry_h__
#define __geometry_h__



#include "geometry_types.h"		/* 3D stuff... */



/* Constants... */

#define FARPOINT	-1000000L	/* Out-of-sight clipping distance */
#define LEEWAY		50		/* Clipping boundary around screen */



/* Addition and subtraction of vectors (v = u+w) */

void GeomAddV(vector *v, vector *u, vector *w);

void GeomSubtractV(vector *v, vector *u, vector *w);



/*
 * DEFINING A 3D ROTATION MATRIX
 *	We define all positive rotation to be clockwise along the axis.
 *	The x and y axes are clockwise in that order viewed along the
 *	z-axis.

 *	theta	-	rotation about the y-axis
 *	phi	-	rotation about the x-axis
 *	gamma	-	rotation about the z-axis

 *	Rotation occurs anticlockwise in this order
 *	i.e. about the y axis first, then x, then z:
 *		y z
 *		|/
 *		+--x

 *	NOTE: vertices are stored as _column_ vectors.
 */



/* set up a 3D rotation matrix */

void GeomGenRotMatrix(matrix *m, rvector *rv);



/* Multiply two matrices together (q = mn) */

void GeomMultiplyM(matrix *q, matrix *m, matrix *n);



/* Multiply a vector by a matrix (v = mu) */

void GeomMultiplyV(vertex *v, matrix *m, vector *u);



/* Multiply a vset by a matrix (vs = m.us) */

void GeomMultiplyVs(vset *vs, matrix *m, vset *us);



/* Translate a vset by a vector (v[] = v[]+u) */

void GeomTranslateVs(vset *vs, vector *d);



/* Scale a vset */

void GeomScaleVs(vset *vs, fix scale);



/*
 * CLIPPING TO VIEW
 *	The clipping mechanism here defines the eye-point to be at (0,0,0)
 *	and the screen centre to be at (0,0,zs) perpendicular to the viewer.
 *	The screen distance zs is changable by the user.
 *	The screen width and height is defined by sx and sy.
 *	Clipping is within a closed rectangular frustum defined by the
 *	eye-point and the corners of the screen (defined by sx and sy, also
 *	user alterable) and the eye-perpendicular plane at (0,0,FARPOINT).

 *	The clipping function returns 1 if a given vertex is within
 *	the clipping frustum, 0 otherwise.
 */

/* Return 1 if v is in the clipping frustum, 0 otherwise */

int GeomInView(vertex *v, vector *g);



/* Routines to project vertex and vset */

void GeomProjectV(pixel *p, vertex *v, vector *g);


void GeomProjectVs(pixel pixels[], vset *vs, vector *g);



#endif
