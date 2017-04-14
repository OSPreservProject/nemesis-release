/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      GUNK Nemesis tools
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Sandpiper boot image builder.  Loader for gas struct exec files
** 
** ENVIRONMENT: 
**
**      Alpha/OSF1
** 
** ID : $Id: load_exec.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
** 
**
*/

#include <stdio.h>
#include <stdlib.h>

#include "nmerge.h"

#include "exec.h"

/*
 * Load an image written in struct exec format
 */
struct neminfo *load_exec(char *filename)
{
  int     fd;
  struct  exec ex;		/* exec header		*/
  struct  neminfo *ni;		/* program header	*/
  char    *bptr, *cptr;
  long    mem_req;
  int     rc;
  int     i;
  
  if ((fd = open (filename, 0)) == -1) {
    printf ("Cannot open %s\n", filename);
    exit (1);
  }
  
  /*
   * Read and check the aouthdr
   */
  rc = read (fd, &ex, sizeof (struct exec));
  if( ex.a_magic != OMAGIC ){
    printf("%s is not an OMAGIC file, relink using -N\n", 
	   filename);
    close (fd);
    exit (1);
  }
  
  /*
   * Is there enough memory to load ?
   */
  mem_req = ex.a_text + ex.a_data + ex.a_bss;
  printf("Image requires %#010x bytes\n", mem_req);
  bptr = (char *)malloc((int)(mem_req+PAGSIZ));
  if( !bptr ){
    printf("can't allocate memory\n");
    exit(1);
  }
  
  printf ("Loading %s\n\ttext at %#010lx\tlen=%#010lx\n",
	  filename, ex.a_tstart, ex.a_text);
  printf ("\tdata at %#010lx\tlen=%#010lx\n",
	  ex.a_dstart, ex.a_data);
  printf ("\tbss  at %#010lx\tlen=%#010lx\n", 
	  ex.a_dstart + ex.a_data, ex.a_bss);
  printf("\tent  at %#010lx\n", ex.a_entry);
  
  /* Read the file */
  lseek (fd, (int)N_TXTOFF(ex), 0);
  rc = read (fd, bptr, (int)ex.a_text + (int)ex.a_data);
  
  
  /* Zero BSS */
  cptr = bptr + ex.a_text + ex.a_data;
  for (i = 0; i < ex.a_bss; i++)
    *cptr++ = 0;
  
  /*
   * Fill in the program header info and pass back to caller
   */
  ni = (struct neminfo *)malloc(sizeof(struct neminfo));
  if( !ni ){
    printf("can't allocate memory\n");
    exit(1);
  }
  ni->tsize = ex.a_text;
  ni->dsize = ex.a_data;
  ni->bsize = ex.a_bss;
  ni->text_start = ex.a_tstart;
  ni->data_start = ex.a_dstart;
  ni->bss_start = (int)ex.a_dstart + (int)ex.a_data;
  ni->entry = ex.a_entry;
  ni->image = (long)bptr;
  return ni;
}
