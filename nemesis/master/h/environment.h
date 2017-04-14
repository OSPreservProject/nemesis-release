
/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      Environment query macros
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Provide a access to local namespace environment variables, with defaulting
** 
** ENVIRONMENT: 
**
**      With access to namespace
** 
** ID : $Id: environment.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _environment_h_
#define _environment_h_

#define GET_ARG_FROM(name, arg_type, default, cont) \
({ \
    Type_Any _any; \
    arg_type NOCLOBBER _res = 0; \
		     \
    if (Context$Get (cont, (name), &_any)) {\
         TRY \
	    _res = NARROW(&_any, arg_type);\
	CATCH_TypeSystem$Incompatible()\
	    {\
		printf("Argument %s of wrong type\n",#name);\
		_res = (default); \
	    } \
	ENDTRY;\
    } else {\
	_res = (default);\
    }\
    _res;\
})

#define GET_ARG(name, arg_type, default) GET_ARG_FROM(name,arg_type,default,Pvs(root))

#define GET_INT_ARG(name, default)  GET_ARG(name, int64_t, default)
#define GET_STRING_ARG(name, default) GET_ARG(name, string_t, default)
#define GET_BOOL_ARG(name,default) GET_ARG(name, bool_t, default)

#define ENV_WINDOW_X_LOCATION "x"
#define ENV_WINDOW_Y_LOCATION "y"
#define ENV_WINDOW_WIDTH "width"
#define ENV_WINDOW_HEIGHT "height"

#define ENV_NFS_SERVER "nfsserver"
#define ENV_NFS_DIRECTORY "nfsdir"

#define MAIN_RUNES(modname) \
 Closure_Apply_fn Main; \
 static Closure_op ms = { Main }; \
static const Closure_cl cl = {&ms, NULL}; \
CL_EXPORT(Closure, modname, cl); 


#endif /* _environment_h_ */

