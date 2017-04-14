
#ifndef _SymbolTable_h_
#define _SymbolTable_h_

/*
 * the same old generic symbol table,,,
 */

/* 
 * Size of hash table
 */
#define TABLESIZE 43

/* 
 * An Entry in the hash table
 */
#define STNULL	(STNode *)0
typedef struct _STNode STNode;
struct _STNode {
	STNode *next;	/* next in bucket */
	char   *s;	/* symbol */
	caddr_t val;	/* value */
};

/* 
 * The table state itself
 */
#define NULL_SymbolTable ((SymbolTable_t)0)
struct _SymbolTable_t {
  STNode heads[ TABLESIZE ];
};

extern SymbolTable_t New_SymbolTable( void );

extern  int SymbolTable_Enter (
	SymbolTable_t	 self,
	char            *s,
	caddr_t		 val );

extern void SymbolTable_Lookup (
	SymbolTable_t	 self,
	char            *s,
	caddr_t		*val,
	bool_t          *found );

extern void SymbolTable_Remove (
	SymbolTable_t	 self,
	char            *s );

extern void SymbolTable_Delete (
	SymbolTable_t	 self );

#endif /* _SymbolTable_h_ */
