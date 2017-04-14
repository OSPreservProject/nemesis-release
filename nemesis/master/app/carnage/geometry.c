/*
 * FILE		:	geometry.c
 * AUTHOR	:	Ralph Becket
 * DATE		:	Mon Mar 15 15:15:31 GMT 1993

 * Geometry routines for 3D rendering.

 * Routines for rotation matrix generation, matrix multiplication,
 * matrix-vector multiplication, translation, scaling, projection
 * and clipping are provided.

 */

#include <nemesis.h>

#include "geometry_types.h"		/* 3D stuff... typedefs */

#include "geometry.h"


#ifdef TRACE_GEOMETRY
#include "stdio.h"
#define TRC(x)	{x}
#else
#define TRC(x)	{}
#endif


#ifdef TRACE2_GEOMETRY
#include "stdio.h"
#define TRC2(x)	{x}
#else
#define TRC2(x)	{}
#endif


/* Globals... NONE!!!! */


/* Addition and subtraction of vectors (v = u+w) */

void GeomAddV(v, u, w)
	vector	*v;
	vector	*u;
	vector	*w;
{
	v->x = u->x + w->x;
	v->y = u->y + w->y;
	v->z = u->z + w->z;
}


void GeomSubtractV(v, u, w)
	vector	*v;
	vector	*u;
	vector	*w;
{
	v->x = u->x - w->x;
	v->y = u->y - w->y;
	v->z = u->z - w->z;
}



/*
 * DEFINING A 3D ROTATION MATRIX
 *	We define all positive rotation to be clockwise along the axis.
 *	The x and y axes are clockwise in that order viewed along the
 *	z-axis.

 *	theta	-	rotation about the y-axis
 *	phi	-	rotation about the x-axis
 *	gamma	-	rotation about the z-axis

 *	The rotation matrices are as follows:
 *
 *	gamma =
 *		( cg   sg  0  )
 *		(-sg   cg  0  )
 *		(  0   0   1  )
 *	theta =
 *		( ct   0   st )
 *		(  0   1   0  )
 *		(-st   0   ct )
 *	phi =
 *		(  1   0   0  )
 *		(  0  cp  -sp )
 *		(  0  sp   cp )

 *	We define rotation to be anticlockwise in the order theta, phi gamma
 *	i.e. about the y axis first, then x, then z:
 *		y z
 *		|/
 *		+--x
 *	The combined rotation matrix for this is:
 *
 *	R =
 *		(  cg.ct+sg.sp.st       sg.cp        cg.st-sg.sp.ct )
 *		( -sg.ct+cg.sp.st       cg.cp       -sg.st-cg.sp.ct )
 *		( -cp.st                sp           cp.ct          )

 *	NOTE: vertices are stored as _column_ vectors.
 */



/*
 * NOTE: For efficiency, all values are stored as longs.  This means that
 * the small values ( < 1.0) generated in a rotation matrix cannot be
 * stored properly.  To solve this, they are stored as longs to a precision
 * of 10 bits (i.e. by multiplication by 1024.)  Hence, when two rotation
 * matrices are multiplied together, each result must be divided by 1024 to
 * keep fixed point form.  When a vector is multiplied by a matrix, it must
 * be divided by 1024 to return to normal long form.
 */



/* set up a 3D rotation matrix */

void GeomGenRotMatrix(m, rv)
	matrix	*m;
	rvector	*rv;
{

#define N_ANGLES	256
#define SIN(a)		(sin_table[a])
#define COS(a)		(cos_table[a])

	/* Fixed point representations of sin & cos for fast lookup */

	static const int32_t sin_table[N_ANGLES] =
	{
		    0,    25,    50,    75,   100,   125,   150,   175,
		  199,   224,   248,   273,   297,   321,   344,   368,
		  391,   414,   437,   460,   482,   504,   526,   547,
		  568,   589,   609,   629,   649,   668,   687,   706,
		  724,   741,   758,   775,   791,   807,   822,   837,
		  851,   865,   878,   890,   903,   914,   925,   936,
		  946,   955,   964,   972,   979,   986,   993,   999,
		 1004,  1008,  1012,  1016,  1019,  1021,  1022,  1023,
		 1023,  1023,  1022,  1021,  1019,  1016,  1012,  1008,
		 1004,   999,   993,   986,   979,   972,   964,   955,
		  946,   936,   925,   914,   903,   890,   878,   865,
		  851,   837,   822,   807,   791,   775,   758,   741,
		  724,   706,   687,   668,   649,   629,   609,   589,
		  568,   547,   526,   504,   482,   460,   437,   414,
		  391,   368,   344,   321,   297,   273,   248,   224,
		  199,   175,   150,   125,   100,    75,    50,    25,
		    0,   -25,   -50,   -75,  -100,  -125,  -150,  -175,
		 -199,  -224,  -248,  -273,  -297,  -321,  -344,  -368,
		 -391,  -414,  -437,  -460,  -482,  -504,  -526,  -547,
		 -568,  -589,  -609,  -629,  -649,  -668,  -687,  -706,
		 -724,  -741,  -758,  -775,  -791,  -807,  -822,  -837,
		 -851,  -865,  -878,  -890,  -903,  -914,  -925,  -936,
		 -946,  -955,  -964,  -972,  -979,  -986,  -993,  -999,
		-1004, -1008, -1012, -1016, -1019, -1021, -1022, -1023,
		-1023, -1023, -1022, -1021, -1019, -1016, -1012, -1008,
		-1004,  -999,  -993,  -986,  -979,  -972,  -964,  -955,
		 -946,  -936,  -925,  -914,  -903,  -890,  -878,  -865,
		 -851,  -837,  -822,  -807,  -791,  -775,  -758,  -741,
		 -724,  -706,  -687,  -668,  -649,  -629,  -609,  -589,
		 -568,  -547,  -526,  -504,  -482,  -460,  -437,  -414,
		 -391,  -368,  -344,  -321,  -297,  -273,  -248,  -224,
		 -199,  -175,  -150,  -125,  -100,   -75,   -50,   -25
	};

	static const int32_t cos_table[N_ANGLES] =
	{
		 1023,  1023,  1022,  1021,  1019,  1016,  1012,  1008,
		 1004,   999,   993,   986,   979,   972,   964,   955,
		  946,   936,   925,   914,   903,   890,   878,   865,
		  851,   837,   822,   807,   791,   775,   758,   741,
		  724,   706,   687,   668,   649,   629,   609,   589,
		  568,   547,   526,   504,   482,   460,   437,   414,
		  391,   368,   344,   321,   297,   273,   248,   224,
		  199,   175,   150,   125,   100,    75,    50,    25,
		    0,   -25,   -50,   -75,  -100,  -125,  -150,  -175,
		 -199,  -224,  -248,  -273,  -297,  -321,  -344,  -368,
		 -391,  -414,  -437,  -460,  -482,  -504,  -526,  -547,
		 -568,  -589,  -609,  -629,  -649,  -668,  -687,  -706,
		 -724,  -741,  -758,  -775,  -791,  -807,  -822,  -837,
		 -851,  -865,  -878,  -890,  -903,  -914,  -925,  -936,
		 -946,  -955,  -964,  -972,  -979,  -986,  -993,  -999,
		-1004, -1008, -1012, -1016, -1019, -1021, -1022, -1023,
		-1023, -1023, -1022, -1021, -1019, -1016, -1012, -1008,
		-1004,  -999,  -993,  -986,  -979,  -972,  -964,  -955,
		 -946,  -936,  -925,  -914,  -903,  -890,  -878,  -865,
		 -851,  -837,  -822,  -807,  -791,  -775,  -758,  -741,
		 -724,  -706,  -687,  -668,  -649,  -629,  -609,  -589,
		 -568,  -547,  -526,  -504,  -482,  -460,  -437,  -414,
		 -391,  -368,  -344,  -321,  -297,  -273,  -248,  -224,
		 -199,  -175,  -150,  -125,  -100,   -75,   -50,   -25,
		    0,    25,    50,    75,   100,   125,   150,   175,
		  199,   224,   248,   273,   297,   321,   344,   368,
		  391,   414,   437,   460,   482,   504,   526,   547,
		  568,   589,   609,   629,   649,   668,   687,   706,
		  724,   741,   758,   775,   791,   807,   822,   837,
		  851,   865,   878,   890,   903,   914,   925,   936,
		  946,   955,   964,   972,   979,   986,   993,   999,
		 1004,  1008,  1012,  1016,  1019,  1021,  1022,  1023
	};

	int32_t	theta	= (rv->theta & 0xff);	/* Keep in 0..255 range */
	int32_t	phi	= (rv->phi   & 0xff);
	int32_t	gamma	= (rv->gamma & 0xff);

	/* Initialise cos & sine of phi, gamma, theta */

	fix	sp = SIN(phi);		fix	cp = COS(phi);
	fix	sg = SIN(gamma);	fix	cg = COS(gamma);
	fix	st = SIN(theta);	fix	ct = COS(theta);

TRC(printf("GeomGenRotMatrix: phi = %1.3f  theta = %1.3f  gamma = %1.3f\n", phi, theta, gamma);)

	/* Set up the matrix */
	m->a11 = RENORMALISE(  cg*ct+sg*RENORMALISE(sp*st));
	m->a12 = RENORMALISE(  sg*cp);
	m->a13 = RENORMALISE(  cg*st-sg*RENORMALISE(sp*ct));

	m->a21 = RENORMALISE( -sg*ct+cg*RENORMALISE(sp*st));
	m->a22 = RENORMALISE(  cg*cp);
	m->a23 = RENORMALISE( -sg*st-cg*RENORMALISE(sp*ct));

	m->a31 = RENORMALISE( -cp*st);
	m->a32 = sp;
	m->a33 = RENORMALISE(  cp*ct);

TRC(printf("GeomGenRotMatrix: rotation matrix is:\n");)
TRC(printf("\t(%5d,%5d,%5d)\n", m->a11, m->a12, m->a13);)
TRC(printf("\t(%5d,%5d,%5d)\n", m->a21, m->a22, m->a23);)
TRC(printf("\t(%5d,%5d,%5d)\n", m->a31, m->a32, m->a33);)
}



/* Multiply two (fixed-point) matrices together (q = mn) */

void GeomMultiplyM(q, m, n)
	matrix	*q;
	matrix	*m;
	matrix	*n;
{
	int32_t	a11, a12, a13;
	int32_t	a21, a22, a23;
	int32_t	a31, a32, a33;

	/* The FIXTOLs renormalize a result matrix after multiplication */

	a11 = FIXTOL(m->a11*n->a11 + m->a12*n->a21 + m->a13*n->a31);
	a12 = FIXTOL(m->a11*n->a12 + m->a12*n->a22 + m->a13*n->a32);
	a13 = FIXTOL(m->a11*n->a13 + m->a12*n->a23 + m->a13*n->a33);

	a21 = FIXTOL(m->a21*n->a11 + m->a22*n->a21 + m->a23*n->a31);
	a22 = FIXTOL(m->a21*n->a12 + m->a22*n->a22 + m->a23*n->a32);
	a23 = FIXTOL(m->a21*n->a13 + m->a22*n->a23 + m->a23*n->a33);

	a31 = FIXTOL(m->a31*n->a11 + m->a32*n->a21 + m->a33*n->a31);
	a32 = FIXTOL(m->a31*n->a12 + m->a32*n->a22 + m->a33*n->a32);
	a33 = FIXTOL(m->a31*n->a13 + m->a32*n->a23 + m->a33*n->a33);

	q->a11 = a11;	q->a12 = a12;	q->a13 = a13;
	q->a21 = a21;	q->a22 = a22;	q->a23 = a23;
	q->a31 = a31;	q->a32 = a32;	q->a33 = a33;
}



/* Multiply a vertex by a matrix (v = mu) */

void GeomMultiplyV(v, m, u)
	vertex	*v;
	matrix	*m;
	vertex	*u;
{
	int32_t	x, y, z;

	/* The FIXTOLs renormalize a vector after matrix multiplication */

	x = FIXTOL(m->a11*u->x + m->a12*u->y + m->a13*u->z);
	y = FIXTOL(m->a21*u->x + m->a22*u->y + m->a23*u->z);
	z = FIXTOL(m->a31*u->x + m->a32*u->y + m->a33*u->z);

	v->x = x;
	v->y = y;
	v->z = z;
}



/* Multiply a vset by a matrix (vs = m.us) */

void GeomMultiplyVs(vs, m, us)
	vset	*vs;
	matrix	*m;
	vset	*us;
{
	int	i;

	for(i = 0; i < vs->n; i++)
		GeomMultiplyV(&(vs->v[i]), m, &(us->v[i]));
}



/* Translate a vset by a vector (v[] = v[]+u) */

void GeomTranslateVs(vs, d)
	vset	*vs;
	vector	*d;
{
	/* For speed, we (naughtily) don't use our AddVector routines */

  	uint32_t	n = vs->n;
	int32_t		dx = d->x;
	int32_t		dy = d->y;
	int32_t		dz = d->z;
	vertex		*v;

	for(v = &(vs->v[0]); v < &(vs->v[n]); v++)
	{
		v->x += dx;
		v->y += dy;
		v->z += dz;
	}
}



/* Scale a vset. */

void GeomScaleVs(vs, scale)
	vset	*vs;
	fix	scale;
{
	vertex	*v;

	for(v = &(vs->v[0]); v < &(vs->v[vs->n]); v++)
	{
		v->x = FIXTOL(v->x * scale);
		v->y = FIXTOL(v->y * scale);
		v->z = FIXTOL(v->z * scale);
	}
}



/*
 * CLIPPING TO VIEW
 *	The clipping mechanism here defines the eye-point to be at (0,0,0)
 *	and the screen centre to be at (0,0,zs) perpendicular to the viewer.
 *	The screen distance zs is changable by the user.

 *	NOTE: the z-axis comes OUT of the screen, so zs should be negative
 *	in order for clipping to work.

 *	Clipping is within a closed rectangular frustum defined by the
 *	eye-point and the corners of the screen (defined by sx and sy, also
 *	user alterable) and the eye-perpendicular plane at (0,0,FARPOINT).

 *	The clipping function returns 1 if a given vertex is within
 *	the clipping frustum, 0 otherwise.
 */


int GeomInView(v, g)
	vertex *v;
	vector *g;
{
	int32_t	x = (short)(v->x*g->z / v->z) + (g->x/2);
	int32_t	y = (short)(v->y*g->z / v->z) + (g->x/2);
	int32_t	z = (short)(v->z);

TRC2(printf("GeomInView: vertex (%d, %d, %d)", v->x, v->y, v->z);)
TRC2(printf("; projection (%d, %d); screen %dx%d\n", x, y, g->x, g->y);)
	return (
		    (z <= g->z)             && (z >= FARPOINT)
		 && (x >= -LEEWAY) && (x <= g->x+LEEWAY)
		 && (y >= -LEEWAY) && (y <= g->y+LEEWAY)
	       );			/* Remember - visible z is -ve! */
}



/* Routines to project a vertex onto the screen (p = proj(v)) */

void GeomProjectV(p, v, g)
	pixel	*p;
	vertex	*v;
	vector  *g;
{
	p->x = (short)(v->x*g->z / v->z) + (g->x/2);
	p->y = (short)(v->y*g->z / v->z) + (g->x/2);
}



/* And to project a vset (ps = proj(vs)) */

void GeomProjectVs(pixels, vs, g)
	pixel	pixels[];
	vset	*vs;
	vector  *g;
{
	pixel	*pp;
	vertex	*vv;

	for(pp = pixels, vv = &(vs->v[0]); vv < &(vs->v[vs->n]); pp++, vv++)
	{
		if(vv->z == 0)
			vv->z = 1;		/* Crash mat! */

TRC(fprintf(stderr, "GeomProjectVs: vector (%d, %d, %d)", vv->x, vv->y, vv->z);)
		pp->x = (short)(vv->x*g->z / vv->z) + (g->x/2);
		pp->y = (short)(vv->y*g->z / vv->z) + (g->y/2);
TRC(printf(" projected onto (%d, %d)\n", pp->x, pp->y);)
	}
}
