
/*
 * Implementation of FileStack proto-module.
 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "Middlc.h"
#include "FileStack.h"

#define STRINGSIZE (1024)

/*#define TRACE_FILESTACK*/
#ifdef TRACE_FILESTACK
#define TRC(x) x
#else
#define TRC(x)
#endif

static struct _FileStack_t _fs;
const FileStack_t fs = &_fs;

void FileStack_Init( int pathsize, char **path, int suffsize, char **suffs )
{
  TRC(printf("FileStack: creating...\n"));
  
  fs->curfile     = -1;
  fs->files[0].fd = (FILE *)0;
  fs->numpaths    = pathsize;
  fs->paths       = path;
  fs->numsuffs    = suffsize;
  fs->suffices    = suffs;
  fs->pbsp        = 0;

  return;
}   

/* 
 *  The interface between FileStack and the lexer is as follows:
 */

extern void * Lexer_FileInit( FILE *fd );
extern void   Lexer_FileEnds( void *old_buffer );


/*
 * Try to find a new file, open it and push it on the stack.
 */
bool_t  FileStack_Push( char *name, bool_t dot_only )
{
  char *fullname;
  int i, j;
  FILE *fd;

  TRC(printf("FileStack: Push file '%s'.\n", name ));

  if ( ++(fs->curfile) == STACKSIZE ) {
    --fs->curfile;
    Error_Fatal( "file stack overflow: too many open files");
  }

  if (!(fullname = malloc( STRINGSIZE ) )) {
    --fs->curfile;
    Error_Fatal( "malloc failed in file stack");
  }

  for (i = 0; i < fs->numpaths; i++) {
    if (dot_only && i != 0)
      break;
    for (j = 0; j < fs->numsuffs; j++) {
      if (name[0] == '/')
	sprintf(fullname, "%s%s", name, fs->suffices[j]);
      else
	sprintf(fullname, "%s/%s%s", fs->paths[i], name, fs->suffices[j]);
      TRC(printf("FileStack: trying '%s'...\n", fullname ));

      if ((fd = fopen(fullname, "r")) != NULL) {
	fs->files[fs->curfile].fd = fd; 
	fs->files[fs->curfile].lineno = 1;
	fs->files[fs->curfile].name = fullname;
	
	TRC(printf("FileStack: %s worked\n", fs->files[fs->curfile].name));

	fs->files[fs->curfile].lex_data = Lexer_FileInit( fd );

	return False;
      }
    }
  }
  --fs->curfile;
  Error_String( "can't find interface '%s'", name );
  return True;
}

/*
 * Close the current file and back up the stack.
 */
void FileStack_Pop( void )
{
  TRC(printf("FileStack: Pop.\n"));
  if ( fs->curfile < 0 ) {
    Error_Bug( "Attempt to pop too many files");
  }
  
  fclose( fs->files[fs->curfile].fd );
  if ( --fs->curfile >= 0 )
    Lexer_FileEnds ( fs->files[fs->curfile].lex_data );

  return ;
}


/*
 * Tell the FileStack that there has been a newline seen in the Lexer
 */
void FileStack_NewLine( void )
{
  if ( fs->curfile < 0 ) {
    Error_Bug( "Attempt to bump line no. of no file.");
  }
  
  fs->files[fs->curfile].lineno++;
  return;
}


/*
 * Return the current line number in the file being parsed
 */
int FileStack_Lineno( void )
{
  if ( fs->curfile < 0 )
    return 0;
  
  return( fs->files[fs->curfile].lineno );
}


/*
 * Return the current filename,
 */
char *FileStack_Filename( void )
{
  if ( fs->curfile < 0 )
    return "Top level";

  return( fs->files[fs->curfile].name );
}


