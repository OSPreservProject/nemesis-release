/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/nemesis/context/Context.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Provides a single-level naming context; implementing
**      Context.if. Supports multiple readers in different domains,
**	plus addition and deletion by managing domain. Currently no
**	garbage collection.
** 
** ENVIRONMENT: 
**
**      Anywhere.
** 
** ID : $Id: Context.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <links.h>

#include <ContextMod.ih>
#include <WordTblMod.ih>

/* 
 * Debugging Macros
 */

/* #define CONTEXT_TRACE */
/* #define DEBUG */

#ifdef CONTEXT_TRACE
#define TRC(_x) _x
#define DEBUG
#else
#define TRC(_x)
#endif
#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(x)
#endif

#define HASH_TABLE_SIZE  (89)
#define MAX_PATH_NAME	 (256)

/*
 * Module Stuff
 */

static ContextMod_NewContext_fn NewContext;
static ContextMod_NewMerged_fn  NewMerged;

static ContextMod_op m_op = { NewContext, NewMerged };
static const ContextMod_cl m_cl = { &m_op, (addr_t)0 };

CL_EXPORT(ContextMod,ContextMod,m_cl);
CL_EXPORT_TO_PRIMAL(ContextMod,ContextModCl,m_cl);

static Context_List_fn		List;
static Context_Get_fn		Get;
static Context_Add_fn		Add;
static Context_Remove_fn	Remove;
static Context_Dup_fn		Dup;
static Context_Destroy_fn	DestroyCtxt;

static Context_op c_op = { List, Get, Add, Remove, Dup, DestroyCtxt };

static Context_List_fn			MergedList;           
static Context_Get_fn			MergedGet;            
static Context_Add_fn			MergedAdd;            
static Context_Remove_fn		MergedRemove;         
static Context_Dup_fn			MergedDup;         
static Context_Destroy_fn        	MergedDestroy;    
static MergedContext_AddContext_fn	MergedAddContext;   
static MergedContext_DeleteContext_fn	MergedDeleteContext;
static MergedContext_ListContexts_fn	MergedListContexts;

static MergedContext_op Merged_op = {
  MergedList,             
  MergedGet,            
  MergedAdd,            
  MergedRemove,         
  MergedDup,
  MergedDestroy,    
  MergedAddContext,   
  MergedDeleteContext,
  MergedListContexts
};

/*
 * Context data structures
 */

/* Link pointers */
typedef struct Link_t Link_t;
struct Link_t {
  Link_t *next;
  Link_t *prev;
};

/* Name Entry */
typedef struct Bucket_t Bucket_t;
struct Bucket_t {
  Link_t   l;
  string_t name;
  Type_Any obj;
};

/* Context itself */
typedef struct Context_st Context_st;
struct Context_st {
  Link_t ht[ HASH_TABLE_SIZE ];
  word_t		size;		/* size of hash table 		*/
  word_t		entries;	/* no. of entries     		*/
  Heap_clp		h;
  TypeSystem_clp 	ts;
  Type_Code		ctxtCode;	/* Type Code for a context	*/
};

/*
 * Statics
 */
static uint32_t Hash( string_t *p, int *len, word_t size );
static Bucket_t *GetBucket( Context_st *st, 
			 uint32_t val,
			 string_t name,
			 word_t   len );
static void AddBucket(Context_st *st,
		      uint32_t val,
		      string_t name,
		const Type_Any *obj );
static void RemoveBucket(Context_st *st, Bucket_t *b );

/********************************************************/
/*							*/
/*	Module call: create a new context		*/
/*							*/
/********************************************************/

/*
 * Get a new context
 */
static Context_cl *NewContext(ContextMod_cl    *self,
			      Heap_clp		h,
			      TypeSystem_clp	ts)
{
  Context_st *st;
  struct _Context_cl *cl = (struct _Context_cl *)0;
  int i;
  
  TRC(eprintf("ContextMod$New : called.\n"));
  
  /* Create state record */
  st = Heap$Malloc( h, sizeof( *st ) + sizeof( *cl ));
  if ( !st ) {
    DB(eprintf("ContextMod$New : failed to create state.\n"));
    RAISE_Heap$NoMemory();
    return cl; 
  }
  st->h       = h;
  st->ts      = ts;
  st->size    = HASH_TABLE_SIZE;
  st->entries = 0;
  for( i=0; i < HASH_TABLE_SIZE ; i++ ) {
    LINK_INITIALISE(&(st->ht[i]));
  }
  
  /* Create closure */
  cl = (struct _Context_cl *)( st + 1 );
  cl->st = st;
  cl->op = &c_op;
    
  return cl;
}

/********************************************************/
/*							*/
/*	Internal calls				       	*/
/*							*/
/********************************************************/

/*
 * Hash function : algorithm very similar to Tcl: multiply by nine and
 * add char. 
 * 
 * Returns the hash value of the first component of the name. 
 * If there is a further component to the name, the string pointer
 * will point to a non-null character, and the first component will be
 * unchanged in location, but now null-terminated.
 */
static uint32_t Hash( string_t *sp, int *len, word_t size )
{
  uint32_t  h = 0;
  char     *p = *sp;

  /* Compute hash fn up to a null or separator */
  while( *p && *p != SEP ) {
    h += ( (h<<3) + (*p++) );
    h &= 0x00ffffff;
  };

  *len = p - *sp;

  /* Skip a separator, if present on the end */
  if ( *p ) ++p;
  *sp = p;

  return h % size;
}

/*
 * Internal lookup routine
 */
static Bucket_t *GetBucket( Context_st *st, 
			 uint32_t val,
			 string_t name,
			 word_t   len )
{
  Link_t *l;
  Bucket_t *b;

  /* Simply run along the buckets in the chain, looking for a string */
  /* match. */
  for( l = st->ht[ val ].next; l != &(st->ht[ val ]); l = l->next ) {
    b = (Bucket_t *)l;

    if ( (!memcmp(name, b->name, len)) && (!b->name[len]) ) {
      return b;
    }
  }
  return (Bucket_t *)0;
}

/*
 * Add a new bucket
 */
static void AddBucket(Context_st *st,
		      uint32_t val,
		      string_t name,
		const Type_Any *obj )
{
  Bucket_t *b;

  /* Get memory for the bucket and the string. */
  b = Heap$Malloc( st->h, sizeof(*b) + strlen(name) + 1 );
  if (!b) RAISE_Heap$NoMemory();
  /* Fill in the name and the any. */
  b->name = (string_t)(b+1);
  ANY_COPY(&b->obj,obj);
  strcpy( b->name, name );

  /* Add to the bucket chain */
  LINK_ADD_TO_HEAD( &(st->ht[val]), &(b->l) );
  st->entries++;
}

/*
 * Remove a bucket
 */
static void RemoveBucket(Context_st *st,
			 Bucket_t *b )
{
  LINK_REMOVE( &(b->l));
  st->entries--;
  Heap$Free( st->h, b );
}

/********************************************************/
/*							*/
/*	Context closure methods	       			*/
/*							*/
/********************************************************/

/* 
 * List the names in a context
 */
static Context_Names *List (Context_cl *self)
{
  Context_st *st = (Context_st *)(self->st);
  Context_Names *res;
  NOCLOBBER int i, j;

  TRC(eprintf("Context$List: called\n"));

  res = SEQ_NEW (Context_Names, st->entries, Pvs(heap));

  j = 0;

  TRY {
    /* Iterate through hash table */
    for( i=0; i < st->size; i++) {
      Link_t *l;
      
      /* Iterate through buckets */
      for( l = st->ht[i].next; l != &(st->ht[i]); l = l->next ) {
	
	Bucket_t 	       *b = (Bucket_t *)l;
	if (! (res->data[j++] = strdup( b->name )))
	  RAISE_Heap$NoMemory();
	
	TRC(eprintf("Context$List: added '%s'\n", b->name));
      }
    }
  } 
  CATCH_ALL {
    DB(eprintf("Context$List: failed in Malloc: undoing.\n"));
    SEQ_FREE_ELEMS (res);
    SEQ_FREE (res);
    DB(eprintf("Context$List: done undoing.\n"));
    RAISE_Heap$NoMemory();
  } 
  ENDTRY;
  
  TRC(eprintf("Context$List: done.\n"));

  return res;
}

/*
 * Look up a name in the context
 */
static bool_t Get( Context_cl *self, string_t name,
		  /* RETURNS */ Type_Any *o )
{
  Context_st *st = (Context_st *)(self->st);
  string_t rest = name;
  int	   len;
  uint32_t val;
  Bucket_t *b;

  TRC(eprintf("Context$Get: look up '%s'\n", name));

  /* Look up the first component in the name */
  val    = Hash( &rest, &len, st->size  );
  b      = GetBucket( st, val, name, len );
  if (b) ANY_COPY(o,&b->obj);

  /* If there was only one path component, return the result of the */
  /* internal lookup. */
  if ( *rest == '\0' )
    {
      if (b)
	{
	  TRC(eprintf("Context$Get: found simple name.\n"));
	  return True;	  
	}
      else
	{
	  DB(eprintf("Context$Get: '%s' not found.\n", name));
	  return False;
	}
    } 
  else
    {
	/* At this stage there is another component. Thus we need to check */
	/* that the Type.Any actually is a subtype of Context, and then */
	/* recurse. */
	
	/* Check conformance with the context type */
	if (b) {
	    if(TypeSystem$IsType(st->ts, o->type, Context_clp__code ) ) {
		Context_clp cl = (Context_clp)
		    (TypeSystem$Narrow(st->ts, o, Context_clp__code ));
		return Context$Get( cl, rest, o );
	    } else {
		if(Pvs(exns)) { 
		    RAISE_Context$NotContext();
		    /* XXX Have to check Pvs(exns), since Context$Get is caled
		       before exception system is set up */
		} else {
		    DB(eprintf("Context$Get: %s not context\n", b->name));
		    return False;
		}
	    }
	} else { 
	    /* Haven't found this item */
	    DB(eprintf("Context$Get: failed to continue path.\n"));
	    return False;
	}
    }
}

/*
 * Bind a name in a context (not necessarily this one!)
 */
static void Add(Context_cl *self,
		string_t name,
	  const Type_Any *obj )
{
    Context_st *st = (Context_st *)(self->st);
    string_t rest = name;
    int      len;
    uint32_t val;
    Bucket_t *b;
    Type_Any *result;
    
    TRC(eprintf("Context$Add: bind '%s'.\n", name));
    
    /* Look up the first component in the name */
    val    = Hash( &rest, &len, st->size  );
    b      = GetBucket( st, val, name, len );
    result = b ? &b->obj : (Type_Any *)0;
    
    /* If there was only one path component, return the result of the */
    /* internal lookup. */
    if ( *rest == '\0' )
    {
	if ( b ) {
	    DB(eprintf("Context$Add: name exists.\n");
	       eprintf("len=%x b->name='%s' name=%lx b->name=%lx\n",
		       len, b->name, name, b->name));
	    RAISE_Context$Exists();
	} else {
	    TRC(eprintf("Context$Add: binding name.\n"));
	    AddBucket( st, val, name, obj );
	}
    } else {
	/* At this stage there is another component. Thus we need to */
	/* check that the Type.Any actually is a subtype of Context, */
	/* and then recurse. */
	
	/* Check conformance with Context */
	TRC(eprintf("Context$Add: compound name '%s'\n", rest));
	TRC(eprintf("Context$Add: result->type  = %x\n", result->type));
	TRC(eprintf("Context$Add: Context__code = %x\n", Context_clp__code));
	if ( b ) {
	    if(TypeSystem$IsType(st->ts, result->type, Context_clp__code)) {
		Context_clp cl = (Context_clp)
		    (TypeSystem$Narrow(st->ts, result, Context_clp__code));
		return Context$Add(cl, rest, obj );
	    } else {
		RAISE_Context$NotContext();
	    }
	} else {
	    DB(eprintf("Context$Add: context not found.\n"));
	    RAISE_Context$NotFound(name);
	}
    }
}


/*
 * Remove a name from a context (not necessarily this one!)
 */
static void Remove(Context_cl *self,
		   string_t name )
{
    Context_st *st = (Context_st *)(self->st);
    string_t rest = name;
    int      len;
    uint32_t val;
    Bucket_t *b;
    Type_Any *result;
    
    TRC(eprintf("Context$Remove: delete '%s'\n", name));
    
    /* Look up the first component in the name */
    val    = Hash( &rest, &len, st->size  );
    b      = GetBucket( st, val, name, len );
    result = b ? &b->obj : (Type_Any *)0;
    
    /* If there was only one path component, remove it. */
    if ( *rest == '\0' )
    {
	if (result)
	{
	    TRC(eprintf("Context$Remove: found simple name.\n"));
	    RemoveBucket( st, b );
	}
	else
	{
	    DB(eprintf("Context$Remove: %s not found.\n", name));
	    RAISE_Context$NotFound(name);
	    return;
	} 
    }
    else
    {
	/* At this stage there is another component. Thus we need to check */
	/* that the Type.Any actually is a subtype of Context, and then */
	/* recurse. */
	
	/* Check conformance with Context */
	if (result) { 
	    if(TypeSystem$IsType(st->ts, result->type, Context_clp__code))
	    {
		Context_clp cl = (Context_clp)
		    (TypeSystem$Narrow(st->ts, result, Context_clp__code));
		return Context$Remove(cl, rest );
	    } else {
		RAISE_Context$NotContext();
	    }
	} else {
	    DB(eprintf("Context$Remove: failed to continue path.\n"));
	    RAISE_Context$NotFound(name);
	}
    }
}

/* 
 * Duplicate a Context
 */
static Context_clp
Dup (Context_clp self, Heap_clp h, WordTbl_clp xl)
{
  Context_st   *st = (Context_st *)(self->st);
  Context_clp   res;
  bool_t	xl_provided = (xl != NULL); /* false => we are root */
  NOCLOBBER int i;

  res = NewContext (NULL, h, st->ts);

  /* Iterate through hash table */
  for (i = 0; i < st->size; i++)
  {
    Link_t *l;
    
    /* Iterate through buckets */
    for( l = st->ht[i].next; l != &(st->ht[i]); l = l->next )
    {
      Bucket_t   *b     = (Bucket_t *)l;
      Type_Any	  cany;
      addr_t	  ptr;

      cany = b->obj;

      if (xl && WordTbl$Get (xl, cany.val, &ptr))
      {
	cany.val = (Type_Val) ptr;
      }
      else if (ISTYPE (&b->obj, Context_clp))
      {
	if (!xl)
	{
	  WordTblMod_clp m = NAME_FIND ("modules>WordTblMod", WordTblMod_clp);
	  xl = WordTblMod$New (m, Pvs(heap));
	}
	WordTbl$Put (xl, (WordTbl_Key) self, (WordTbl_Val) res);
	cany.val = (Type_Val) Context$Dup ((Context_clp) b->obj.val, h, xl);
      }
      else if (TypeSystem$UnAlias (st->ts, b->obj.type) == string_t__code)
      {
	cany.val = (Type_Val) strduph ((string_t) b->obj.val, h);
	if (!cany.val) RAISE_Heap$NoMemory();
      }

      Context$Add (res, b->name, &cany);
    }
  }

  if (!xl_provided && xl) WordTbl$Destroy (xl);

  return res;
}

/*
 * Destroy a naming context
 */
static void DestroyCtxt( Context_cl *self )
{
  Context_st *st = (Context_st *)(self->st);
  int i;

  /* Run through all the hash entries. */
  for( i=0; i < st->size; i++ )
    {
      while( st->ht[i].next != &(st->ht[i]) ) 
	{
	  RemoveBucket( st, (Bucket_t *)(st->ht[i].next) );
	}
    }
  /* Finally lose both the state and closure in one go */
  Heap$Free( st->h, st );
}

/*------------------------------------------------------------------------*/


/********************************************************/
/*							*/
/*	Merged Contexts					*/
/*							*/
/********************************************************/


/*
 * Data structures
 */

typedef struct Member_t Member_t;
struct Member_t {
  Member_t *next;
  Member_t *prev;
  Context_clp c;
  bool_t  prune_after;
};

/* Merge itself */
#define NUM_CONTEXTS 	(10)
typedef struct Merge_st Merge_st;
struct Merge_st {
  struct _MergedContext_cl cl;		/* Closure itself 		*/
  TypeSystem_clp 	ts;		/* Type System			*/
  Member_t		contexts;	/* Linked list of contexts	*/
  uint32_t		size;		/* Maximum no. of contexts 	*/
  uint32_t		num;		/* Current no. of contexts 	*/
  Heap_clp 		h;		/* Heap used for workspace 	*/
};

/*
 * Statics
 */
static Member_t *FindContext(Merge_st *st,
			     string_t name,
			     Member_t *start,
			     /* RETURNS */
			     bool_t *compound );

/*
 * Merged contexts are complicated by the requirement to pass compound
 * path names as arguments. Compound path names should only refer to the
 * first context which contains the first component of the name. Hence
 * most context operations start by locating the first context
 * containing the first component.
 */


/********************************************************/
/*							*/
/*	Module call: create a new merged context	*/
/*							*/
/********************************************************/

/*
 * Get a new context
 */
MergedContext_cl *NewMerged(ContextMod_cl *self,
				   Heap_clp h,
				   TypeSystem_clp ts )
{
  Merge_st *st;
  
  TRC(eprintf("ContextMod$NewMerged : called.\n"));
  
  /* Create state record */
  st = Heap$Malloc( h, sizeof( *st ));
  if ( !st ) {
    DB(eprintf("ContextMod$NewMerged : failed to create state.\n"));
    RAISE_Heap$NoMemory();
  }
  st->size = NUM_CONTEXTS;
  st->num  = 0;
  st->h    = h;
  st->ts   = ts;

  LINK_INITIALISE(&(st->contexts));
  st->contexts.c = NULL;
  st->contexts.prune_after = False;
  CL_INIT(st->cl, &Merged_op, st );

  return &(st->cl);
}

/********************************************************/
/*							*/
/*	Internal calls				       	*/
/*							*/
/********************************************************/

/*
 * Search through the contexts for one that matches the first
 * component of the given name. Return a closure for the context, or
 * null if it was not found
 */
static Member_t *FindContext(Merge_st *st,
			     string_t name,
			     Member_t *start,
			     /* RETURNS */
			     bool_t *compound )
{
    uchar_t	dup[ strlen(name) + 1 ]; /* XXX - GCC scariness */ 
    string_t 	rest;
    Type_Any    	dummy;
    Member_t     *l = start;
    
    /* Take a copy of the string */
    strcpy(dup, name );
    
    /* Insert a zero terminator in the duplicate name where the first */
    /* separator would be, thus giving the first component in dup */
    rest = strpbrk(dup, SEP_STRING );
    if ( rest ) {
	*rest 	= '\0';
	*compound 	= True;
    } else {
	*compound   = False;
    }
    
    l = st->contexts.next;

    TRC(eprintf("FindContext: contexts list:\n"));

    while(l != &st->contexts) {
	TRC(eprintf("+ %p\n", l->c));
	l = l->next;
    }

    /* If no start specified, use first context */
    
    if(start) {
	if(start->prune_after) {
	    TRC(eprintf("FindContext: Search pruned after context %p\n",l->c));
	    return NULL;
	}
	l = start->next;
	TRC(eprintf("FindContext: Continuing search from context %p\n", l->c));
    } else {
	l = st->contexts.next;
	TRC(eprintf("FindContext: Starting search from context %p\n", l->c));
    }
    
    /* Look for dup in all the contexts */
    while( l != &(st->contexts))
    {
	TRC(eprintf("FindContext: Checking context %p\n", l->c));
	if ( Context$Get(l->c, dup, &dummy ) ) {
	    /* We've found it; return */
	    TRC(eprintf("FindContext: Found '%s'\n", dup)); 
	    return l;
	}
	l = l->next;
    }
    
    /* Failed to find the context, return null. */
    TRC(eprintf("MergedContext$FindContext: not found.\n"));
    return NULL;
}

/********************************************************/
/*							*/
/*	Context closure methods	       			*/
/*							*/
/********************************************************/

/* 
 * List the names in a merged context.
 * 
 * The algorithm is as follows: first create a scratch context.
 * Then for each context, get a list of names. 
 *  For each name, if it is already in the scratch context, do nothing. 
 * Otherwise, add it to the scratch context.
 *  When all the contexts have been dealt with, return the
 * result of the List operation on the scratch context and free the
 * scratch context.
 *
 * XXX better handling of exceptions required here. 
 */
static Context_Names *MergedList(Context_cl *self)
{
  Context_clp	 scratch;
  Merge_st	*st = (Merge_st *)(self->st);
  Context_Names *NOCLOBBER res;
  Type_Any	 dummy;
  Member_t	*l;
  int		 i;

  TRC(eprintf("Context$List: called\n"));
  
  /* Create a scratch context */
  scratch = NewContext(ContextMod, Pvs(heap), st->ts);
  if ( !scratch ) {
    DB(eprintf("Merge_List: failed to create scratch context.\n"));
    RAISE_Heap$NoMemory();
  };

  /* Iterate through contexts */
  TRY { 
    for( l = st->contexts.next; l != &(st->contexts); l = l->next ) {
      Context_Names *clist;
      
      clist = Context$List(l->c);
      
      /* Add all the new names in this context to the list */
      for( i = 0; i < clist->len; i++) {
	if (! (Context$Get(scratch, clist->data[i], &dummy ) ) ) {
	  Context$Add(scratch, clist->data[i], &dummy );
	}
	FREE (clist->data[i]);
      }
      
      /* Free up the context's list */
      SEQ_FREE (clist);
    }
    
    res = Context$List (scratch);
    
  }
  FINALLY {
    /* Free up the scratch context */
    Context$Destroy(scratch);
  }
  ENDTRY;

  /* Return the list */
  return res;
}

/*
 * Look up a name in the context
 */
static bool_t MergedGet(Context_cl *self,
			string_t name,
			/* RETURNS */
			Type_Any *o )
{
  Merge_st     *st = (Merge_st *)(self->st);
  Member_t     *m = NULL;
  bool_t	compound;
  
  TRC(eprintf("MergedContext$Get: look up '%s'\n", name));
  
  while((m = FindContext(st, name, m, &compound))) {
      
      TRC(eprintf("MergedContext$Get: searching in %p\n", m->c)); 
      if(Context$Get(m->c, name, o)) {
	  return True;
      }
      
  }
  
  TRC(eprintf("MergedContext$Get: '%s' not found\n", name));
  return False;
  
};
  

/*
 * Bind a name in a context (not necessarily this one!)
 */
static void MergedAdd(Context_cl *self,
		      string_t 	  name,
		const Type_Any 	 *obj )
{
  Merge_st     *st = (Merge_st *)(self->st);
  Member_t     *NOCLOBBER m = NULL;
  Context_clp ctxt;
  bool_t	compound;
  bool_t        NOCLOBBER done = False;

  TRC(eprintf("MergedContext$Add: bind '%s'\n", name));
  
  /* Find if the name might be in a context we have; if there is a */
  /* context, let it deal with the problem */ 

  while ((m = FindContext(st, name, m, &compound )) ) {
      
      /* If we get a Context$NotFound raised, trap it and try the
         next. Other exceptions are passed up as error conditions */

      TRY {
	  Context$Add(m->c, name, obj );
	  done = True;
      } CATCH_Context$NotFound(n) {
      } ENDTRY;

      if(done) return;

  }
  
  /* Otherwise, if it isn't a compound name, hand it to the first */
  /* context, assuming there is one. */
  if ( !compound ) {
      if(!LINK_EMPTY(&(st->contexts))) {
	  ctxt = st->contexts.next->c;
	  Context$Add(ctxt, name, obj );
	  return;
      } else {
	  RAISE_Context$NotContext();
      }
  } else {

      /* A compound name, and no one has found it for us */

      RAISE_Context$NotFound(name);
  }
  
}

/*
 * Remove a name from a context.
 */
static void MergedRemove(Context_cl *self,
		   string_t name )
{
  Merge_st     *st = (Merge_st *)(self->st);
  Member_t     *NOCLOBBER m = NULL;
  bool_t       compound;
  bool_t       NOCLOBBER done = False;

  TRC(eprintf("MergedContext$Remove: remove '%s'\n", name));

  while((m = FindContext(st, name, m, &compound))) {

      TRY {
	  Context$Remove(m->c, name);
	  done = True;
      } CATCH_Context$NotFound(n) {
      } ENDTRY;

      if (done) return;
      
  }
  
  RAISE_Context$NotFound(name);

};

/* 
 * Duplicate a MergedContext
 */
static Context_clp
MergedDup (Context_clp self, Heap_clp h, WordTbl_clp xl)
{
  Merge_st		*st = (Merge_st *)(self->st);
  Member_t		*l;
  MergedContext_clp	 res;
  bool_t	 	 xl_provided = (xl != NULL); /* false => we are root */

  res = NewMerged (NULL, h, st->ts);

  if (!xl)
  {
    WordTblMod_clp m = NAME_FIND ("modules>WordTblMod", WordTblMod_clp);
    xl = WordTblMod$New (m, Pvs(heap));
  }
  WordTbl$Put (xl, (WordTbl_Key) self, (WordTbl_Val) res);

  for (l = st->contexts.next; l != &(st->contexts); l = l->next)
  {
    MergedAddContext (res, Context$Dup (l->c, h, xl),
		      MergedContext_Position_After, 
		      l->prune_after, NULL);
  }

  if (!xl_provided) WordTbl$Destroy (xl);

  return (Context_clp) res;
}

/*
 * Destroy a merged context
 */
static void MergedDestroy( Context_cl *self )
{
  Merge_st *st = (Merge_st *)(self->st);

  /* First free all the links */
  while( !LINK_EMPTY(&(st->contexts))) {
    Member_t   *l = st->contexts.next;
    
    LINK_REMOVE(l);
    Heap$Free(st->h, l );
  }

  /* Free the structure itself */
  Heap$Free( st->h, st );
}

/*
 * Add a context to a merge 
 */
static void
MergedAddContext(MergedContext_cl		*self,
		 Context_clp		c	/* IN */,
		 MergedContext_Position	pos	/* IN */,
		 bool_t                 prune_after /* IN */,
		 Context_clp		c1	/* IN */ )
{
  Merge_st 	*st = (Merge_st *)(self->st);
  Member_t 	*l, *l1;

  for( l=st->contexts.next; l != &(st->contexts); l = l->next ) 
    {
      if ( l->c == c ) {
	DB(eprintf("MergedContext$AddContext: duplicate\n"));
	RAISE_Context$Exists();
      }
    }
  
  /* Find where to put "c" */

  if (c1)
  {
    for( l1=st->contexts.next; l1 != &(st->contexts); l1 = l1->next ) 
      if ( l1->c == c1 ) break;
    if (l1 == &(st->contexts))
      RAISE_Context$NotContext();
  }
  else
  {
    l1 = &(st->contexts);
    pos = !pos;
  }

  l = Heap$Malloc(st->h, sizeof(*l) );
  if (!l) {
    DB(eprintf("MergedContext$AddContext: failed in malloc.\n"));
    RAISE_Context$Denied();
  }
  l->c = c;
  l->prune_after = prune_after;

  if (pos == MergedContext_Position_Before)
  {
    LINK_INSERT_BEFORE(l1,l);
  }
  else
  {
    LINK_INSERT_AFTER(l1,l);
  }

  (st->num)++;
}
 
/*
 * Delete a context from a merge
 */
static void MergedDeleteContext( MergedContext_cl *self, Context_clp c )
{
  Merge_st *st = (Merge_st *)(self->st);
  Member_t 	*l;

  for( l=st->contexts.next; l != &(st->contexts); l = l->next ) 
    {
      if ( l->c == c ) {
	TRC(eprintf("MergedContext$DeleteContext: found\n"));
	LINK_REMOVE(l);
	Heap$Free(st->h, l );
	(st->num)--;
	return;
      }
    }
  DB(eprintf("MergedContext$DeleteContext: not found!\n"));
  RAISE_Context$NotContext();
}

static MergedContext_ContextList *MergedListContexts(MergedContext_cl *self)
{
    Merge_st *st = (Merge_st *)(self->st);
    MergedContext_ContextList *r;
    Member_t *l;

    r=SEQ_CLEAR(SEQ_NEW(MergedContext_ContextList, 0, Pvs(heap)));

    for (l=st->contexts.next; l!=&(st->contexts); l=l->next) {
	SEQ_ADDH(r,l->c);
    }

    return r;
}
