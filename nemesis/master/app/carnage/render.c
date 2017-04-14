/*
 * FILE		:	render.c
 * AUTHOR	:	Ralph Becket
 * DATE		:	Mon Apr  5 17:41:58 BST 1993

 * These functions render a polyhedron into an array of segments
 * Required information is
 *	a polyhedron structure
 *	a scaling factor
 *	a position vector for the polyhedron
 *	a local rotation vector for the polyhedron
 *	a position vector for the camera
 *	a rotation matrix for the camera view
 *      an array of segments
 *
 * If the polyhedron is visible, then it will be scaled, transformed and
 * have its visible faces drawn inside the X window with the given
 * graphics context.

 */

#include <nemesis.h>
#include <WS.h>

#include "geometry_types.h"		/* 3D stuff... */

#include "Carnage_st.h"

#include "geometry.h"
#include "render.h"



#ifdef TRACE_RENDER
#include "stdio.h"
#define TRC(x)	{x}
#else
#define TRC(x)	{}
#endif

/* Prototypes... */

static int RenderFaceVisible(pixel *pixels, face *f);



/* Returns no of segments to draw */

int  RenderPoly( st,
		 poly,
		 polyScale,
		 polyPosV,
		 polyRotV,
		 cameraPosV,
		 cameraRotM,
		 segments,
		 geom
	       )
    
        Carnage_st              *st;
	polyhedron		*poly;
	fix			polyScale;
	vector			*polyPosV;
	rvector			*polyRotV;
	vector			*cameraPosV;
	matrix			*cameraRotM;
        CRend_Segment                 *segments;
	vector                  *geom;
{
	matrix		polyRotM;
	vector		position;

	/* This stuff for manipulation of the object vset */
	vertex		vArray[MAX_VERTICES];
	vset		vs = {MAX_VERTICES, vArray};
	vector		*v;

#define ISLINEDONE(r, c, n)	(st->lineDone[n*r + c])
#define SETLINEDONE(r, c, n)	{st->lineDone[n*r + c] = 1;}
#define CLEARLINEDONE(n)	{memset((void*)&st->lineDone[0], (int)0, (word_t)(n*n*sizeof(int32_t)));}

	/* This stuff to store projected vertices in */
	pixel		        pixels[MAX_VERTICES];
	pixel			*p;
	CRend_Segment			*s = segments; 

	uint32_t		n = poly->vs->n;
	face			*f;
	int			i;


	s = segments;

TRC(printf("RenderPoly: entered\n");)
	/*
	 * Is the object in sight?
	 */

	/* Move object centre relative to camera */

TRC(printf("RenderPoly: position is          (%d, %d, %d)\n", position.x, position.y, position.z);)

	GeomSubtractV(&position, polyPosV, cameraPosV);
TRC(printf("RenderPoly: after translation is (%d, %d, %d)\n", position.x, position.y, position.z);)

	/* Rotate the object centre into place */

	GeomMultiplyV(&position, cameraRotM, &position);
TRC(printf("RenderPoly: after rotation is    (%d, %d, %d)\n", position.x, position.y, position.z);)

	/* Is the object centre in view? */

	if(!GeomInView(&position, geom))
	{
TRC(printf("RenderPoly: object is out of sight; exiting\n");)
		return 0;		/* Quit if out of sight */
	}


TRC(printf("RenderPoly: object is visible\n");)
	/*
	 * If so, go to town rendering it.
	 */

	/* Copy the vset for the object (for manipulation.) */

	vs.n = n;
	memcpy((void *)vs.v, 
	       (void *)poly->vs->v,
	       (word_t)(n * sizeof(vertex)));

	/* Scale the vset */

	if(polyScale != 1024)
		GeomScaleVs(&vs, polyScale);

	/* Generate the local rotation matrix */

	GeomGenRotMatrix(&polyRotM, polyRotV);

	/* Get it to perform the global rotation as well */

	GeomMultiplyM(&polyRotM, cameraRotM, &polyRotM);

	/* Rotate the vset */

	GeomMultiplyVs(&vs, &polyRotM, &vs);

	/* Translate the vset into position */

	GeomTranslateVs(&vs, &position);

TRC(printf("RenderPoly: vset transformed\n");)
	/*
	 * The poly's vset is now correctly transformed.
	 * Next we decide which edges to display, based
	 * upon visibility of faces.  For speed, we only
	 * draw each edge once.
	 */

	/* Project the vset onto the viewplane */

	GeomProjectVs(pixels, &vs, geom);

	/* Clear the part of the lineDone matrix we are going to use */

	CLEARLINEDONE(n);

	/*
	 * Go through each face.  If it is visible, add lines which
	 * have not already been accounted for to the drawing list.
	 */

	for(f = &(poly->fs->f[0]); f < &(poly->fs->f[poly->fs->n]); f++)
	{
		uint32_t	pa, pb;	/* Vertex no's of line start & end */


		/* Go on to next face if not visible */

		if(!RenderFaceVisible(pixels, f))
		{
TRC(printf("RenderPoly: face %d is not visible\n", f-&(poly->fs->f[0]));)
			continue;
		}
TRC(printf("RenderPoly: face %d is visible\n", f-&(poly->fs->f[0]));)

		/* Add new lines to the drawing list */

		for(i = 0; i <= f->n-1; i++)
		{
			/* Find start & end points */
			/* Remeber: there is a line between first and last */

			pa = f->p[i];
			pb = (i < f->n-1) ? f->p[i+1] : f->p[0];

			/* Go on to next line if this has been drawn */

			if(ISLINEDONE(pa, pb, n))
				continue;

			/* Otherwise add this line to the list */

			SETLINEDONE(pa, pb, n);
			SETLINEDONE(pb, pa, n);

			s->x1 = pixels[pa].x;
			s->y1 = pixels[pa].y;
			s->x2 = pixels[pb].x;
			s->y2 = pixels[pb].y;
			TRC(printf("RenderPoly: added line (%d, %d) - (%d, %d)\n", s->x1, s->y1, s->x2, s->y2);)
			  
			s++;
		}
	}
    
TRC(printf("RenderPoly: exiting\n");)

	return s - segments;
}



/*
 * Find the z-component of the cross product of a face to decide
 * whether or not it faces us.  The cross product is given by the
 * equation

 *	(x0,y0,z0) x (x1,y1,z1) =
 *
 *	| i  j  k|
 *	|x0 y0 z0| =
 *	|x1 y1 z1|
 *
 *	((y0z1-z0y1), -(x0z1-z0x1), (x0y1-y0x1))

 * However, we only want the z-component, (x0y1-y0x1).  Furthermore,
 * we have to use the projected points since perspective may still
 * cause a face to be hidden.
 */

static int RenderFaceVisible(pixels, f)
	pixel	*pixels;
	face	*f;
{
	int32_t	x0, y0;
	int32_t	x1, y1;

	
	if(f->n < 3)			/* Less than 3 lines always visible */
		return 1;

	x0 = pixels[f->p[1]].x - pixels[f->p[0]].x;
	y0 = pixels[f->p[1]].y - pixels[f->p[0]].y;

	x1 = pixels[f->p[2]].x - pixels[f->p[1]].x;
	y1 = pixels[f->p[2]].y - pixels[f->p[1]].y;

TRC(printf("RenderFaceVisible: z component of face x product is %4d", (x0*y1 - y0*x1));)
TRC(printf(" (%svisible)\n", (x0*y1 - y0*x1) < 0 ? "" : "in");)
	return (x0*y1 - y0*x1) < 0;
}
