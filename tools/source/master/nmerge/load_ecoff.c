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
**      Sandpiper boot image builder.  Loader for ECOFF 
** 
** ENVIRONMENT: 
**
**      Alpha/OSF1
** 
** ID : $Id: load_ecoff.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
** 
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <a.out.h>

#include "nmerge.h"


/*
 * Load an image written in ECOFF format
 */
struct neminfo *load_ecoff(char *filename)
{
  int     fd;
  struct aouthdr exec;		/* aux header		*/
  struct filehdr fhdr;		/* ecoff file header	*/
  struct neminfo *ni;		/* nmerge info   	*/

  char  *bptr, *cptr;
  long mem_req;
  int i;

  if ((fd = open (filename, 0)) == -1) {
    printf ("Cannot open %s\n", filename);
    exit (1);
  }

  /* Read the file header information */

  i = read (fd, &fhdr, sizeof (struct filehdr));
  if (i != sizeof (struct filehdr) || fhdr.f_magic != ALPHAMAGIC ){
    printf ("%s is not an executable file\n", filename);
    close (fd);
    exit (1);
  }
  /*
   * read the aouthdr and check for the right type of
   * file.
   */
  i = read (fd, &exec, sizeof (struct aouthdr));

  if( exec.magic != OMAGIC ){
    printf("%s is not an OMAGIC file, relink using -N\n", 
	   filename);
    close (fd);
    exit (1);
  }

  /*
   * Get memory to load image
   */
  mem_req = exec.tsize+exec.dsize+exec.bsize;
  printf("Image requires %#010x bytes\n", mem_req);
  bptr = (char *)malloc((int)(mem_req+PAGSIZ));
  if( !bptr ){
    printf("can't allocate memory\n");
    exit(1);
  }

  /*
   * Read the text
   */
  lseek (fd, (int)N_TXTOFF(fhdr,exec), 0);
  i = read (fd, bptr, (int)exec.tsize);
  printf ("Loading %s\n\ttext at %#010lx\tlen=%#010lx\n",
	  filename, exec.text_start, exec.tsize);

  /*
   * Read the data
   */
  cptr = bptr+exec.tsize;
  i = read (fd, cptr, (int)exec.dsize);
  printf ("\tdata at %#010lx\tlen=%#010lx\n",
	  exec.data_start, exec.dsize);
  close (fd);

  /*
   * Zero out bss
   */
  printf ("\tbss  at %#010lx\tlen=%#010lx\n", exec.bss_start, exec.bsize);
  cptr += exec.dsize;
  for (i = 0; i < exec.bsize; i++)
    *cptr++ = 0;
  
  printf("\tent  at %#010lx\n", exec.entry);

  /*
   * Fill in the program header info and pass back to caller
   */
  ni = (struct neminfo *)malloc(sizeof(struct neminfo));
  if( !ni ){
    printf("can't allocate memory\n");
    exit(1);
  }
  ni->tsize = exec.tsize;
  ni->dsize = exec.dsize;
  ni->bsize = exec.bsize;
  ni->text_start = exec.text_start;
  ni->data_start = exec.data_start;
  ni->bss_start = exec.bss_start;
  ni->entry = exec.entry;
  ni->image = (long)bptr;
  return ni;
}
