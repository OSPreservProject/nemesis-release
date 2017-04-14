
#ifndef _FileStack_h_
#define _FileStack_h_

#define STACKSIZE	(30)
#define PBSTACKSIZE	(80)



#define NULL_FileStack ((FileStack_t)0)

struct FSEntry_t {
  FILE          *fd;
  char 		*name;
  unsigned int   lineno;
  void          *lex_data; /* Whatever lex needs saving for suspended files */
};

struct _FileStack_t {
  struct  FSEntry_t files[ STACKSIZE ];	/* Stack of file descriptors */
  int     curfile;			/* Stack pointer */
  int     numpaths;			/* How many dirs to search */
  char	**paths;			/* The search paths */
  int     numsuffs;			/* How many suffices to try */
  char	**suffices;			/* The suffices */
  char    pbs[ PBSTACKSIZE ];	/* Push back stack for Unget */
  int     pbsp;			/* Push back stack pointer */
};

extern FileStack_t fs;

extern void FileStack_Init( int pathsize, char **path,
			    int suffsize, char **suffs );

extern bool_t 	 FileStack_Push( char *name, bool_t dot_only );
extern void	 FileStack_Pop( void );
extern void	 FileStack_NewLine( void );
extern int 	 FileStack_Lineno( void );
extern char	*FileStack_Filename( void );

#endif /* _FileStack_h_ */
