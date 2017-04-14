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
**      Clanger state & types
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Internal representation of Clanger data structures
** 
** ENVIRONMENT: 
**
**      User space domain
** 
** ID : $Id: Clanger_st.h 1.3 Thu, 20 May 1999 15:25:49 +0100 dr10009 $
** 
**
*/

#ifndef _Clanger_st_h_
#define _Clanger_st_h_

#include <Operation.ih>
#include <MergedContext.ih>
#include <ContextMod.ih>
#include <ContextUtil.ih>
#include <SpawnMod.ih>
#include <Spawn.ih>
#include <Clanger.ih>

typedef enum {
    NODE_LOOP,
    NODE_IF,
    NODE_FINALLY,
    NODE_RAISE,
    NODE_BREAK,
    NODE_INVOCST,
    NODE_NULLST,
    NODE_INVOCVAL,
    NODE_CONS,
    NODE_CONSLIST,
    NODE_BIND,
    NODE_BOOLEAN,
    NODE_UNARY,
    NODE_BINARY,
    NODE_IDENT,
    NODE_NUMBER,
    NODE_STRING,
    NODE_LITBOOL,
    NODE_LITNULL,
    NODE_CD, 
    NODE_PWD,
    NODE_LIST,
    NODE_PAUSE,
    NODE_EXEC,
    NODE_SPAWN,
    NODE_FORK,
    NODE_RM,
    NODE_KILL,
    NODE_HALT,
    NODE_MORE,
    NODE_INCLUDE,
} node_type;



typedef struct Lexer_st {
    char chars[ 1024 ];
    int  index;
    int  state;
    int  hexval;
} Lexer_st;

typedef struct Expr_t {
    struct Expr_t *next; 	/* LINK field */
    struct Expr_t *prev; 	/* LINK field */
    struct Expr_t *chain;	/* link for the st->parse_tree list */
    
    node_type major_type;
    uint32_t minor_type;
    
    uint64_t   val;
    
    struct Expr_t  *ch1;
    struct Expr_t  *ch2;
    struct Expr_t  *ch3;
    struct Expr_t  *ch4;
    struct Expr_t  *ch5;
} Expr_t;
#define NULL_Expr ((Expr_t *)0)

typedef struct Ctxt_List {
    struct Ctxt_List *next;
    struct Ctxt_List *prev;
    Context_clp c;
} Ctxt_List;

typedef struct LexStack_Item {
    struct LexStack_Item *prev;
    enum {
	LexStack_String,
	LexStack_Rd
    } kind;
    union {
	struct {
	    string_t str;
	    string_t strptr;
	} string;
	struct {
	    Rd_clp rd;
	} rd;
    } p;
} LexStack_Item;

#define LEXUNDOBUFFER 2

#define CURRENTLINEBUFFER 512

typedef struct Clanger_st {
    Lexer_st		lex;
    Expr_t		*ptr;	     /* parse root after a successful parse */
    Expr_t		*parse_tree; /* all nodes in the tree		    */
    bool_t		yydebug;     
    Wr_clp		wr;          /* output; Pvs(out) also used */

    MergedContext_clp	namespace;
    Ctxt_List		front;
    ContextMod_clp	cmod;
    ContextUtil_clp     cutil;
    SpawnMod_clp        spawnmod;
    Context_clp         current;    /* ctx in which relative paths are resolved */
    Context_clp         c; 
    Context_clp         pvs_cx;
    char                *cur_name;  /* textual name of above */
    Clanger_cl          closure; 
    LexStack_Item       *lexitem;
    char                last;
    bool_t              lastset;
    char                lexundo[LEXUNDOBUFFER];
    int                 lexundoindex ;
    char                currentline[CURRENTLINEBUFFER];
    int                 currentlineindex;
    int                 atendofline;
} Clanger_st;

typedef Clanger_st yynem_st; 

/*
 * Before synthesizing a call, Clanger assembles all the parameters
 * into an array of Par_t's. 
 */
typedef struct Par_t {
    Operation_ParamMode	mode;	/* Parameter passing mode 	*/
    Type_Code		tc;	/* type code, from signature	*/
    Type_Any		a;	/* any type of actual arg	*/
    Expr_t	       *lvalue;	/* Lvalue for results		*/
} Par_t;

/*
 * We slap a generic closure onto a clp to extract operation routine
 * addresses 
 */
typedef struct GenericClosure_t {
    addr_t *op;
    addr_t  st;
} GenericClosure_t;

extern const char * const node_names[];

/*
 * Lexer prototype
 */
extern int Lex( Clanger_st *st, Expr_t **val );
#define yylex(st, val) Lex((st), &((val)->express))

/*
 * Building parse tree nodes
 */

extern Expr_t *	MakeExprNode(Clanger_st *st, 
			     uint32_t major_type,
			     uint64_t val,
			     Expr_t  *ch1,
			     Expr_t  *ch2,
			     Expr_t  *ch3,
			     Expr_t  *ch4 );

extern void	FreeParse   (Clanger_st *st);


#define MakeLoopStmt(_st,_init_action,_text_expr,_loop_action,_loop_action2 )\
MakeExprNode(_st,NODE_LOOP,0,_init_action,_text_expr,_loop_action,_loop_action2);

#define MakeIfStmt(_st,_test_expr,_true_action,_false_action )\
MakeExprNode(_st,NODE_IF,0,_test_expr,_true_action,_false_action,NULL_Expr);

#define MakeTryFinallyStmt(_st,_main_action,_final_action )\
MakeExprNode(_st,NODE_FINALLY,0,_main_action,_final_action,NULL_Expr,NULL_Expr);

#define MakeRaiseStmt(_st,_exc_name,_param_list )\
MakeExprNode(_st,NODE_RAISE,0,_exc_name,_param_list,NULL_Expr,NULL_Expr);

#define MakeBreakStmt(_st) \
MakeExprNode(_st,NODE_BREAK,0,NULL_Expr,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeSpawnInvocation(_st,_child_name,_env,_invoc) \
MakeExprNode(_st,NODE_SPAWN,0,_child_name,_env,_invoc,NULL_Expr);

#define MakeInvocStmt(_st,_returns,_intf,_op,_arg_list) \
MakeExprNode(_st,NODE_INVOCST,0,_returns,_intf,_op,_arg_list);

#define MakeNullStmt(_st) \
MakeExprNode(_st,NODE_NULLST,0,0,0,0,0);

#define MakeIdentifier(_st,_val) \
MakeExprNode(_st,NODE_IDENT,(uint64_t)(word_t)(strdup(_val)),NULL_Expr,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeNumber(_st,_val) \
MakeExprNode(_st,NODE_NUMBER,_val,NULL_Expr,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeString(_st,_val) \
MakeExprNode(_st,NODE_STRING,(uint64_t)(word_t)(strdup(_val)),NULL_Expr,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeBoolean(_st,_op,_lside,_rside) \
MakeExprNode(_st,NODE_BOOLEAN,_op,_lside,_rside,NULL_Expr,NULL_Expr);

#define MakeLitBool(_st,_val) \
MakeExprNode(_st,NODE_LITBOOL,_val,NULL_Expr,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeLitNull(_st) \
MakeExprNode(_st,NODE_LITNULL,0, 0, 0, 0, 0);

#define MakeUnary(_st,_op,_lside) \
MakeExprNode(_st,NODE_UNARY,_op,_lside,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeBinary(_st,_op,_lside,_rside) \
MakeExprNode(_st,NODE_BINARY,_op,_lside,_rside,NULL_Expr,NULL_Expr);

#define MakeInvocVal(_st,_intf,_op,_arg_list,_filter)  \
MakeExprNode(_st,NODE_INVOCVAL,0,_intf,_op,_arg_list,_filter);

#define MakeContextConstructor(_st,_binds) \
MakeExprNode(_st,NODE_CONS,0,_binds,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeListConstructor(_st,_binds) \
MakeExprNode(_st,NODE_CONSLIST,0,_binds,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeBinding(_st,_lvalue,_rvalue ) \
MakeExprNode(_st,NODE_BIND,0,_lvalue,_rvalue,NULL_Expr,NULL_Expr);

#define MakeCDStmt(_st,_path) \
MakeExprNode(_st,NODE_CD,0,_path,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakePWDStmt(_st) \
MakeExprNode(_st,NODE_PWD,0,0,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeListStmt(_st,_path) \
MakeExprNode(_st,NODE_LIST,0,_path,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakePauseStmt(_st,_path) \
MakeExprNode(_st,NODE_PAUSE,0,_path,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeExecStmt(_st,_path,_parm) \
MakeExprNode(_st,NODE_EXEC,0,_path,_parm,NULL_Expr,NULL_Expr);

#define MakeForkStmt(_st,_cls,_stk) \
MakeExprNode(_st,NODE_FORK,0,_cls,_stk,NULL_Expr,NULL_Expr);

#define MakeHaltStmt(_st) \
MakeExprNode(_st,NODE_HALT,0,0,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeRMStmt(_st,_path) \
MakeExprNode(_st,NODE_RM,0,_path,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeMoreStmt(_st,_path) \
MakeExprNode(_st,NODE_MORE,0,_path,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeKillStmt(_st,_expr) \
MakeExprNode(_st,NODE_KILL,0,_expr,NULL_Expr,NULL_Expr,NULL_Expr);

#define MakeIncludeStmt(_st,_expr) \
MakeExprNode(_st, NODE_INCLUDE, 0,_expr,NULL_Expr, NULL_Expr, NULL_Expr);
/*
 * Executing interface operations
 */
extern void Clanger_Eval	(Clanger_st *st, Expr_t *e,
				 /* OUT */ Type_Any *a );

extern void Clanger_Statement	(Clanger_st *st, Expr_t *stmtlst );

extern void Clanger_Function	(Clanger_st *st,
				 string_t    ifname,
				 string_t    opname,
				 Expr_t     *arglist,
				 Expr_t     *reslist,
				 string_t    filter,
				 bool_t      spawn,
				 string_t    domname,
				 Expr_t     *env,
				 Type_Any   *res);

extern bool_t Clanger_Assign	(Clanger_st *st,
				 Type_Any *lval, Type_Any *val);

extern void   TerminateShell    ();

/* these probably belong in TypeSystem */
extern void   	 UnAlias	(Type_Any *a);
extern Type_Code UnAliasCode	(Type_Code tc);

/* Lexer interface */

void NewLexString(Clanger_st *st, string_t str);
void NewLexRd    (Clanger_st *st, Rd_clp rd);
char LexGetChar(Clanger_st *st);
void LexUnGetChar(Clanger_st *st);
bool_t LexEOF(Clanger_st *st);
#endif /* _Clanger_st_h_ */


