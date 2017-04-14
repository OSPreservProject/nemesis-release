/*
 * FILE		:	objects.h
 * AUTHOR	:	Ralph Becket
 * DATE		:	Mon May 31 15:13:07 BST 1993

 * Typedefs etc concerning game objects.  Each object has an
 *	id,
 *	type,
 *	name,
 *	active state [& controller],
 *	slave state [& master],
 *	collision detection info,
 *	size, position, velocity, orientation, spin and mass,
 *	[private info].

 * Active objects have controllers (controlling clients) and provide
 * IDL interfaces to appropriate clients as well as private routines
 * for manipulation within the game server (basically, movement
 * validation routines, state updating and firing control.)

 * Passive objects are those that have no controller (controlling client.)
 * Such objects blindly follow the laws of physics as laid down in
 * the game universe.

 * Active and passive objects may exhibit special behaviour
 * through means of of an UpdateXXX() method (e.g. lifetime
 * of laser bolts.)

 * Slaved objects are controlled from a master object.  A slaved
 * object's position, orientation, velocity and spin are all
 * relative to the master object.  Master/slave hierarchies are
 * possible.

 * Collision detection info specifies the following:  whether
 * this object is to be checked against others (e.g. a laser
 * bolt will not, but a fighter certainly will),  whether the
 * the object does/receives damage on collision,
 * whether collision causes something special to happen
 * (e.g. picking up a cargo container), and with what sorts of
 * objects collision detection is to occur on (e.g. laser bolts
 * are not checked against anything, whereas fighters & missiles
 * will be checked againt averything, including laser bolts.)

 */



#ifndef __objects_h__
#define __objects_h__



#include "geometry_types.h"		/* 3D stuff... */
#include "comms.h"			/* Comms stuff... */
#include "bolt_obj.h"			/* Laser bolt object */
#include "fighter_obj.h"		/* Fighter object */
#include "missile_obj.h"		/* Missile object */



typedef unsigned long objid;

typedef enum objtype {
		       BOLT,
		       MISSILE,
		       FIGHTER
		     }  objtype;

typedef struct object
{
	objid		id;
	objtype		type;
	char		*name;

	int		active;		/* 1 for yes, 0 for no */
	comlink		controller;	/* If active */

	/* Slave info to go here... */

	/* Collision info to go here... */

	unsigned	radius;		/* Size - may improve on this */
	unsigned	mass;
	vector		position;
	rvector		orientation;
	vector		velocity;
	rvector		spin;		/* This one's for Mothy */

	union				/* Private info */
	{
		boltobj		bolt;
		fighterobj	fighter;
		missileobj	missile;
	} private;
} object;



#endif
