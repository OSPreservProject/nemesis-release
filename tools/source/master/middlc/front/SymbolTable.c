
/*
 * Implementation of SymbolTable type.
 */

#include "Middlc.h"
#include "SymbolTable.h"


static int SThash(char *name);
static int STlu(SymbolTable_t st, char *s, STNode **prevp, STNode **nodep);
static void STrm(STNode *prev, STNode *node);

SymbolTable_t New_SymbolTable( void )
{
  SymbolTable_t st = new(SymbolTable);
  int i;
  
  for (i=0; i < TABLESIZE; i++)
    {
      st->heads[i].next = STNULL;
    }
  
  return (st);
}

/*
 *	hash symbol name to yield a listhead index
 */
static int SThash(char *name)
{
  int hash;
  
  for (hash = 0; *name != '\0'; )
    {
      hash += *name++;
    }
  return (hash % TABLESIZE);
}

/*
 *	internal routine to lookup a particular symbol in a particular table
 */
static int STlu(SymbolTable_t st, char *s, STNode **prevp, STNode **nodep)
{
  STNode *prev, *node;
  
  prev = &st->heads[SThash(s)];
  for (node = prev->next; node != STNULL; prev = node, node = prev->next) 
    {
      if (strcmp(node->s, s) == 0)
	{
	  *prevp = prev;
	  *nodep = node;
	  return (1);
	}
    }
  *prevp = prev;
  return (0);
}

/*
 *	lookup symbol in table - if found, copy associated data into info and
 *	return (1), else return (0)
 */
void SymbolTable_Lookup(SymbolTable_t self, char * s, caddr_t *val,
		   bool_t *found)
{
  STNode *prev, *node;
  
  if (! STlu(self, s, &prev, &node)) 
    {
      *found = 0;
      return ;
    }
  *val = node->val;
  *found = 1;
  return ;
}

/*
 *	enter a symbol with info into table - if symbol already exists, simply
 *	replace info
 */
int SymbolTable_Enter(SymbolTable_t self, char *s, caddr_t val)
{
  STNode *prev, *node;
  
  if ( !s ) {
    fprintf(stderr, "SymbolTable: null string.\n");
    return -1;
  }

  if (! STlu(self, s, &prev, &node))
    {
      node = (STNode *) malloc(sizeof(STNode));
      node->s    = s;
      node->val  = val;
      node->next = STNULL;
      prev->next = node;
      return 0;
    }
  node->val = val;
  return -1; /* should this really be the same as for the null string? - dme */
}

/*
 *	internal routine to remove a symbol
 */
static void STrm(STNode *prev, STNode *node)
{
  prev->next = node->next;
  free((char *)node);
}

/*
 *	delete symbol from table
 */
void SymbolTable_Remove(SymbolTable_t self, char *s)
{
  STNode *prev, *node;
  
  if (STlu(self, s, &prev, &node))
    {
      STrm(prev, node);
    }
  return ;
}

/*
 *	remove an entire symbol table
 */
void SymbolTable_Delete(SymbolTable_t self)
{
  int i;
  STNode *prev, *node;
  
  for (i = 0; i < TABLESIZE; i++)
    {
      prev = &self->heads[i];
      while ((node = prev->next) != STNULL) 
	{
	  STrm(prev, node);
	}
    }
  free((char *)self);
  return ;
}
