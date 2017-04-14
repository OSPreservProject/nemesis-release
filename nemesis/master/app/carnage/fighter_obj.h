/*
 * FILE		:	fighter_obj.h
 * AUTHOR	:	Ralph Becket
 * DATE		:	Mon May 31 13:15:35 BST 1993

 * Types & prototypes for the server to handle fighter objects.
 */



#ifndef __fighter_obj_h__
#define __fighter_obj_h__



#include "geometry_types.h"		/* 3D stuff... */
#include "objects.h"			/* Object type stuff... */



/* Typedefs... */

typedef struct fighterobj
{
	/* Fighter specific things... */
} fighterobj;



/* Prototypes... */

#ifdef __STDC__

void NewFighter();
void UpdateFighter();

#else

void NewFighter(object *new);
void UpdateFighter(object *self, unsigned acc, rvector *turn, int fire /*etc*/);

#endif /* __STDC__ */



#endif
