/*
 ****************************************************************************
 * (C) 1998                                                                 *
 *                                                                          *
 * University of Cambridge Computer Laboratory /                            *
 * University of Glasgow Department of Computing Science                    *
 ****************************************************************************
 *
 *        File: mkfs.c
 *      Author: Matt Holgate
 *     Changes: 
 *              
 *        Date: 1998
 * 
 * Environment: UNIX tool
 * Description: SimpleFS/NemFS file system creator
 *
 ****************************************************************************
 * $Id: mkfs.c 1.2 Thu, 15 Apr 1999 16:44:25 +0100 dr10009 $
 ****************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

/* Linux sensibly defines these types... */
#if defined(__linux__)
#define uint32_t u_int32_t
#define uint64_t u_int64_t
#elif defined(__osf__) && defined(__alpha__)
#define uint32_t unsigned int
#define uint64_t unsigned long int
#endif

#include "nemfs.h"
#include "mkfs.h"

/* debugging macro */
#ifdef DEBUG
#define TRC(x) x
#else
#define TRC(x)
#endif

/* defaults in absence of command line options */
#define BLKSIZE     1024
#define NBLKS       1024
#define NINODES     1024
#define MAXFREELIST 1024
#define MAXEXT      2

/* global variables (external) */
uint32_t   blksize      = BLKSIZE;
block_t    nblks        = NBLKS;
uint32_t   ninodes      = NINODES;
uint32_t   maxfreelist  = MAXFREELIST;
uint32_t   maxext       = MAXEXT;
char       *device      = NULL;
char       **inputfiles = NULL;
char       *progname    = NULL;
int        verbose      = 0;

/* global variables (this file only) */
static char       *buf = NULL;        /* buffer to write to */
static FILE       *file = NULL;
static block_t    wblk;

static void writebuf(void);
static void mkfs(void);

int main(int argc, char *argv[]) {

    /* get our command line args */
    progname = argv[0];
    if (!parseargs(argc, argv)) exit(EXIT_FAILURE);

    /* make the file system */
    mkfs();

    return EXIT_SUCCESS;

}

static void mkfs()
{

    /* dynamically define our inode type depending on maximum no. of
       extents value */
    INODE_STRUCT_DEF(maxext);

    superblock_t  sblk;
    inode_t       *inodearray = NULL;     /* the inode table */
    extent_t      *fslistarray = NULL;    /* the free space list */

    uint32_t      i,j,k;  /* general loop vars etc. */
    uint32_t      n;
    char          c;
    block_t       blk;
    uint64_t      size;
    block_t       sizeblks;
    FILE          *infile;
    uint32_t      dirsize;
    char          *cp;
    char          *dir[256];
    uint32_t      uid = 0;
    char          *dirdata;
    char          *dd;
    uint32_t      dirinode;

    /* XXX we need to do some basic sanity checks on size of input
       params eg. superblock > blksize ?? */

    /* check to see if file already exists */
    if ((file = fopen(device, "r"))) {
	fclose(file);
	fprintf(stderr, "%s: file/device '%s' already exists. Overwrite? ", 
		progname, device);
	fflush(stderr);
	if ((c = getchar()) != 'y' && c != 'Y') {
	    fprintf(stderr, "Operation cancelled.\n");
	    exit(EXIT_FAILURE);
	}
    }

    wblk = 0;

    TRC(printf("Size of superblock is %d\n", (int) sizeof(superblock_t)));
    TRC(printf("Size of extent is %d\n", (int) sizeof(extent_t)));
    TRC(printf("Size of extent list is %d\n", (int) sizeof(extentlist_t)));
    TRC(printf("Size of inode is %d\n", (int) sizeof(inode_t)));

    
    /* create the superblock - Block 1 */
    sblk.blksize        = blksize;
    sblk.nblks          = nblks;

    sblk.ninodes        = ninodes;
    sblk.inodesize      = 1 + (sizeof(inode_t) - 1) / blksize;
    sblk.inodetbl_base  = 2; /* usually starts at 2 */
    sblk.inodetbl_len   = ninodes * sblk.inodesize;

    sblk.maxfreelist    = maxfreelist;
    sblk.fslist_base    = sblk.inodetbl_base + sblk.inodetbl_len;
    sblk.fslist_len     = 1 + (maxfreelist*sizeof(extent_t)-1) / blksize;

    sblk.maxext         = maxext;

    sblk.datastart      = sblk.fslist_base + sblk.fslist_len;
    sblk.rootdir        = 0;          /* XXX unused at present */

    if (sblk.datastart >= nblks) {
	fprintf(stderr, "%s: File system too small for specified "
		"parameters.\n", progname);
	exit(EXIT_FAILURE);
    }

    /* print out superblock info */
    TRC(printf("Blocksize          = %d\n", sblk.blksize));
    TRC(printf("No of blocks       = %d\n", sblk.nblks));
    TRC(printf("No of inodes       = %d\n", sblk.ninodes));
    TRC(printf("Inode size (blks)  = %d\n", sblk.inodesize));
    TRC(printf("Inode table base   = %d\n", sblk.inodetbl_base));
    TRC(printf("Inode tbl length   = %d\n", sblk.inodetbl_len));
    TRC(printf("Max free list      = %d\n", sblk.maxfreelist));
    TRC(printf("Free list base     = %d\n", sblk.fslist_base));
    TRC(printf("Free list len      = %d\n", sblk.fslist_len));
    TRC(printf("Max extents/file   = %d\n", sblk.maxext));
    TRC(printf("Data start block   = %d\n", sblk.datastart));
    TRC(printf("Root dir inode     = %d\n", sblk.rootdir));

    /* allocate buffer space for writes. Make it the number of 
       blocks for an inode so its easier when we have to write those out */
    if ((buf = malloc(blksize * sblk.inodesize)) == NULL) {
	fprintf(stderr, "%s: Couldn't allocate buffer space.\n",progname);
	exit(EXIT_FAILURE);
    }

    /* open the device/output file */
    file = fopen(device, "w+b");  /* XXX should we use raw open for devices? */
    if (!file) {
	fprintf(stderr, "%s: Couldn't open device/file.\n", progname);
	exit(EXIT_FAILURE);
    }

    /* write a null boot block */
    memset(buf, 0, blksize);
    writebuf();

    /* write the superblock */
    memcpy(buf, &sblk, sizeof(sblk));
    writebuf();

    /* create the inode table */
    if ((inodearray = (inode_t *) malloc(sblk.inodetbl_len * blksize)) == NULL) {
	fprintf(stderr, "%s: Couldn't allocate memory.\n", progname);
	exit(EXIT_FAILURE);
    }
    memset(inodearray, 0, sblk.inodetbl_len * blksize);

    /* create the inode free pointers (first inode unusable) */
    for (i=0; i<ninodes; i++) {
	inodearray[i].nextfree = i+1;
    }
    inodearray[ninodes-1].nextfree = 0; /* last free inode points to 0 */

    /* start to process input files. We just use each inode in order, 'cos
       they're all free. (not bothering with links here...) */
    j = 1;                      /* start at inode 1 */
    blk = sblk.datastart;       /* start allocating data blocks here */
    for (i=0; inputfiles[i] != NULL; i++) {
	if ((infile = fopen(inputfiles[i], "r")) == NULL) {
	    fprintf(stderr, "%s: Warning: couldn't open file %s\n", 
		    progname, inputfiles[i]);
	}
	else if (j >= ninodes) {
		fprintf(stderr, "%s: Warning: couldn't add file %s - insufficient inodes\n",
			progname, inputfiles[i]);
		continue;
	}
	else {

	    /* XXX is there a better way of doing this without using stat? */
	    fseek(infile, 0, SEEK_END);
	    size = ftell(infile);
	    fclose(infile);
	    sizeblks = (size > 0) ? (1 + (size - 1) / blksize) : 0;

	    if (blk + sizeblks > nblks) {
		fprintf(stderr, "%s: Warning: couldn't add file %s - insufficient blocks\n",
			progname, inputfiles[i]);
		continue;
	    }

	    TRC(printf("Adding file %s\n", inputfiles[i]));

	    inodearray[j].size = size;
	    inodearray[j].nextfree = INODE_IN_USE;  /* special no. for not free */

	    /* create one extent for this file */
	    inodearray[j].extlist.ext[0].base = (size > 0) ? blk : 0;
	    inodearray[j].extlist.ext[0].len  = sizeblks;
	    blk = blk + sizeblks;
	    for (k=1; k<maxext; k++) {
		inodearray[j].extlist.ext[k].base = 0;
		inodearray[j].extlist.ext[k].len = 0;
	    }

	    /* move this name down in the array */
	    inputfiles[j-1] = inputfiles[i];
	    j++;
	}
    }
    inputfiles[j-1] = NULL;

    /* reset free inode pointer in inode 0 */
    if (j < ninodes) inodearray[0].nextfree = j;
    else inodearray[0].nextfree = 0;

TRC(
    printf("\nInode: %4d; Reserved\n",0);
    for (i=0; inputfiles[i] != NULL; i++) {
	printf("Inode: %4d; ",i+1);
	printf("File: %20s; size: %8ld; nextfree: %d\t",
	       inputfiles[i], (long int) inodearray[i+1].size, inodearray[i+1].nextfree);
	printf("Base: %5d\tLen: %5d\n", 
	       inodearray[i+1].extlist.ext[0].base, 
	       inodearray[i+1].extlist.ext[0].len);
    }
    printf("Next free inode is %d\n", inodearray[0].nextfree);
    )

  /* create directory */
  TRC(printf("Creating root directory...\n"));
  uid = 0;
  dirsize = 6; /* magic + end marker */
  for (i=0; inputfiles[i] != NULL; i++) {

    /* strip off any path */
    cp = strrchr(inputfiles[i], '/');

    if (!cp) cp = inputfiles[i];
    else cp++;

    /* check to see if unique */
    for (j=0; j<i; j++)
      if (strcmp(cp, dir[j]) == 0) break;

    /* there was a collision */
    if (j<i) {

      dir[i] = malloc(strlen(cp) + 5); /* space for .xxx\0 */
      if (!dir[i]) {
	fprintf(stderr, "%s: couldn't allocate memory.\n", progname);
	fclose(file);
	exit(EXIT_FAILURE);
      }

      sprintf(dir[i], "%s.%03d", cp, uid);
      printf("%s\n",dir[i]);
      uid++;

    }
    else {
      dir[i] = malloc(strlen(cp) + 1); /* space for \0 */
      if (!dir[i]) {
	fprintf(stderr, "%s: couldn't allocate memory.\n", progname);
	fclose(file);
	exit(EXIT_FAILURE);
      }

      strcpy(dir[i], cp);
    }

    dirsize += (strlen(dir[i]) + 1 + sizeof(uint32_t));
    /* name + \0 + inode num */
  }

  /* entry for directory */
  dir[i] = malloc(2);
  if (!dir[i]) {
    fprintf(stderr, "%s: couldn't allocate memory.\n", progname);
    fclose(file);
    exit(EXIT_FAILURE);
  }
  sprintf(dir[i], ".");
  dirsize += (strlen(dir[i]) + 1 + sizeof(uint32_t));
  i++;

  dir[i] = NULL;

  TRC(printf("directory:\n");

      for (i=0; dir[i] != NULL; i++) {
	printf("name=%30s  inode=%d\n", dir[i], i+1);
      }
      )
    
  TRC(printf("size of dir = %d\n",dirsize));
    
  /* create the directory */
  sizeblks = 1 + ((dirsize - 1) / blksize);
  dirdata = malloc(blksize * sizeblks); /* XXX */
  if (!dirdata) {
    fprintf(stderr, "%s: couldn't allocate memory.\n", progname);
    fclose(file);
    exit(EXIT_FAILURE);
  }
  dd = dirdata;

  /* magic */
  sprintf(dd, "SFSD");
  dd += 5;

  for (i=0; dir[i] != NULL; i++) {
    strcpy(dd, dir[i]);
    dd += strlen(dir[i]) + 1;

    /* avoiding alignment/endian nastiness */
    *(dd++) = ((i+1) & 0x000000FF);
    *(dd++) = ((i+1) & 0x0000FF00) >> 8;
    *(dd++) = ((i+1) & 0x00FF0000) >> 16;
    *(dd++) = ((i+1) & 0xFF000000) >> 24;    

    free(dir[i]);

  }

  *(dd++) = 0;

  TRC(printf("size of directory %d (predicted %d)\n", (int) (dd-dirdata), dirsize));

  /* allocate an inode for the directory */
  dirinode = i;
  TRC(printf("inode for dir is %d\n", dirinode));
  if (dirinode >= ninodes) {
    fprintf(stderr, "%s: run out of inodes for root dir.\n", progname);
    fclose(file);
    exit(EXIT_FAILURE);
  }

  /* reset free inode pointer in inode 0 */
  if (dirinode + 1 < ninodes) inodearray[0].nextfree = dirinode + 1;
  else inodearray[0].nextfree = 0;



  /* check if dir will fit */
  if ((blk + sizeblks) > nblks) {
    fprintf(stderr, "%s: run out of space for directory.\n", progname);
    fclose(file);
    exit(EXIT_FAILURE);
  }

  /* setup directory's inode (groan) */
  inodearray[dirinode].extlist.ext[0].base = blk;
  inodearray[dirinode].extlist.ext[0].len  = sizeblks;
  for (i=1; i<maxext; i++) {
    inodearray[dirinode].extlist.ext[i].base = 0;
    inodearray[dirinode].extlist.ext[i].len  = 0;
  }
  inodearray[dirinode].size = dirsize;
  inodearray[dirinode].nextfree = INODE_IN_USE;

  blk += sizeblks;

  /* write inode table to disk */
  memset(buf, 0, blksize * sblk.inodesize);
  for (i=0; i<sblk.ninodes; i++) {
    memcpy(buf, &inodearray[i], sizeof(inode_t));
    if (fwrite(buf, blksize, sblk.inodesize, file) < sblk.inodesize) {
      fprintf(stderr, "%s: Couldn't write to file.\n", progname);
      fclose(file);
      exit(EXIT_FAILURE);
    }
    wblk += sblk.inodesize;
  }
  
  TRC(printf("expected end of inode tbl = %d, real = %d\n",
	     sblk.inodetbl_base + sblk.inodetbl_len, wblk));
  
  
  /* create the free space list */
  if ((fslistarray = malloc(sblk.fslist_len * blksize)) == NULL) {
    fprintf(stderr, "%s: Unable to allocate memory.\n", progname);
    fclose(file);
    exit(EXIT_FAILURE);
  }    
  memset(fslistarray, 0, sblk.fslist_len * blksize);
  for (i=0; i<maxfreelist; i++) {
    fslistarray[i].base = 0;
    fslistarray[i].len  = 0;
  }
  
  /* in *memory* the entry zero is always reserved as a pointer to the real
     first entry, so our first real entry is [1]. We just set this to the
     space remaining... */
  if (blk < nblks) {
    fslistarray[1].base = blk;
    fslistarray[1].len  = nblks - blk;
  }
  TRC(printf("Free space: base=%d, len=%d\n",
	     fslistarray[1].base, fslistarray[1].len));
  
  /* write free space list to disk */
  if (fwrite(fslistarray, blksize, sblk.fslist_len, file) < sblk.fslist_len) {
    fprintf(stderr, "%s: Couldn't write to file.\n", progname);
    fclose(file);
    exit(EXIT_FAILURE);
  }
  wblk += sblk.fslist_len;
  
  TRC(printf("expected end of free space tbl = %d, real = %d\n",
	     sblk.fslist_base + sblk.fslist_len, wblk));
  
  /* finally write the files, block by block */
  if (verbose) printf("\n");
  for (i=0; inputfiles[i] != NULL; i++) {
    
    if (inodearray[i+1].size == 0) {
      if (verbose) printf("Skipping %19s (empty file)\n", inputfiles[i]);
      continue;
    }

    infile = fopen(inputfiles[i], "r");
    if (!infile) {
      fprintf(stderr, "%s: Fatal error - unable to open file %s.\n",
	      progname, inputfiles[i]);
      fclose(file);
      exit(EXIT_FAILURE);
    }
    
    if (verbose) {
      printf("Writing %20s (%9ld)\tfrom block ", inputfiles[i], 
	     (long int) inodearray[i+1].size);
      printf("%d to ", wblk);
    }
    
    while (!feof(infile)) {
      
      /* note other way round of fread - deliberate - we need bytes */
      if ((n = fread(buf, 1, blksize, infile)) < blksize) {
	memset(buf + n, 0, blksize - n);
      }
      
      /* special case where file ends on block boundary */
      if (n == 0 && feof(infile)) break;
      
      if (ferror(infile)) {
	fprintf(stderr, "\n%s: Fatal error - error reading file "
		"%s.\n", progname, inputfiles[i]);
	fclose(infile);
	fclose(file);
	exit(EXIT_FAILURE);
      }
      
      writebuf();
    }
    fclose(infile);
    if (verbose) printf("%d.\n", wblk-1);
  }
  
  /* write out the root directory */
  if (verbose) {
    printf("Writing %20s (%9ld)\tfrom block ", "ROOT-DIR (.)", 
	   (long int) inodearray[i+1].size);
    printf("%d to ", wblk);
  }
  
  sizeblks = 1 + ((dirsize - 1) / blksize);
  for (i=0; i < sizeblks; i++) {
    memcpy(buf, dirdata + (blksize * i), blksize);
    writebuf();
  }
  
  if (verbose) printf("%d.\n", wblk-1);
  
  TRC(printf("Expected end of data: block %d, real end of data: block %d \n", 
	     blk, wblk));
  if (blk != wblk) {
    fprintf(stderr, "%s: Eeek! Inconsistency error (bug in mkfs)\n", progname);
    fclose(file);
    exit(EXIT_FAILURE);
  }

  /* now spout out zeroes  till the disk is of the required size */
  memset(buf, 0, blksize);
  while (wblk < nblks) {
    writebuf();
  }
  
  /* just need to go back and patch the superblock
     with the root dir inode number */
  sblk.rootdir = dirinode;
  fseek(file, blksize, SEEK_SET);
  memcpy(buf, &sblk, sizeof(sblk));
  writebuf();   

  /* we're done! */
  printf("\nHurrah! File system make finished.\n");
  
  fclose(file);

  free(buf);
  free(inodearray);
  free(fslistarray);
   
  exit(EXIT_SUCCESS);

   
}

static void writebuf(void) {

    if (fwrite(buf, blksize, 1, file) < 1) {
	printf("%s: Couldn't write to file.\n", progname);
	fclose(file);
	exit(EXIT_FAILURE);
    }

    wblk++;

}



