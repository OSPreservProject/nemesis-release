/*
 * FILE		:	geometry_types.h
 * AUTHOR	:	Ralph Becket
 * DATE		:	Mon Mar 15 15:32:06 GMT 1993

 * Typedefs for 3D & matrix routines.
 */



#ifndef __3dtypes_h__
#define __3dtypes_h__



/* Constants... */

#define MAX_VERTICES	64		/* Renderer needs an upper limit */
#define MAX_EDGES	128		/*     Ditto     */



/* A point on the screen */

typedef struct pixel
{
	int32_t		x;
	int32_t		y;
} pixel;


/* A point in space (defined as a _column_ vector) */

typedef struct vertex
{
	int32_t		x;
	int32_t		y;
	int32_t		z;
} vertex;


typedef vertex vector;			/* Nice to have equivalence */


/* The set of vertices used to define an shape. */

typedef struct vset
{
	uint32_t	n;		/* Number of vertices. */
	vertex		*v;		/* Array of n vertices. */
} vset;


/* A face defined by points in a vset, connected by lines. */

/*
 * There is an implicit line from the last vertex to the first.
 * The points should occur in clockwise order (for hidden face removal.)
 */

typedef struct face
{
	uint32_t	n;		/* Number of lines */
	uint32_t	*p;		/* Array of n vertex no.s in a vset */
} face;


/* A set of faces. */

typedef struct fset
{
	uint32_t	n;		/* Number of faces */
	face		*f;		/* Array of n faces */
} fset;


/* A polyhedron as a vset and a number of faces. */

typedef struct polyhedron
{
	vset		*vs;		/* Set of vertices */
	fset		*fs;		/* Set of faces */
} polyhedron;



/*
 * Fixed point number format - avoids slow doubles.  10 Sig. bits accuracy. 
 * After multiplication involving fixed point numbers, the result MUST be 
 * renormalised with RENORMALISE.
 */

typedef int32_t fix;

#define DTOFIX(d)	(fix)((d)*1024.0)
#define RENORMALISE(f)	(fix)((f)>>10)
#define FIXTOL(f)	(int32_t)((f)>>10)

/*
 * Angles are in the range 0..255 (mapping on to 0..2PI.)
 */


/* Rotation vector for creation of rotation matrices */

typedef struct rvector
{
	fix		phi;		/* rotation about the y-axis */
	fix		theta;		/* rotation about the z-axis */
	fix		gamma;		/* rotation about the x-axis */
} rvector;


/*
 * Matrix for rotation of vertices.
 *
 * Entries in a rotation matrix are stored as DTOFIX(double_val).
 * After multiplication, each vector value must be normalized
 * with RENORMALISE(fix_value).  Correct format of values is
 * maintained by the routines GeomMultiplyM(), GeomMultiplyV()
 * and GeomGenRotMatrix().
 */

typedef struct matrix
{
	fix		a11, a12, a13;
	fix		a21, a22, a23;
	fix		a31, a32, a33;
} matrix;


#endif
