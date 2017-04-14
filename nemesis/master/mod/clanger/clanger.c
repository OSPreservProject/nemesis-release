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
** ID : $Id: clanger.c 1.3 Thu, 20 May 1999 15:25:49 +0100 dr10009 $
**
**
*/

#include <stdio.h>
#undef EOF

#include <nemesis.h>
#include <exceptions.h>

#include <Clanger.ih>
#include <Clanger_st.h>


#include <ctype.h> /* for isspace */

bool_t yyparse( yynem_st *yy_st );

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif
#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED", __FUNCTION__);


static	Clanger_ExecuteString_fn 		Clanger_ExecuteString_m;
static	Clanger_ExecuteRd_fn 		Clanger_ExecuteRd_m;
static  Clanger_GetRoot_fn              Clanger_GetRoot_m;
static	Clanger_Destroy_fn 		Clanger_Destroy_m;

Clanger_op	clanger_ms = {
  Clanger_ExecuteString_m,
  Clanger_ExecuteRd_m,
  Clanger_GetRoot_m,
  Clanger_Destroy_m
};

    


/*---------------------------------------------------- Entry Points ----*/

/* XXX ; ExecuteString still has a few problems */

static void Clanger_ExecuteString_m (
	Clanger_cl	*self,
	string_t	str	/* IN */,
	Wr_clp	wr	/* IN */ )
{
  Clanger_st	*st = (Clanger_st *) self->st;
  char *cptr;
  Wr_clp oldout;
  int i;


  cptr = str;

  st->lex.chars[0] = '\0';
  st->lex.index  = 0;
  st->lex.state  = 0;
  st->lex.hexval = 0;
  NewLexString(st, str);
  oldout         = Pvs(out);
  Pvs(out)       = wr;
  

  for (i=0; !LexEOF(st); i++) {
      bool_t b;
      st->ptr = st->parse_tree = NULL;
      b = yyparse( st );
      if ( !b ) {
	  TRY {
	      Clanger_Statement(st, st->ptr );
	  } CATCH_Context$NotFound(name) {
	      wprintf(wr, "Clanger$ExecuteString: Exception: Context$NotFound "
		      "(lookup of %s failed)\n", name);
	      
	  } CATCH_ALL {
	      wprintf(wr, "Clanger$ExecuteString: Exception `%s' caught.\n",
		      exn_ctx.cur_exception );
	  } ENDTRY;
      } else {
	  wprintf(wr, "Clanger syntax error. Bad line is:\n\"%s\"\n", st->currentline);
      }
      FreeParse (st);  
  }
  Pvs(out) = oldout;
}
#if 0
static void Clanger_ExecuteRd_m (
	Clanger_cl	*self,
	Rd_clp	rd	/* IN */,
	Wr_clp	wr	/* IN */ )
{
  Clanger_st	*st = (Clanger_st *) self->st;
  int i;
  Wr_clp oldout;

  st->lex.chars[0] = '\0';
  st->lex.index  = 0;
  st->lex.state  = 0;
  st->lex.hexval = 0;

  NewLexRd(st,rd);
  st->wr = wr;
  oldout = Pvs(out);
  Pvs(out) = st->wr;

  for (i=0; !LexEOF(st); i++) {
      bool_t b;

      st->ptr = st->parse_tree = NULL;
      b = yyparse( st );
      
      if ( !b ) {
	  TRY {
	      Clanger_Statement(st, st->ptr );
	  } CATCH_Context$NotFound(name) {
	      wprintf(wr, "Exception: Context$NotFound "
		      "(lookup of %s failed)\n", name);
	      
	  } CATCH_ALL {
	      wprintf(wr, "Exception `%s' caught.\n",
		      exn_ctx.cur_exception );
	  } ENDTRY;
      } else
	  wprintf(wr, "Syntax Error.\n");
      FreeParse (st);  
  }
  Pvs(out) = oldout;
}
#endif /* 0 */

#define BUFFERSIZE 16384

static void Clanger_ExecuteRd_m( Clanger_cl *self, Rd_clp rd, Wr_clp wr) {
    char *buf;
    int hitend = 0;
    buf = malloc(sizeof(char)*BUFFERSIZE);
    while (!hitend) {
	int numread;
	numread = Rd$GetLine(rd, buf, BUFFERSIZE-2);
	if (numread == 0) {
	    hitend = 1;
	}
	if (numread >= BUFFERSIZE-2) {
	    Wr$PutStr(wr, "ERROR: clanger line buffer overflow!\n");
	    hitend = 1;
	} else {
	    buf[numread] = 0;
	    Clanger_ExecuteString_m(self, buf, wr);
	}
    }
    free(buf);
}

static Context_clp Clanger_GetRoot_m(Clanger_cl *self) {
      Clanger_st	*st = (Clanger_st *) self->st;
      return st->c;
}

static void Clanger_Destroy_m (
	Clanger_cl	*self )
{
  Clanger_st	*st = (Clanger_st *) self->st;

  Context$Destroy(st->c);
  Context$Destroy(st->pvs_cx);
  Context$Destroy((Context_clp)(st->namespace));
  Heap$Free(Pvs(heap),st);
  return ;
}

/*
 * End 
 */
