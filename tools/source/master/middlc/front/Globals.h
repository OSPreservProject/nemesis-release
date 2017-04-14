
#ifndef _Globals_h_
#define _Globals_h_

typedef enum { Out_ParseOnly, Out_Xref, Out_Python, Out_MakeDepend } Out_Mode;

#define MAX_DIRS 20
#define MAX_SUFFICES 20

struct _Globals {
  Interface_t 		intf;		/* Interface currently being parsed */
  Identifier_t  	name;           /* Name of interface being parsed */
  SymbolTable_t 	symbols;	/* Symbols in this interface so far */
  DeclList_t    	types;          /* Types seen */
  InterfaceList_t	all_intfs;	/* All interfaces seen */
  SymbolTable_t		needs;		/* All interfaces seen */
  SymbolTable_t 	extends;	/* Supertypes seen */
  int           	op_count;       /* Counts ops in this interface */

  int 		pathsize;	/* Search path */
  char	       *path[MAX_DIRS]; 

  int 		suffixsize;	/* File suffices */
  char         *suffices[MAX_SUFFICES]; 

  Out_Mode	mode;		/* Output mode */
  bool_t	verb;		/* Verbose mode */
  bool_t        errors;         /* True if errors have been encountered */
  char         *banner;         /* String identifying this program */

  Parser_Mode_t pmode;		/* Needs or Extends? */
  bool_t        local;		/* Are we in a local interface graph? */
  bool_t 	top;		/* Are we at the top level? */
};

extern struct _Globals globals;

#endif /* _Globals_h_ */
