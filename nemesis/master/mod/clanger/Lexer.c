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
**      Lexer.c
**
** FUNCTIONAL DESCRIPTION:
**
**      Lexical analyser for the Clanger command language.
**
** ENVIRONMENT:
**
**      Internal to Clanger. User space.
**
** ID : $Id: Lexer.c 1.3 Thu, 20 May 1999 15:25:49 +0100 dr10009 $
**
**
*/
#include <stdio.h>
#undef EOF
#include <nemesis.h>
#include <exceptions.h>
#include <ctype.h>
#include <stdlib.h>

#include <Rd.ih>
#include <Wr.ih>

#include <Clanger_st.h>
#include <Clammar.tab.h>


#define EOFCHAR -1

#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x)
#endif

#ifdef LEX_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define LEXTRC(_x)

#define ADD_CHAR(_c) if (st->lex.index < BUF_SIZE ) \
			 st->lex.chars[st->lex.index++] = _c
#define BACK_UP (LexUnGetChar(st))
#define GETCHAR() (LexGetChar(st))
#define NEW_STATE( _st ) st->lex.state = _st
#define DISPATCH_STATE(_c,_s) \
      if ( c == (_c) ) {	\
         NEW_STATE( (_s) );	\
	 break;			\
      }
#define TWO_CHAR_TOKEN(_c1,_c2,_tk) \
      NEW_STATE( STATE_START );	\
      if ( c == (_c2) ) {	\
        return (_tk);		\
      } else {			\
	BACK_UP;	      	\
	return (_c1);		\
      }				\
      break;

#define HEXVAL(_c) (( (_c) > '9' ) ? ( toupper(_c)-'A' ) : (_c)-'0' )


#define BUF_SIZE (1024)



typedef enum state_t {
    STATE_START,
    STATE_IDENTIFIER,
    STATE_DECIMAL,
    STATE_ZERO,
    STATE_QUOTE,
    STATE_STRING,
    STATE_HEX,
    STATE_ESCAPE,
    STATE_ESCHEX,
    STATE_EQUALSIGN,
    STATE_PLINGSIGN,
    STATE_LEFTANGLE,
    STATE_AMPERSAND,
    STATE_VERTBAR,
    STATE_TIME_NANO,
    STATE_TIME_MICRO,
    STATE_TIME_MILI,
    STATE_STAR
} state_t;

static const char * const state_names[] = {
    "Start",
    "Identifier",
    "Decimal",
    "Zero",
    "String",
    "Hex",
    "Escape",
    "EscHex",
    "EqualSign",
    "PlingSign",
    "RightAngle",
    "LeftAngle",
    "Ampersand",
    "VertBar"
};

static const char esc_table[][2] = {
    { 'n', '\n' },
    { 't', '\t' },
    { 'v', '\v' },
    { 'b', '\b' },
    { 'r', '\r' },
    { 'f', '\f' },
    { 'a', '\a' },
    { '\\', '\\' },
    { '?', '\?' },
    { '\'', '\'' },
    { '\"', '\"' },
};

/* Keep emacs font lock happy: " */

#define NUM_ESCS (sizeof(esc_table) / 2)

typedef struct keyword_t keyword_t;
struct keyword_t {
    char *word;
    int   tok;
};

/*
** XXX SMH: this is getting far too long - all the 'nice' things like
** ls, exec, rm, kill, etc, prob should be stuck into the namespace 
** somewhere as .. em... LDLUMPs? modules? anyway, should be available
** to be Thread_Fork()'d by the shell.
*/
static const struct keyword_t keyword_table[] = {
    { "break",	 TK_BREAK },
    { "case",	 TK_CASE },
    { "catch",	 TK_CATCH },
    { "catchall", TK_CATCHALL },
    { "cd",      TK_CD },
    { "def",	 TK_DEF },
    { "do",	 TK_DO },
    { "else",	 TK_ELSE },
    { "exec",    TK_EXEC }, 
    { "exit",    TK_EXIT },
    { "finally", TK_FINALLY },
    { "for",	 TK_FOR },
    { "halt",	 TK_HALT },
    { "if",	 TK_IF },
    { "kill",	 TK_KILL },
    { "ls",      TK_LS }, 
    { "more",    TK_MORE }, 
    { "pwd",     TK_PWD },
    { "quit",    TK_EXIT },
    { "raise",	 TK_RAISE },
    { "return",	 TK_RETURN },
    { "rm",	 TK_RM },
    { "run",     TK_EXEC }, 
    { "switch",	 TK_SWITCH },
    { "try",	 TK_TRY },
    { "while",	 TK_WHILE },
    { "true",    TK_TRUE },
    { "false",   TK_FALSE },
    { "null",    TK_NULL },
    { "pause",   TK_PAUSE },
    { "include", TK_INCLUDE },
    { "spawn",   TK_SPAWN },
    { "threadfork", TK_FORK },
    
    { "\0", 0 }};

char esc_translate( char c )
{
    int i;

    for( i=0; i < NUM_ESCS; i++) {
	if ( c == esc_table[i][0] ) return esc_table[i][1];
    }
    return c;
}

int keyword_translate( char *s )
{
    const keyword_t *k;

    for( k = keyword_table; k->tok != 0; k++ )
	if (!strcmp( s, k->word )) return k->tok;
    return TK_IDENTIFIER;
}
#if 0
#define EOF -1
#endif /* 0 */

int Lex( Clanger_st *st, Expr_t **val)
{
    int c = 0;
    int token;

    st->lex.index  = 0;
    st->lex.state  = STATE_START;
    st->lex.hexval = 0;

    while( c != EOFCHAR ) {

	c = GETCHAR();
	switch( st->lex.state ) {

	case STATE_START:

	    if (c=='#') {
		while (c != '\n' && c != EOFCHAR) {
		    c = GETCHAR();
		}
		BACK_UP;
		break;
	    }


	    if ( isalpha(c) || c == '_' ) {
		ADD_CHAR(c);
		NEW_STATE( STATE_IDENTIFIER );
		break;
	    }
	    if ( c == '0' ) {
		ADD_CHAR(c);
		NEW_STATE( STATE_ZERO );
		break;
	    }
	    if ( isdigit(c) ) {
		ADD_CHAR(c);
		NEW_STATE( STATE_DECIMAL );
		break;
	    }
		
		

	    if( c == '>' ) { /* leading >'s are tricky */
		int n = 0;
		switch((n = GETCHAR())) {
		  case '=': return TK_GE;
		  case '>': return TK_GT; /* XXX SMH: need to use >> for GT */
		  default : 
		      ADD_CHAR(c); 
		      ADD_CHAR(n);
		      NEW_STATE( STATE_IDENTIFIER );
		}
		break;
	    }

	    DISPATCH_STATE('\'', STATE_QUOTE );
	    DISPATCH_STATE('"', STATE_STRING );
	    DISPATCH_STATE('=', STATE_EQUALSIGN );
	    DISPATCH_STATE('!', STATE_PLINGSIGN );
	    DISPATCH_STATE('<', STATE_LEFTANGLE );
	    DISPATCH_STATE('&', STATE_AMPERSAND );
	    DISPATCH_STATE('|', STATE_VERTBAR );
	    DISPATCH_STATE('*', STATE_STAR );
	    if ( c == '\n' ) {
		/* XXX check Parser state at this point? */
		TRC(printf("Generating TK_EOL\n"));
		
		return TK_EOL;
	    }

	    if ( isspace(c) ) {
		/* Do nothing */
	    } else {
		return c;
	    }
	    break;

	case STATE_IDENTIFIER:

	    if ( isalnum(c) || c == '_' || c == '>' || c == '/' ) {
		ADD_CHAR(c);
	    } else {
		BACK_UP;
		ADD_CHAR('\0');
		NEW_STATE( STATE_START );
		token = keyword_translate( st->lex.chars );
		switch (token)
		{
		case TK_IDENTIFIER:
		    *val = MakeIdentifier(st, st->lex.chars ); break;
		case TK_TRUE:
		case TK_FALSE:
		    *val = MakeLitBool(st, token == TK_TRUE); break;
		case TK_NULL:
		    *val = MakeLitNull(st); break;
		}
		return token;
	    }
	    break;

	case STATE_ZERO:

	    if ( c == 'x' ) {
		ADD_CHAR(c);
		NEW_STATE( STATE_HEX );
	    } else if ( isdigit(c) ) {
		ADD_CHAR(c);
		NEW_STATE( STATE_DECIMAL );
	    } else {
		BACK_UP;
		ADD_CHAR('\0');
		*val = MakeNumber(st, 0 );
		NEW_STATE( STATE_START );
		return TK_NUMBER;
	    }
	    break;
	case STATE_TIME_MILI:
	case STATE_TIME_MICRO:
	case STATE_TIME_NANO:
	    if (c=='s') {
		uint64_t mult;
		mult = 1;
		if (st->lex.state == STATE_TIME_MILI) mult = 1000000;
		if (st->lex.state == STATE_TIME_MICRO) mult =   1000;
		*val = MakeNumber(st, strtou64(st->lex.chars,0,10) * mult );
		NEW_STATE( STATE_START );
		return TK_NUMBER;
		break;
	    }
	    NEW_STATE(STATE_START);
	    break;

	case STATE_DECIMAL:
	    if (c=='m') {
		NEW_STATE(STATE_TIME_MILI);
		break;
	    } 
	    if (c=='u') {
		NEW_STATE(STATE_TIME_MICRO);
		break;
	    }
	    if (c=='n') {
		NEW_STATE(STATE_TIME_NANO);
		break;
	    }
	    if (c=='s') {
		BACK_UP;
		ADD_CHAR('\0');
		*val = MakeNumber(st, strtou64(st->lex.chars,0,10) * 1000000000 );
		NEW_STATE( STATE_START );
		return TK_NUMBER;
	    }
		
	    if ( isdigit(c) ) {
		ADD_CHAR(c);
	    } else {
		uint64_t n;
		BACK_UP;
		ADD_CHAR('\0');
		*val = MakeNumber(st, n=strtou64(st->lex.chars,0,10) );
		NEW_STATE( STATE_START );
		return TK_NUMBER;
	    }
	    break;

	case STATE_HEX:

	    if ( isxdigit(c) ) {
		ADD_CHAR(c);
	    } else {
		BACK_UP;
		ADD_CHAR('\0');
		*val = MakeNumber(st, strtou64(st->lex.chars,0,16) );
		NEW_STATE( STATE_START );
		return TK_NUMBER;
	    }
	    break;

	case STATE_QUOTE:

	    if ( c == '\'' ) {
		ADD_CHAR('\0');
		*val = MakeIdentifier(st, st->lex.chars);
		NEW_STATE( STATE_START );
		return TK_IDENTIFIER;
	    } else {
		ADD_CHAR(c);
	    }
	    break;

	case STATE_STRING:

	    if ( c == '\\' ) {
		NEW_STATE( STATE_ESCAPE );
	    } else if ( c == '"' ) {
		ADD_CHAR('\0');
		*val = MakeString(st, st->lex.chars);
		LEXTRC(printf("Parsed string [%s]\n", st->lex.chars));
		NEW_STATE( STATE_START );
		return TK_STRING;
	    } else if ( isprint( c ) ) {
		ADD_CHAR(c);
	    }
	    break;

	case STATE_ESCAPE:

	    if ( c == 'x' ) {
		st->lex.hexval = 0;
		NEW_STATE( STATE_ESCHEX );
	    } else {
		ADD_CHAR( esc_translate(c) );
		NEW_STATE( STATE_STRING );
	    }
	    break;

	case STATE_ESCHEX:

	    if ( isxdigit(c) ) {
		st->lex.hexval = (st->lex.hexval << 4) + HEXVAL(c);
	    } else {
		BACK_UP;
		ADD_CHAR(st->lex.hexval);
		NEW_STATE( STATE_STRING );
	    }
	    break;

	case STATE_EQUALSIGN:
	    TWO_CHAR_TOKEN('=','=',TK_EQ);

	case STATE_PLINGSIGN:
	    TWO_CHAR_TOKEN('!','=',TK_NE);

	case STATE_LEFTANGLE:
	    NEW_STATE( STATE_START );
	    switch (c) {
	    case '=': return TK_LE;
	    case '|': return TK_CONSL;
	    case '*': return TK_LISTL;
	    default:  BACK_UP; return '<';
	    }
	    break;

	case STATE_AMPERSAND:
	    TWO_CHAR_TOKEN('&','&',TK_AND);

	case STATE_STAR:
	    TWO_CHAR_TOKEN('*','>',TK_LISTR);

	case STATE_VERTBAR:
	    NEW_STATE( STATE_START );
	    switch (c) {
	    case '|': return TK_OR;
	    case '>': return TK_CONSR;
	    default:  BACK_UP; return '|';
	    }
	    break;

	default:
	    return 'c';
	}

    }
    return 0;
}

void NewLexString(Clanger_st *st, string_t str) {
    LexStack_Item *old;

    old = st->lexitem;
    st->lexitem = Heap$Malloc(Pvs(heap),sizeof(LexStack_Item)); 
    /* destroyed by lexer when exhausted */
    st->lexitem->kind = LexStack_String;
    st->lexitem->p.string.str = str;
    st->lexitem->p.string.strptr = str;
    st->lexitem->prev = old;
    st->currentlineindex = 0;    st->currentline[0] = 0; st->atendofline = 0;
    LEXTRC(fprintf(stderr,"Creating LexString at %x, old = %x\n", st->lexitem, old));
}
    
void NewLexRd(Clanger_st *st, Rd_clp rd) {
    LexStack_Item *old;
 
    old = st->lexitem;
    st->lexitem = Heap$Malloc(Pvs(heap),sizeof(LexStack_Item)); 
    /* destroyed by lexer when exhausted */
    st->lexitem->kind = LexStack_Rd;
    st->lexitem->p.rd.rd = rd;
    st->lexitem->prev = old;
    st->currentlineindex = 0;    st->currentline[0] = 0; st->atendofline = 0;

    LEXTRC(fprintf(stderr,"Creating LexString at %x, old = %x\n", st->lexitem, old));
    
}

char LexPop(Clanger_st *st) {

 
    if (st->lexitem->prev) {
	LexStack_Item *prev;

	prev = st->lexitem->prev;
	Heap$Free(Pvs(heap),prev);
	LEXTRC(fprintf(stderr,"Popping down to %x from %x\n", prev, st->lexitem));
	
	st->lexitem = prev;

	return LexGetChar(st);
    } else {
	fprintf(stderr,"LexPop: Stack empty\n");
	
	return EOFCHAR;
    }
}

char LexGetChar(Clanger_st *st) {
    char NOCLOBBER ch;

    LEXTRC(fprintf(stderr,"LexGetChar called, lexitem = %x, kind = %d\n", st->lexitem, st->lexitem->kind));
    
    if (st->lexundoindex) {
	ch = st->lexundo[--st->lexundoindex];
    } else {


    switch (st->lexitem->kind) {
    case LexStack_String:
	LEXTRC(fprintf(stderr,"LexGetChar on string, lexitem = %x\n", st->lexitem));
	
	if (st->lexitem->p.string.strptr) {
	    ch =*(st->lexitem->p.string.strptr++);
	    LEXTRC(printf("LexGetChar got [%c]\n", ch));
	    
	} else {
	    LEXTRC(fprintf(stderr,"LexGetChar got EOF, popping\n"));
	    
	    ch= LexPop(st);
	}
	break;
    case LexStack_Rd:
	LEXTRC(fprintf(stderr,"LexGetChar on rd, lexitem = %x\n", st->lexitem));
	TRY {
	    ch = Rd$GetC(st->lexitem->p.rd.rd);
	    LEXTRC(printf("LexGetChar got [%c]\n", ch));
	    
	} CATCH_Rd$EndOfFile() {
	    LEXTRC(fprintf(stderr,"LexGetChar got EOF, popping\n"));
	    
	    ch = LexPop(st);
	} ENDTRY;
	
	break;
    }
    LEXTRC(fprintf(stderr,"LexGetChar returns [%c] (%d)\n", ch, ch));
    st->lastset = 1;
    
    st->last = ch;
    }
    if (st->atendofline) {
	st->currentlineindex = 0;
	st->atendofline = 0; 
	LEXTRC(printf("Finishing line; line was %s\n", st->currentline));
    }	
    if (st->currentlineindex < CURRENTLINEBUFFER-1) {
	    st->currentline[st->currentlineindex++] = ch;
	    st->currentline[st->currentlineindex] = 0;	    
    }

    if (ch == '\n') {
	st->atendofline = 1;
    } 
    return ch;
}

void LexUnGetChar(Clanger_st *st) {
    if (st->currentlineindex>0) st->currentlineindex --;

    if (st->lexundoindex < LEXUNDOBUFFER && st->lastset) {
	st->lexundo[st->lexundoindex++] = st->last;
	st->lastset = 0;
	return;
    }
    
    switch (st->lexitem->kind) {
    case LexStack_String: 
	if (st->lexitem->p.string.strptr > st->lexitem->p.string.str) {
	    st->lexitem->p.string.strptr --;
	} else {
	    fprintf(stderr,"Attempt to advance before the start of a string\n")
;
	}
	break;
    case LexStack_Rd:
	Rd$UnGetC(st->lexitem->p.rd.rd);
	break;
    }
}

bool_t LexEOF(Clanger_st *st) {
    
    LEXTRC(fprintf(stderr,"LexEOF called, l = %x l->kind=%d\n", st->lexitem, (int) (st->lexitem->kind)));
    if  (st->lexundoindex) return False;
    
    switch (st->lexitem->kind) {
    case LexStack_String: 
	LEXTRC(fprintf(stderr,"LexEOF on string pointer %x -> %c\n", st->lexitem->p.string.strptr, *(st->lexitem->p.string.strptr)));
	
	if (*st->lexitem->p.string.strptr) {
	    LEXTRC(fprintf(stderr,"Got a char; returning False\n"));
	    
	    return False;
	}
	else {
	    
	    LEXTRC(fprintf(stderr,"Got a null; popping\n"));
	}
	
	    
	break;
    case LexStack_Rd: 
    {
	Rd_clp rd = st->lexitem->p.rd.rd;
	bool_t r;
	

	r = Rd$EOF(rd);
	
	if (!r) return False;
	break;
    }
    
    }
    /* this lexitem is exhausted; pop it */
    if (st->lexitem->prev) {
	
	st->lexitem = st->lexitem->prev;
	/* and tail-recurse on the new reader */
	return LexEOF(st);
    } else {
	return True;
    }
}		
