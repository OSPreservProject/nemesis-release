%{
#include <nemesis.h>
#include <links.h>

#include <Rd.ih>
#include <Wr.ih>

#include <Clanger_st.h>

#ifdef CLANGER_DEBUG
#	define DB(_x)	_x
#	define YYDEBUG	1
#else
#	define DB(X)	
#	undef YYDEBUG
#endif
%}

%token <keyword>	TK_BREAK 
%token <keyword>	TK_CASE
%token <keyword>	TK_CATCH
%token <keyword>	TK_CATCHALL
%token <keyword>	TK_CD
%token <keyword>	TK_DEF
%token <keyword>	TK_DO
%token <keyword>	TK_ELSE
%token <keyword>	TK_EXEC
%token <keyword>	TK_SPAWN
%token <keyword>	TK_FORK
%token <keyword>	TK_EXIT
%token <keyword>	TK_FINALLY
%token <keyword>	TK_FOR
%token <keyword>	TK_HALT
%token <keyword>	TK_IF
%token <keyword>	TK_KILL
%token <keyword>	TK_LS
%token <keyword>	TK_MORE
%token <keyword>        TK_PAUSE
%token <keyword>	TK_PWD
%token <keyword>	TK_RAISE
%token <keyword>	TK_RETURN 
%token <keyword>	TK_RM
%token <keyword>	TK_SWITCH 
%token <keyword>	TK_TRY 
%token <keyword>	TK_WHILE 
%token <keyword>	TK_CONSL
%token <keyword>	TK_CONSR
%token <keyword>        TK_LISTL
%token <keyword>        TK_LISTR
%token <keyword>	TK_EOL
%token <keyword>        TK_INCLUDE

%token <express>	TK_IDENTIFIER 
%token <express>	TK_NUMBER
%token <express> 	TK_FLOAT 
%token <express> 	TK_STRING
%token <express>	TK_TRUE
%token <express>	TK_FALSE
%token <express>        TK_NULL

%nonassoc <operator>	'=' 

%nonassoc <operator>	TK_TK_MUL_ASSIGN 
%nonassoc <operator>	TK_TK_DIV_ASSIGN 
%nonassoc <operator>	TK_TK_MOD_ASSIGN
%nonassoc <operator>	TK_ADD_ASSIGN
%nonassoc <operator>	TK_SUB_ASSIGN
%nonassoc <operator>	TK_LEFT_ASSIGN
%nonassoc <operator>	TK_RIGHT_ASSIGN
%nonassoc <operator>	TK_AND_ASSIGN
%nonassoc <operator>	TK_XOR_ASSIGN
%nonassoc <operator>	TK_OR_ASSIGN 

%left <operator>	TK_OR
%left <operator> 	TK_AND
%left <operator>	'|'
%left <operator>	'^'
%left <operator>	'&'
%left <operator>	TK_EQ TK_NE
%left <operator>	'<' TK_GT TK_LE TK_GE
%left <operator>	TK_LEFT TK_RIGHT
%left <operator>	'-' '+' 
%left <operator>	'*' '/' '%' 
%nonassoc <operator>	TKF_UMINUS TKF_UPLUS TKF_REF TKF_DEREF TKF_NULL
%right <operator>	'!' '~'

%type <express> StatementList Statement StatementPossiblyNull Expr Invocation Primary LValue
%type <express> InvocValue SpawnValue BExprList ExprList BLValueList LValueList Input
%type <express> Constructor BindingList Binding ConsBinding

%union {
  int operator;
  Expr_t *express;
  int keyword;
}

%pure_parser
%start Input

%%

/* LaTeX documentation starts here */
/* --
\documentclass[a4paper]{report}
\usepackage{a4wide}
\usepackage{epsfig}

\newcommand{\nodec}[1]{
  \section{#1}
  \subsection*{Grammar:}
}
\newcommand{\examplec}[1]{ 
  \subsection*{Example:} 	
  \begin{quote}
  {\texttt #1 }
  \end{quote}
}
\newcommand{\syntaxc}[1]{
  \subsection*{Syntax:}
  \begin{quote}
  \texttt{#1}
  \end{quote}
}
\newcommand{\desc}[1]{
  \subsection*{Description:}
  #1
}
\newcommand{\notec}[1]{
  \subsection*{Notes:}
  #1
}
\author{Dickon Reed}
\renewcommand{\ttdefault}{cmtt} % Don't like courier

\begin{document}
\title{
  \epsfig{file=NemKernel.ps,width=5in} 
  \vspace{1.5in}
  \Huge{Clanger Reference manual} \\
}
\maketitle
\tableofcontents




\chapter{Introduction}

\textbf{\emph{\large This document is a draft. Please correct any
problems or report to Dickon.Reed@cl.cam.ac.uk any mistakes in this
documentation.  }}

Clanger is an interpreted systems programming language that interacts
with various parts of Nemesis in order to provide high low level
interaction with the operating system.

Clanger itself is implemented as a module that exports a factory
interface (see \texttt{ClangerMod.if} that generates clanger
interpreter closures.

Presently, clanger is used as the standard scripting language of
Nemesis; at the end of initialisation a clanger script called
\texttt{init.rc} is invoked to customise the rest of startup.

There is a facility (see \texttt{include}) for a clanger script to
switch to reading things from the console. Therefore, clanger may be
used interactively.

Clanger is dynamically typed and dynamically bound. Therefore, it is
an evaluation time error if an identifier is unbound or types are
inappropiate.


This chapter is automatically generated from the clanger grammar itself.

In the syntax descriptions below:

\begin{itemize}
\item s1, s2.. represent well formed statements.
\item e1, e2.. represent well formed expressions.
\item exn1, exn2..    represent qualified exception names (ie InterfaceName\$ExecptionName).
\item i1, i2.. represent names of things in clanger's namespace.
\end{itemize}

Clanger interpretation units are lists of statements. So therefore,
the grammar begins:

\begin{verbatim} */

Input
	: StatementList { DB(printf("Got a StatementList\n")); } TK_EOL {
	  	$$ = $1; yy_st->ptr = $$; DB(printf("Falling through YYACCEPT\n")); YYACCEPT; }
| error TK_EOL {
	  	$$ = 0; yy_st->ptr = $$; DB(printf("Falling through YYABORT\n")); YYABORT; }
	;

/*--
\end{verbatim}
(The second rule there defines an error clause for the grammar; it is
not a part of the language).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\chapter{Statements}

Note that statements are separated by semicolons.



%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{While}
\begin{verbatim}
*/
			   
Statement
        : TK_WHILE '(' Expr ')' Statement {
	    $$ = MakeLoopStmt(yy_st, NULL_Expr, $3, $5, NULL_Expr ); }
/*--
\end{verbatim}
\syntaxc{while (e1) s1}
 Evaluate Expr. If it is false, the statement completes. If it is
true, Statement is exectued and the whole statement repeats. 
\examplec{ while true wr\$PutStr["Mwhaha! "] }


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Do While}
\begin{verbatim}
*/
	| TK_DO Statement TK_WHILE '(' Expr ')' {
	    $$ = MakeLoopStmt(yy_st, $2, $5, $2, NULL_Expr ); }
/*--
\end{verbatim}
\syntaxc{do s1 while e1}
\desc{
\begin{enumerate}
\item Execute s1
\item Evaluate e1. If it false, finish the statement.
\item Repeat from step 1.
\end{enumerate}
}
\examplec{ do bar\$BuyRound while (true) }


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{For}
\begin{verbatim}
*/

	| TK_FOR '(' StatementPossiblyNull ';' Expr ';' StatementPossiblyNull ')' Statement {
	    $$ = MakeLoopStmt(yy_st, $3, $5, $7, $9 ); }	
/*--
\end{verbatim}
\syntaxc{ for ( s1 ; e1 ; s2 ) s3 }
\desc{C style FOR statement. This does:
\begin{enumerate}
\item Execute s1.
\item Evaluate e1. If e1 is false, finish statement
\item Execute s3
\item Execute s2
\item Repeat from step 2.
\end{enumerate}
}




%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Compound}
\begin{verbatim}
*/
	| '{' StatementList '}' {
	    $$ = $2; }
/*--
\end{verbatim}
\syntaxc{ \{ s1; s2; s3... \} }
\desc{Execute each statement in the list in order. A StatementList may
be empty, so \{\} is a valid statement.}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Matched if}
\begin{verbatim}
*/
	| TK_IF '(' Expr ')' Statement TK_ELSE Statement {
	    $$ = MakeIfStmt(yy_st, $3, $5, $7 ); }
/*--
\end{verbatim}
\syntaxc{ if (expr) s1 else s2 }
If Expr evaluates to true, execute the first statement otherwise
execute the second statement. There is no unmatched if statement; if
you don't want anything to happen when Expr evaluates to false, have the
second statement be \{\}.


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Try Finally}
\begin{verbatim}
*/
	| TK_TRY StatementPossiblyNull TK_FINALLY Statement {
	    $$ = MakeTryFinallyStmt(yy_st, $2, $4 ); }
/*--
\end{verbatim}
\syntaxc{try s1 finally s2}
\desc{Execute s1. Then execute s2, whether or not
the s1 raised an exception. Any exception raised by s1 or s2 remains raised.}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Raise exception with arguments}
\begin{verbatim}
*/
	| TK_RAISE TK_IDENTIFIER BExprList {
	    $$ = MakeRaiseStmt(yy_st, $2, $3 ); }
/*--
\end{verbatim}
\syntaxc{raise exn1 b}
\desc{Raise exeception exn1. The first argument is an exception name (for
instance Rd\$EndOfFile). The second argument is the list of expressions
which will constitute the exception arguments.}
\examplec{raise Rd\$EndofFile [ weirdtypecode ]}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Raise exception without arguments}
\begin{verbatim}
*/
	| TK_RAISE TK_IDENTIFIER {
	    $$ = MakeRaiseStmt(yy_st, $2, NULL_Expr ); }
/*--
\end{verbatim}
\syntaxc{raise exn1}
\desc{Raise exception exn1 which does not have any arguments}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Break}
\begin{verbatim}
*/
        | TK_BREAK {
	    $$ = MakeBreakStmt(yy_st ); }
/*--
\end{verbatim}
\desc{Exit the nearest enclosing do, while or for loop.}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Binding}
\begin{verbatim}
*/
        | Binding {
	    $$ = $1; }
/*--
\end{verbatim}
\syntaxc{lv1 = e1}
\desc{Assign e1 to lv1}
\examplec{spong = 2 + 3}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Invocation}
\begin{verbatim}
*/
	| Invocation {
	    $$ = $1; }
/*--
\end{verbatim}
\syntaxc{[i01,i02..] = i10 \$ i11 [ e1,e2..]}
\syntaxc{[i01,i02..] = i10 \$ i11}
\syntaxc{i10 \$ i11 [ e1,e2..]}
\syntaxc{i10 \$ i11}
\desc{See the invocation section below}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Simple spawned invocation with name}
\begin{verbatim}
*/
	| TK_SPAWN '(' Invocation ',' TK_STRING ')' {
	    $$ = MakeSpawnInvocation(yy_st, $5, NULL_Expr, $3); }
/*--
\end{verbatim}
\syntaxc{spawn name [i01,i02..] = i10 \$ i11 [ e1,e2..]}
\syntaxc{spawn name [i01,i02..] = i10 \$ i11}
\syntaxc{spawn name i10 \$ i11 [ e1,e2..]}
\syntaxc{spawn name i10 \$ i11}
\desc{See the invocation section below}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Spawned invocation with name and environment}
\begin{verbatim}
*/
	| TK_SPAWN '(' Invocation ',' TK_STRING ',' TK_IDENTIFIER ')' {
	    $$ = MakeSpawnInvocation(yy_st, $5, $7, $3); }
/*--
\end{verbatim}
\syntaxc{spawn name env [i01,i02..] = i10 \$ i11 [ e1,e2..]}
\syntaxc{spawn name env [i01,i02..] = i10 \$ i11}
\syntaxc{spawn name env i10 \$ i11 [ e1,e2..]}
\syntaxc{spawn name env i10 \$ i11}
\desc{See the invocation section below}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\chapter{Additional statements}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Remove}
\begin{verbatim}
*/
        | TK_RM TK_IDENTIFIER {
	    $$ = MakeRMStmt(yy_st, $2); }
/*--
\end{verbatim}
\syntaxc{rm i1}
\desc{Invoke Context\$Remove on i1}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Kill}
\begin{verbatim}
*/
        | TK_KILL Expr {
	    $$ = MakeKillStmt(yy_st, $2); }
/*--
\end{verbatim}
\syntaxc{kill e1}
\desc{Destroys a domain by calling DomainMgr\$Destroy. 
}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{cd with argument}
\begin{verbatim}
*/
        | TK_CD TK_IDENTIFIER {
	    $$ = MakeCDStmt(yy_st, $2); }
/*--
\end{verbatim}
\syntaxc{cd i1}
\desc{Change the ``current'' context to a context i1. If i1 begins with a $>$ it is taken relative to the root context of
this clanger. Otherwise, it is taken relative to the ``current''
context.
}
\examplec{cd conf}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{cd without argument}
\begin{verbatim}
*/
        | TK_CD {
	    $$ = MakeCDStmt(yy_st, NULL_Expr); }
/*--
\end{verbatim}
\syntaxc{cd}
\desc{Change the ``current'' context to point to the root context of
this clanger.}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Halt}
\begin{verbatim}
*/
        | TK_HALT {
	    $$ = MakeHaltStmt(yy_st); }
/*--
\end{verbatim}
\syntaxc{halt}
\desc{Invokes the system call \texttt{ntsc\_dbgstop}.
}


 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{pwd}
\begin{verbatim}
*/
        | TK_PWD {
	    $$ = MakePWDStmt(yy_st); }
/*--
\end{verbatim}
\syntaxc{pwd}
\desc{Print the `'current'' context.}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{threadfork}
\begin{verbatim}
*/
        | TK_FORK TK_IDENTIFIER {
	    $$ = MakeForkStmt(yy_st, $2, 0); }
/*--
\end{verbatim}
\syntaxc{threadfork}
\desc{Invoke Threads$Fork on the first argument of threadfork (which must be
a Closure).}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{threadfork with stacksize}
\begin{verbatim}
*/
        | TK_FORK TK_IDENTIFIER Expr {
	    $$ = MakeForkStmt(yy_st, $2, $3); }
/*--
\end{verbatim}
\syntaxc{threadfork}
\desc{Invoke Threads$Fork on the first argument of threadfork (which must be
a Closure). The second argument specifies the stack size.}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Execute}
\begin{verbatim}
*/
        | TK_EXEC TK_IDENTIFIER TK_IDENTIFIER {
	    $$ = MakeExecStmt(yy_st, $2, $3); }
/*--
\end{verbatim}
\desc{Invoke Exec\$Run on the first argument of exec. Use the second
arugment as the private root of the new domain.}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Execute with context}
\examplec{run progs$>$eth0$>$closure progs$>$eth0}
\begin{verbatim}
*/
	| TK_EXEC TK_IDENTIFIER {
	    $$ = MakeExecStmt(yy_st, $2, NULL_Expr); }
/*--
\end{verbatim}
\syntaxc{run i1}
\desc{Invoke Exec\$Run on i1. }
\examplec{run modules$>$Carnage}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{List with argument}
\begin{verbatim}
*/
        | TK_LS TK_IDENTIFIER {
	    $$ = MakeListStmt(yy_st, $2); }
/*--
\end{verbatim}
\syntaxc{ls i1}
\desc{
Invoke Context\$List on i1, and attempt to find the type of
everything. Then print that information.
}
\examplec{ls $>$progs}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{List without argument}
\begin{verbatim}
*/
        | TK_LS {
	    $$ = MakeListStmt(yy_st, NULL_Expr); }
/*--
\end{verbatim}
\syntaxc{ls}
Invoke Context\$List on the ``current'' context fallacy, as for
Context\$List.


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Pause}
\begin{verbatim}
*/
        | TK_PAUSE  Expr {
	    $$ = MakePauseStmt(yy_st, $2); }
/*--
\end{verbatim}

\syntaxc{pause e1}
\desc{
Evaluate the argument, cast it (literally, as in a C bit cast for the
time being) to a time in nanoseconds and then wait for that length of
time. 
}

Here might be good time to confess the lexical sugar for time. 

\begin{itemize}
\item A number postfixed with \texttt{ns} is the same as a number.
\item A number postfixed with \texttt{us} is the same as that number
multiplied by 1000.
\item A number postfixed with \texttt{ms} is the same as that number
multiplied by 1000000.
\end{itemize}

\examplec{pause 1000ms}

\begin{verbatim}
*/
        | TK_MORE Expr {
	    $$ = MakeMoreStmt(yy_st, $2); }
/*--
\end{verbatim}
\syntaxc{more i1}
\desc{
Read a file or reader i1 and print it. If the argument evaluates to an
IDC offer, import it first.
}
\examplec{more dev$>$serial0$>$rd}



%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Exit}
\begin{verbatim}
*/
        | TK_EXIT {
	    TerminateShell(); }
/*--
\end{verbatim}
\syntaxc{exit}
\desc{I dread to think what this would do.}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Include}
\begin{verbatim}
*/
        | TK_INCLUDE Expr {
	    DB(printf("Got include"));
            $$ = MakeIncludeStmt(yy_st, $2);
	}

/*--
\end{verbatim}
\syntaxc{include i1}
\desc{
Include a reader or file. That is, read a line from it, execute it as
clanger and repeat until the reader is exhausted.
}
\examplec{include dev$>$serial0$>$rd}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\chapter{Statement glue}
\begin{verbatim}
*/

        ;
StatementPossiblyNull 
: Statement { $$ = $1 }
	| /* empty */ { 
	    $$ = MakeNullStmt(yy_st); }
	;

StatementList 
	: StatementPossiblyNull {
		$$ = $1; DB(printf("Got a statement concluding a statementlist\n")); }
        | StatementPossiblyNull ';' StatementList {
	    Expr_t *tail1 = $1->prev;
	    Expr_t *tail2 = $3->prev;

	    tail1->next = $3;
	    tail2->next = $1;
	    $3->prev = tail1;
	    $1->prev = tail2;

	    DB(printf("Got a statement as apart of a statementlist\n"));
	    $$ = $1;}

	;
/*--
\end{verbatim}
\desc{In certain places, null statements are allowed. The grammar
symbol StatementPossiblyNull may expand to a null statement.

In particular, null statements are not allowed in the following
locations:
\begin{itemize}
\item On either leaf of an IF statement.
\item In the handler of a TRY-FINALLY statement.
\item Inside a DO loop.
\end{itemize}

Note that compound statements are allowed in these places. Within
compound statements, null statements are allowed. So, \{\} is a valid
statement.
}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\chapter{Invocations}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Invocation with parameters and assignment}
\begin{verbatim}
*/
Invocation
	: BLValueList '=' TK_IDENTIFIER '$' TK_IDENTIFIER BExprList {
	  	$$ = MakeInvocStmt(yy_st, $1, $3, $5, $6 ); DB(printf("Got class A invocation\n"));}
/*--
\end{verbatim}
\syntaxc{[i01,i02..] = i10 \$ i11 [ e1,e2..]}
\desc{Evaluate e1, e2, .... Invoke operation i11 on closure i10, with
the resulting list of values as parameters. There must be exactly
the right number of parameters in that list. 
There must be exactly enough results to
fill the list i01, i02,...}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Invocation with assignment without parameters}
\begin{verbatim}
*/
	| BLValueList '=' TK_IDENTIFIER '$' TK_IDENTIFIER {
	  	$$ = MakeInvocStmt(yy_st, $1, $3, $5, NULL_Expr );  DB(printf("Got class B invocation\n"));}
/*--
\end{verbatim}
\syntaxc{[i01,i02..] = i10 \$ i11}
\desc{Invoke operation i11 on closure i10. There must be no
parameters to this operation. There must be exactly enough results to
fill the list i01, i02,...}
 
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Invocation with assignments without parameters}
\begin{verbatim}
*/
	| TK_IDENTIFIER '$' TK_IDENTIFIER BExprList {
	  	$$ = MakeInvocStmt(yy_st, NULL_Expr, $1, $3, $4 ); DB(printf("Got class C invocation\n"));}
/*--
\end{verbatim}
\syntaxc{i10 \$ i11 [ e1,e2..]}

\desc{Invoke operation i11 on closure i10 with
the resulting list of values as parameters. There must be no results
from this operation.
}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Invocation without assignment without parameters}
\begin{verbatim}
*/	| TK_IDENTIFIER '$' TK_IDENTIFIER {
	  	$$ = MakeInvocStmt(yy_st, NULL_Expr, $1, $3, NULL_Expr ); DB(printf("Got class D invocation\n")); }
	;
/*--
\end{verbatim}
\syntaxc{i10 \$ i11}
\desc{Invoke operation i11 on closure i10. There must be no
parameters to this operation. There must be no results from this
operation.}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Invocation in a new domain, within an expression}
\begin{verbatim}
*/
SpawnValue
	: TK_SPAWN '(' InvocValue ',' TK_STRING ',' TK_IDENTIFIER ')' {
		$$ = MakeSpawnInvocation(yy_st, $5, $7, $3); }
	| TK_SPAWN '(' InvocValue ',' TK_STRING ')' {
	  	$$ = MakeSpawnInvocation(yy_st, $5, NULL_Expr, $3); }
	;

/*--
\end{verbatim}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Qualified invocation within an expression with parameters}
\begin{verbatim}
*/
InvocValue
	: TK_IDENTIFIER '$' TK_IDENTIFIER BExprList '.' TK_IDENTIFIER {
	  	$$ = MakeInvocVal(yy_st, $1, $3, $4, $6 ); }
/*--
\end{verbatim}
\syntaxc{i10 \$ i11 [e1, e2..] . i20}

\desc{Evaluate e1,e2... Invoke operation i11 on closure i10 with the resulting list of values. Evaluates to the result
named i20, which must be named in the results list.}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

\nodec{Unqualified invocation within an expression without parameters}
\begin{verbatim}
*/	| TK_IDENTIFIER '$' TK_IDENTIFIER BExprList {
	  	$$ = MakeInvocVal(yy_st, $1, $3, $4, NULL_Expr ); }
	;

/*--
\end{verbatim}
\syntaxc{i10 \$ i11 [ e1, e2..]}
\desc{Evaluate e1,e2... Invoke operation i11 on closure i10 with the
resulting list of values. Evaluates to the single result of the operation.}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\chapter{Expressions}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Brackets}
\begin{verbatim}
*/
Expr
	: '(' Expr ')' {
		$$ = $2; }
/*--
\end{verbatim}
\syntaxc{ ( e1 ) }
\desc{Evaluates and returns e1. }
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Primaries}
\begin{verbatim}
*/	| Primary
/*--
\end{verbatim}
\desc{See below. Identifiers, integer or floating point numbers,
strings and the values true and false are valid expressions.}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Invocations in a new domain}
\begin{verbatim}
*/	| SpawnValue
/*--
\end{verbatim}
\syntaxc{i10 \$ i11 [e1, e2..] . i20}
\syntaxc{i10 \$ i11 [ e1, e2..]}
\desc{See invocations above.}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Invocations}
\begin{verbatim}
*/	| InvocValue
/*--
\end{verbatim}
\syntaxc{i10 \$ i11 [e1, e2..] . i20}
\syntaxc{i10 \$ i11 [ e1, e2..]}
\desc{See invocations above.}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Constructors}
\begin{verbatim}
*/	| Constructor
/*--
\end{verbatim}
\syntaxc{ <* e1, e2.. *> }
\syntaxc{ <| i1=e1, i2=e2.. |> }
\desc{See below. Possibly empty lists and binding lists are valid expressions.}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Boolean or}
\begin{verbatim}
*/	| Expr TK_OR Expr {
		$$ = MakeBoolean(yy_st, TK_OR, $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{e1 or e2}
\desc{e1 and e2 must evaluate to booleans. If either or both is true,
the expression evaluates to true otherwise it evaluates to false}.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Boolean and}
\begin{verbatim}
*/	| Expr TK_AND Expr {
		$$ = MakeBoolean(yy_st, TK_AND, $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{e1 and e2}
\desc{e1 and e2 must evaluate to booleans. If both are true, the
expression evaluates to true otherwise it evaluates to false}.
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Bitwise or}
\begin{verbatim}
*/	| Expr '|' Expr {
		$$ = MakeBinary(yy_st, '|', $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{e1 | e2}
\desc{The bitwise or of e1 and e2 is returned. }
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Bitwise exclusive or}
\begin{verbatim}
*/	| Expr '^' Expr {
		$$ = MakeBinary(yy_st, '^', $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{e1 \^\ e2}
\desc{The bitwise exclusive or of e1 and e2 is returned. }
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Bitwise and}
\begin{verbatim}
*/	| Expr '&' Expr {
		$$ = MakeBinary(yy_st, '&', $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{e1 \& e2}
\desc{The bitwise and of e1 and e2 is returned}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Equality test}
\begin{verbatim}
*/	| Expr TK_EQ Expr {
		$$ = MakeBinary(yy_st, TK_EQ, $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{ e1 == e2}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Inequality test}
\begin{verbatim}
*/	| Expr TK_NE Expr {
		$$ = MakeBinary(yy_st, TK_NE, $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{ e1 != e2}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Less that test}
\begin{verbatim}
*/	| Expr '<' Expr {
		$$ = MakeBinary(yy_st, '<', $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{ e1 < e2 }
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Greater than test}
\begin{verbatim}
*/	| Expr TK_GT Expr {
		$$ = MakeBinary(yy_st, TK_GT, $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{ e1 >> e2 }
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Less than or equals test}
\begin{verbatim}
*/	| Expr TK_LE Expr {
		$$ = MakeBinary(yy_st, TK_LE, $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{ e1 <= e2}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Greater than or equals test}
\begin{verbatim}
*/	| Expr TK_GE Expr {
		$$ = MakeBinary(yy_st, TK_GE, $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{ e1 >= e2}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{XXX}
\begin{verbatim}
*/	| Expr TK_LEFT Expr {
		$$ = MakeBinary(yy_st, TK_LEFT, $1, $3 ); }
/*--
\end{verbatim}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{XXX}
\begin{verbatim}
*/	| Expr TK_RIGHT Expr {
		$$ = MakeBinary(yy_st, TK_RIGHT, $1, $3 ); }
/*--
\end{verbatim}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Binary subtratction}
\begin{verbatim}
*/	| Expr '-' Expr {
		$$ = MakeBinary(yy_st, '-', $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{ e1 - e2}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Artihmetic addition}
\begin{verbatim}
*/	| Expr '+' Expr {
		$$ = MakeBinary(yy_st, '+', $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{e1 + e2}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Arithmetic mutliplication}
\begin{verbatim}
*/	| Expr '*' Expr {
		$$ = MakeBinary(yy_st, '*', $1, $3 ); }
/*--
\end{verbatim}
\syntaxc{ e1 * e2}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Arithmetic division}
\begin{verbatim}
*/	| Expr '/' Expr {
		$$ = MakeBinary(yy_st, '/', $1, $3 ); }
/*--
\end{verbatim}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Arithmetic modulus}
\begin{verbatim}
*/	| Expr '%' Expr {
		$$ = MakeBinary(yy_st, '%', $1, $3 ); }
/*--
\end{verbatim}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Unary negation}
\begin{verbatim}
*/	| '-' Expr %prec TKF_UMINUS
		{ $$ = MakeUnary(yy_st, TKF_UMINUS, $2 ); }
/*--
\end{verbatim}
\syntaxc{-e1}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Unary affirmation}
\begin{verbatim}
*/	| '+' Expr %prec TKF_UPLUS
		{ $$ = MakeUnary(yy_st, TKF_UPLUS, $2 ); }
/*--
\end{verbatim}
\syntaxc{+e1}
\desc{What do you want to use this for? Answers on a postcard to:
\begin{center}
The SRG what to do with the unary plus operator in clanger competition

Cambridge University Computer Laboratory

New Museums Site

Cambridge

UK
\end{center}

Timothy Roscoe or his relatives are not allowed to enter this competition.

}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Reference-of}
\begin{verbatim}
*/	| '&' Expr %prec TKF_REF
		{ $$ = MakeUnary(yy_st, TKF_REF, $2 ); }
/*--
\end{verbatim}
\syntaxc{\&e1}
\desc{Produces a reference to expresssion e1.}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Dereference}
\begin{verbatim}
*/	| '*' Expr %prec TKF_DEREF
		{ $$ = MakeUnary(yy_st, TKF_DEREF, $2 ); }
/*--
\end{verbatim}
\syntaxc{*e1}
\desc{Dereference e1}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Logical negation}
\begin{verbatim}
*/	| '!' Expr 
		{ $$ = MakeUnary(yy_st, '!', $2 ); }
/*--
\end{verbatim}
\syntaxc{!e1}
\desc{Returns the negation of boolean argument e1. XXX check the
source does this.}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Bitwise negation}
\begin{verbatim}
*/	| '~' Expr 
		{ $$ = MakeUnary(yy_st, '~', $2 ); }
	;
/*--
\end{verbatim}
\syntaxc{~e1}
\desc{Returns the bitwise negation of e1}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Identifiers}
\begin{verbatim}
*/
Primary
	: TK_IDENTIFIER 
/*--
\end{verbatim}
\desc{An identifier is a valid expression. When evaluated, it must of
course be bound to something}.
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Integer Numbers}
\begin{verbatim}
*/	| TK_NUMBER
/*--
\end{verbatim}
\desc{Integers numbers are valid expressions. All integers are
promoted to the last signed integer available automatically.}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Strings}
\begin{verbatim}
*/	| TK_STRING
/*--
\end{verbatim}
\desc{Strings surronded by double quotes are valid expressions. 

When prefixed with $\backslash$ the following characters expand:

\begin{description}
\item[n] Newline      (ASCII 13)
\item[t] Horizontal tab          (ASCII 9)
\item[v] Vertical tab (ASCII 11)
\item[b] Backspace (ASCII 8)
\item[r] Carriage return (ASCII 10)
\item[f] Form feed (ASCII 12)
\item[a] Alarm (bell) (ASCII 7)
\item[$\backslash$] Literal backslash 
\item[?] Delete (ASCII 127)
\item['] Single quote 
\item["] Double quote
\end{description}

(Use the source: look at esc\_table in Lexer.c).
}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Floating point numbers}
\begin{verbatim}
*/	| TK_FLOAT
/*--
\end{verbatim}
\desc{Floating point numbers are valid expressions. They are stored
and typed as doubles. XXX check the source does this.}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{The boolean True}
\begin{verbatim}
*/	| TK_TRUE
/*--
\end{verbatim}
\syntaxc{true}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{The boolean False}
\begin{verbatim}
*/	| TK_FALSE
	;
/*--
\end{verbatim}
\syntaxc{false}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Empty binding list constructor}
\begin{verbatim}
*/
Constructor
	: TK_CONSL TK_CONSR {
		$$ = MakeContextConstructor(yy_st, NULL_Expr); }
/*--
\end{verbatim}
\syntaxc{ <| |> }
\desc{Evaluates to an empty context}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Binding list constructor}
\begin{verbatim}
*/	| TK_CONSL BindingList TK_CONSR {
		$$ = MakeContextConstructor(yy_st, $2); }
/*--
\end{verbatim}
\syntaxc{ <| i1=e1, i2=e2, i3=e3.. |> }
\desc{Evaluates to a context containing identifiers i1,
i2,.. associated with expressions e1, e2..}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Empty list constructor}
\begin{verbatim}
*/        | TK_LISTL TK_LISTR {
	    $$ = MakeListConstructor(yy_st, NULL_Expr); }
/*--
\end{verbatim}
\syntaxc{ <* *> }
\desc{Evaluates to a reference to a sequences of refernces to anys
that is empty}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{List constructor}
\begin{verbatim}
*/        | TK_LISTL ExprList TK_LISTR {
	    $$ = MakeListConstructor(yy_st, $2); }

	;
/*--
\end{verbatim}
\syntaxc{ <* e1, e2,.. *> }
\desc{Evaluates to a reference to a sequences of references to anys
containing the results of evaluating e1, e2.... Really.}

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\chapter{Glue}
\begin{verbatim}
*/
LValue
	: TK_IDENTIFIER
	;

/* --
\end{verbatim}
\syntaxc{i1}
\desc{An identifier is an lvalue.}


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Binding list}
\begin{verbatim}
*/
BindingList
	: ConsBinding {
		$$ = $1; }
        | BindingList ',' ConsBinding {
		LINK_ADD_TO_TAIL( $1, $3 );
		$$ = $1; }
/*--
\end{verbatim}
\desc{A binding list consists of one or more ConsBinding symbols}.
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Conventional constructor binding}
\begin{verbatim}
*/
ConsBinding
	: Binding
/*--
\end{verbatim}
\desc{A Binding is a valid constructor binding.}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Lvalue constructor binding}
\begin{verbatim}
*/	| LValue { /* allow flags in Constructors, but not assignments */
		$$ = MakeBinding(yy_st, $1, NULL_Expr); }
/*--
\end{verbatim}
\desc{An lvalue is a valid constructor binding.}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Bindings}
\begin{verbatim}
*/
Binding
	: LValue '=' Expr {
		$$ = MakeBinding(yy_st, $1, $3 ); }
	;

/*--
\end{verbatim}
\desc{An lvalue associated with an expression is a valid binding}
\nodec{List of lvalues for binding an invocation}
\begin{verbatim}

*/

BLValueList
	: '['  ']' {
		$$ = NULL_Expr; }
	| '[' LValueList ']' {
		$$ = $2; }
	;
/*--
\end{verbatim}
\desc{An Binding lvalue list as used in invocations may be empty or
may contain a list of lvalues}.
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{LValue lists}

\begin{verbatim}
*/

LValueList
	: LValue {
		$$ = $1; }
	| LValueList ',' LValue {
	  	LINK_ADD_TO_TAIL( $1, $3 );
		$$ = $1; }
	;
/*--
\end{verbatim}
\desc{LValue lists may contain one or more lvalues}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{Binding expression lists}
\begin{verbatim}
*/
BExprList
	: '['  ']' {
		$$ = NULL_Expr; }
	| '[' ExprList ']' {
		$$ = $2; DB(printf("Got BEXprList\n")); }
	;
/*--
\end{verbatim}
\desc{Binding expression lists as used in invocations are (possibly
empty) lists of expressions}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\nodec{}
\begin{verbatim}
*/
ExprList
	: Expr {
		$$ = $1; }
	| ExprList ',' Expr {
		LINK_ADD_TO_TAIL( $1, $3 );
		$$ = $1; }
	;
/*--
\end{verbatim}
\desc{An expression list is a comma separated list of one or more
expressions.}
\end{document}
*/
%%

#include <stdio.h>

int yyerror(s)
char *s;
{
#if 1
  printf("yyerror: %s\n", s);
#endif
  return 0;
}

