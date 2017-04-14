/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	
**
** FUNCTIONAL DESCRIPTION:
**
**	
**
** ENVIRONMENT: 
**
**
**
** ID : $Id: cexec.c 1.3 Tue, 01 Jun 1999 14:52:06 +0100 dr10009 $
**
**
*/

/* XXX: this really isn't actually a part of clanger and should be moved
   to a different directory */

#include <nemesis.h>
#include <stdio.h>

#include <CExec.ih>
#include <cexec_st.h>
#include <IDCMacros.h>
#include <Spawn.ih>
#include <SpawnMod.ih>
#include <time.h>
#include <ecs.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);


static  CExec_Load_fn           CExec_Load_m;
static  CExec_Run_fn            CExec_Run_m;
static  CExec_Open_fn           CExec_Open_m;
static  CExec_Destroy_fn        CExec_Destroy_m;
static  CExec_RunWait_fn        CExec_RunWait_m;

static  CExec_op        ms = {
  CExec_Load_m,
  CExec_Run_m,
  CExec_Open_m,
  CExec_Destroy_m,
  CExec_RunWait_m
};

const static    CExec_cl        cl = { &ms, NULL };

CL_EXPORT (CExec, CExec, cl);

/*---------------------------------------------------- Entry Points ----*/

static void CExec_Load_m (
    CExec_cl	*self,
    string_t	name	/* IN */ )
{
    FILE *f;
    Type_Any mod_any;

    f = fopen(name,"r");

    if (!f) {
	printf("CExec_Load_m: can't open %s for reading\n",name);
	return;
    }

    ANY_INIT(&mod_any, Rd_clp, f->rd); /* We know what's in there... */
    Exec$Load(NAME_FIND("modules>Exec", Exec_clp), &mod_any, 
	      VP$ProtDomID(Pvs(vp)));
    fclose(f);
}

static Rd_clp CExec_Open_m (
    CExec_cl *self,
    string_t name)
{
    FILE *f;
    Rd_clp rd;

    f=fopen(name,"r");
    if (!f) {
	printf("CExec_Open_m: can't open %s for reading\n",name);
	return NULL;
    }

    rd = f->rd;

    FREE(f); /* XXX this is very naughty, but ought to work for now */

    return rd;
}
	

static void CExec_Destroy_m (
    CExec_cl	*self )
{
/*    CExec_st	*st = self->st; */

    UNIMPLEMENTED;
}
typedef struct Closure_st {
    char *filename;
    Closure_cl self;
    Type_Any any;
    Context_clp env;
    bool_t block;
} Closure_st;


static	Closure_Apply_fn 		Closure_Apply_m;

static	Closure_op	clms = {
  Closure_Apply_m
};

/* this routine is the entry point of new domains */
static void Closure_Apply_m(Closure_cl *self) {
    Closure_st *clst = (Closure_st *) self->st;
    FILE *f;
    Type_Any mod_any;
    Closure_clp cl;
    /* okay, we are now in the new domain */
    TRC(printf("New domain is Go!\n"); PAUSE(SECONDS(1)));

    TRC(printf("Loading %s\n", clst->filename));

    f = fopen(clst->filename,"r");
    TRC(printf("Opened file; FILE * is %p", f); PAUSE(SECONDS(1)));

    if (!f) {
	printf("CExec: can't open %s for reading\n",clst->filename);
	/* XXX: raise an exception here? */
	return;
    }
    ANY_INIT(&mod_any, Rd_clp, f->rd);
    TRC(printf("Loading program\n"));

    cl = Exec$LoadProgram(NAME_FIND("modules>Exec", Exec_clp), &mod_any, VP$ProtDomID(Pvs(vp)));
    fclose(f);

    if (! clst->block) {
	/* we aren't supposed to block any more; so fork off a thread
	   to actually do the work then have this thread exit */
	TRC(printf("Forking main thread\n"));
	Threads$Fork(Pvs(thds), ((cl)->op->Apply), cl, 0);
	TRC(printf("Main thread forked\n"));
    } else {
	TRC(printf("Entering main thread\n"));
	Closure$Apply(cl);
    }
    TRC(printf("New program closure finishing\n"));
}

static void start_program (
        CExec_cl        *self,
        string_t        name    /* IN */,
        Context_clp     context /* IN */,
	bool_t          block)
{
  Spawn_clp spawn_clp;
  Closure_st *clst;
  Type_Any *spawn_stub;

  TRC(printf("Starting program %s; blocking %d\n", name, block));
  Exec$FillEnv(NAME_FIND("modules>Exec", Exec_clp), context); 
  clst = malloc(sizeof(Closure_st));
  clst->self.op = &clms;
  clst->self.st = (void*) clst;
  clst->env = context;
  clst->filename = name;
  clst->block = block;
  ANY_INIT(&(clst->any), Closure_clp, (&clst->self));

  spawn_clp = SpawnMod$Create(NAME_FIND("modules>SpawnMod", SpawnMod_clp), &clst->any, Pvs(heap));

  Spawn$SetName(spawn_clp, name);
  Spawn$SetEnv(spawn_clp, context);
  spawn_stub = Spawn$GetStub(spawn_clp);

  Closure$Apply(NARROW(spawn_stub, Closure_clp));
  TRC(printf("Spawn Closure invocation returned\n"));
  Spawn$Destroy(spawn_clp);
}

static void CExec_Run_m (
    CExec_cl	*self,
    string_t	name	/* IN */,
    Context_clp	context	/* IN */ )
{
    start_program(self, name, context, 0);
}

static void CExec_RunWait_m (
    CExec_cl	*self,
    string_t	name	/* IN */,
    Context_clp	context	/* IN */ )
{
    start_program(self, name, context, 1);
}



/*
 * End 
 */








