/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      PvsContext
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Exports Pervasives into the namespace
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: PvsContext.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <string.h>
#include <exceptions.h>

#include <Context.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

static Context_List_fn		List;
static Context_Get_fn		Get;
static Context_Add_fn		Add;
static Context_Remove_fn	Remove;
static Context_Dup_fn		Dup;
static Context_Destroy_fn	Destroy;

static Context_op c_op = { List, Get, Add, Remove, Dup, Destroy };
static const Context_cl c_cl = { &c_op, NULL };

CL_EXPORT(Context, PvsContext, c_cl);

static string_t const names[] = {
    "vp", "heap", "types", "root", "exns",
    "time", "actf", "evs", "thd", "thds", "srcth",
    "in", "out", "err", "console",
    "bndr", "objs", "salloc", "sdriver", "gkpr", "entry",
    "libc", "pers", 0 };

static uint32_t lookup_name(string_t name);

#define LIST(_number,_type,_name) \
	if (Pvs(_name)) SEQ_ADDH(list, #_name);

static Context_Names *List(Context_clp self)
{
    Context_Names *list;

    TRC(printf("PvsList()\n"));

    list=SEQ_NEW(Context_Names, 19, Pvs(heap));

    SEQ_CLEAR(list);

    LIST( 0, VP_clp, vp);
    LIST( 1, Heap_clp, heap);
    LIST( 2, TypeSystem_clp, types);
    LIST( 3, Context_clp, root);
    LIST( 4, ExnSupport_clp, exns);
    
    LIST( 5, Time_clp, time);
    LIST( 6, ActivationF_clp, actf);
    LIST( 7, Events_clp, evs);
    LIST( 8, Thread_clp, thd);
    LIST( 9, Threads_clp, thds);
    LIST(10, SRCThread_clp, srcth);
    
    LIST(11, Rd_clp, in);
    LIST(12, Wr_clp, out);
    LIST(13, Wr_clp, err);
    LIST(14, Wr_clp, console);
    
    LIST(15, Binder_clp, bndr);
    LIST(16, ObjectTbl_clp, objs);
    LIST(17, StretchAllocator_clp, salloc);
    LIST(18, StretchDriver_clp, sdriver);
    LIST(19, Gatekeeper_clp, gkpr);
    LIST(20, Entry_clp, entry);
    
    LIST(21, addr_t, libc);
    LIST(22, addr_t, pers);

    return list;
}

#define GET(_number,_type,_name) \
	case _number: \
	ANY_INIT(obj, _type, Pvs(_name)); \
	TRC(printf(#_name "=%p\n",Pvs(_name))); \
	return Pvs(_name)!=NULL;

static bool_t Get(Context_clp self, string_t name, Type_Any *obj)
{
    uint32_t rec;
    char *c;

    TRC(printf("PvsGet(%s)\n",name));

    /* Special case for Pvs(root) */
    if ((c=strchr(name,'>'))) {
	if ((c-name)==4 && strncmp(name,"root",4)==0) {
	    return Context$Get(Pvs(root),c+1,obj);
	} else {
	    RAISE_Context$NotContext();
	}
    }

    rec=lookup_name(name);

    switch(rec) {
	GET( 0, VP_clp, vp);
	GET( 1, Heap_clp, heap);
	GET( 2, TypeSystem_clp, types);
	GET( 3, Context_clp, root);
	GET( 4, ExnSupport_clp, exns);
    
	GET( 5, Time_clp, time);
	GET( 6, ActivationF_clp, actf);
	GET( 7, Events_clp, evs);
	GET( 8, Thread_clp, thd);
	GET( 9, Threads_clp, thds);
	GET(10, SRCThread_clp, srcth);
    
	GET(11, Rd_clp, in);
	GET(12, Wr_clp, out);
	GET(13, Wr_clp, err);
	GET(14, Wr_clp, console);
    
	GET(15, Binder_clp, bndr);
	GET(16, ObjectTbl_clp, objs);
	GET(17, StretchAllocator_clp, salloc);
	GET(18, StretchDriver_clp, sdriver);
	GET(19, Gatekeeper_clp, gkpr);
	GET(20, Entry_clp, entry);
    
	GET(21, addr_t, libc);
	GET(22, addr_t, pers);
    }

    TRC(printf("PvsGet: not one we know about\n"));

    return False;
}

#define ADD(_number, _type, _name) \
	case _number: \
	if (Pvs(_name)) RAISE_Context$Exists(); \
	if (ISTYPE(obj, _type)) Pvs(_name)=NARROW(obj, _type); \
	else RAISE_Context$Denied(); \
	return;

static void Add(Context_clp self, string_t name, const Type_Any *obj)
{
    uint32_t rec;

    TRC(printf("PvsAdd(%s,%p)\n",name,obj->val));

    rec=lookup_name(name);

    switch(rec) {
	ADD( 0, VP_clp, vp);
	ADD( 1, Heap_clp, heap);
	ADD( 2, TypeSystem_clp, types);
	ADD( 3, Context_clp, root);
	ADD( 4, ExnSupport_clp, exns);
    
	ADD( 5, Time_clp, time);
	ADD( 6, ActivationF_clp, actf);
	ADD( 7, Events_clp, evs);
	ADD( 8, Thread_clp, thd);
	ADD( 9, Threads_clp, thds);
	ADD(10, SRCThread_clp, srcth);
    
	ADD(11, Rd_clp, in);
	ADD(12, Wr_clp, out);
	ADD(13, Wr_clp, err);
	ADD(14, Wr_clp, console);
    
	ADD(15, Binder_clp, bndr);
	ADD(16, ObjectTbl_clp, objs);
	ADD(17, StretchAllocator_clp, salloc);
	ADD(18, StretchDriver_clp, sdriver);
	ADD(19, Gatekeeper_clp, gkpr);
	ADD(20, Entry_clp, entry);
    
	ADD(21, addr_t, libc);
	ADD(22, addr_t, pers);
    }

    TRC(printf("PvsAdd: not one we know about\n"));

    RAISE_Context$Denied();
}

#define REMOVE(_number, _type, _name) \
	case _number: \
	TRC(printf("Removing " #_name "\n")); \
	Pvs(_name)=NULL; \
	return;

static void Remove(Context_clp self, string_t name)
{
    uint32_t rec;

    TRC(printf("PvsRemove(%s)\n",name));

    rec=lookup_name(name);

    switch(rec) {
	REMOVE( 0, VP_clp, vp);
	REMOVE( 1, Heap_clp, heap);
	REMOVE( 2, TypeSystem_clp, types);
	REMOVE( 3, Context_clp, root);
	REMOVE( 4, ExnSupport_clp, exns);
    
	REMOVE( 5, Time_clp, time);
	REMOVE( 6, ActivationF_clp, actf);
	REMOVE( 7, Events_clp, evs);
	REMOVE( 8, Thread_clp, thd);
	REMOVE( 9, Threads_clp, thds);
	REMOVE(10, SRCThread_clp, srcth);
    
	REMOVE(11, Rd_clp, in);
	REMOVE(12, Wr_clp, out);
	REMOVE(13, Wr_clp, err);
	REMOVE(14, Wr_clp, console);
    
	REMOVE(15, Binder_clp, bndr);
	REMOVE(16, ObjectTbl_clp, objs);
	REMOVE(17, StretchAllocator_clp, salloc);
	REMOVE(18, StretchDriver_clp, sdriver);
	REMOVE(19, Gatekeeper_clp, gkpr);
	REMOVE(20, Entry_clp, entry);
    
	REMOVE(21, addr_t, libc);
	REMOVE(22, addr_t, pers);
    }

    RAISE_Context$NotFound(name);
}

static Context_clp Dup(Context_clp self, Heap_clp h, WordTbl_clp xl)
{
    TRC(printf("PvsDup()\n"));

    RAISE_Context$Denied();

    return NULL;
}

static void Destroy(Context_clp self)
{
    TRC(printf("PvsDestroy()\n"));

    /* Nothing to it */
}

static uint32_t lookup_name(string_t name)
{
    int i;

    if (!name) return -1;

    for (i=0; names[i]; i++)
	if (strcmp(name, names[i])==0) {
	    return i;
	}

    return -1;
}
