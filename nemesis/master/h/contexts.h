/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.		                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      contexts.h
** 
** DESCRIPTION:
** 
**      Macros for dealing with contexts (ala Context.if)
**
*/

#ifndef _CONTEXTS_H_
#define _CONTEXTS_H_

#include <Context.ih>
#include <ContextMod.ih>
#include <ContextUtil.ih>

#include <exceptions.h>


#define NAME_EXISTS_IN_CX(_name,_ctxt)\
   ({\
	Type_Any any; \
	bool_t result;\
	result = False;\
	if (Context$Get(_ctxt, _name, &any)) result = True;\
	result;\
    })

#define NAME_EXISTS(_name) NAME_EXISTS_IN_CX(_name, Pvs(root))

/*
 * Name allocate: _namepattern should be a printf string
 * containing one %d. NAME_ALLOCATE_IN_CX will return a string
 * which is a valid new name in that context, by iterating through
 * ascending integers to find a new name. The macro returns a string
 * which has been dynamically allocated and should be freed by the client.
 *
 * eg NAME_ALLOCATE("self>thread_%d") 
 */

#define NAME_ALLOCATE_IN_CX_WITH_BUFFER(_namepattern, _ctxt, _bfr)  \
    ({                                                           \
	int i;                                                   \
	bool_t found;                                            \
	i = 0;                                                   \
	found = False;                                           \
	while (!found) {                                         \
	    sprintf(_bfr, _namepattern, i++);                     \
	    if (!NAME_EXISTS_IN_CX(_bfr,_ctxt)) {               \
		found = True;                                    \
	    }                                                    \
	}                                                        \
	_bfr;                                                     \
    })

#define NAME_ALLOCATE_IN_CX(_namepattern, _ctxt) ({ \
	string_t str = NULL; \
	str = malloc(strlen(_namepattern)+16);                  \
	NAME_ALLOCATE_IN_CX_WITH_BUFFER(_namepattern, _ctxt, str); \
								    })\

#define NAME_ALLOCATE(_namepattern) NAME_ALLOCATE_IN_CX(_namepattern, Pvs(root))

/*
 * Name lookup : handy when you assume nothing is going to go
 * wrong (raises TypeSystem.Incompatible if it does) 
 */

#define NAME_FIND(_name,_type) NAME_LOOKUP (_name, Pvs (root), _type)
#define NAME_LOOKUP(_name,_ctxt,_type) \
  ({ \
    Type_Any any; \
    if (!Context$Get (_ctxt, _name, &any)) RAISE_Context$NotFound(_name); \
    NARROW (&any, _type); \
  })

#define NAME_AWAIT(_name) \
do {						\
    Type_Any any;				\
    while(!Context$Get(Pvs(root), _name, &any))	\
	PAUSE(MILLISECS(100));			\
} while(0)


#define SEP		'>'	/* name separator; see Context.if */
#define SEP_STRING	">"     /* legacy from when there were two seps */

#define CX_NEW_IN_CX(_ctx,_name)					  \
({									  \
  ContextMod_clp cmod = NAME_FIND ("modules>ContextMod", ContextMod_clp); \
  Context_clp tmp = ContextMod$NewContext (cmod, Pvs(heap), Pvs(types));  \
  ANY_DECL (tmpany, Context_clp, tmp); 					  \
  Context$Add (_ctx,_name,&tmpany);					  \
  tmp;									  \
})

#define CX_ADD_IN_CX(_ctx,_name,_val,_type)				  \
do {									  \
  ANY_DECL (tmpany,_type,_val); 					  \
  Context$Add (_ctx,_name,&tmpany);					  \
} while(0)

#define CX_NEW(_name)		 CX_NEW_IN_CX(Pvs(root),_name)
#define CX_ADD(_name,_val,_type) CX_ADD_IN_CX(Pvs(root),_name,_val,_type)

#define CX_DUMP(_val,_type) \
    do { \
    ContextUtil_clp cu=NAME_FIND("modules>ContextUtil",ContextUtil_clp); \
    ANY_DECL(any,_type,_val); \
    ContextUtil$List(cu,&any,Pvs(err),"("#_val")"); \
    } while(0)


#endif /* _CONTEXTS_H_ */
