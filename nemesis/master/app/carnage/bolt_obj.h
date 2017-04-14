/*
 * FILE		:	bolt_obj.h
 * AUTHOR	:	Ralph Becket
 * DATE		:	Mon May 31 13:15:35 BST 1993

 * Types & prototypes for the server to handle laser bolt objects.
 */



#ifndef __fighter_obj_h__
#define __fighter_obj_h__



#include "geometry_types.h"		/* 3D stuff... */
#include "objects.h"			/* Object type stuff... */



/* Typedefs... */

typedef struct boltobj
{
	unsigned	lifetime;	/* Disappears when == 0 */
} boltobj;



/* Prototypes... */

#ifdef __STDC__

void NewBolt();
void UpdateBolt();

#else

void NewBolt(object *new, object *owner);
void UpdateBolt(object *self);

#endif /* __STDC__ */



#endif
