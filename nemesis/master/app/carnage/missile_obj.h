/*
 * FILE		:	missile_obj.h
 * AUTHOR	:	Ralph Becket
 * DATE		:	Mon May 31 13:15:35 BST 1993

 * Types & prototypes for the server to handle missile objects.
 */



#ifndef __missile_obj_h__
#define __missile_obj_h__



#include "geometry_types.h"		/* 3D stuff... */
#include "objects.h"			/* Object type stuff... */



/* Typedefs... */

typedef struct missileobj
{
	unsigned	lifetime;	/* Disappears when == 0 */
} missileobj;



/* Prototypes... */

#ifdef __STDC__

void NewMissile();
void UpdateMissile();

#else

void NewMissile(object *new, object *owner);
void UpdateMissile(object *self);

#endif /* __STDC__ */



#endif
