/*
 *	ntsc/ix86/pagesetup.c
 *	---------------------
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * Code to set up the default page table
 *
 * $Id: pagesetup.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 */

/* When this code is called, paging isn't enabled. The first 1Mb of
   memory is available for the page directory and tables, and probably for
   any other random information we want to keep there too. */

/* The page directory is going to be put in as low memory as feasible,
   i.e. at 8k. We don't want it at 0 to avoid trampling on BIOS stuff
   that may still be hanging around. The page at 0x1000 is reserved
   for pervasives. */

/* The initial page directory is basically flat (up to 16Mb; NTSC can
   sort the rest out when it gets going), apart from page 0 which is
   protected to catch NULL pointer dereferences */

struct tabentry {
    int present : 1;
    int wp : 1;
    int access : 1;
    int res1 : 2;
    int accessed : 1;
    int dirty : 1;
    int res2 : 2;
    int info : 3;
    int address : 20;
};

#define PAGEDIR_SIZE 1024
#define PAGETAB_SIZE 1024

#define PAGEDIR ((struct tabentry *)0x2000L)
#define PAGETAB ((struct tabentry *)0x3000L)

void create_initial_pagetable(void)
{
    int i, j;
    struct tabentry entry;

    /* Clear page directory */
    for (i=0; i<PAGEDIR_SIZE; i++) {
	*((unsigned int *)&entry)=0;
	PAGEDIR[i]=entry;
    }

    /* To map the first 16Mb of memory we need four page tables; put
       them in the directory... */
    for (i=0; i<4; i++) {
	*((unsigned int *)&entry)=0;
	entry.present=1;
	entry.wp=1; /* Writable */
	entry.access=1; /* Supervisor access only */
	entry.accessed=0; /* Not yet accessed */
	entry.dirty=0; /* Not yet overwritten */
	entry.address=3+i; /* Page tables are at 0x3000 upwards */
	PAGEDIR[i]=entry;
    }

    /* ...and create the tables */
    for (i=0; i<4; i++) {
	for (j=0; j<PAGETAB_SIZE; j++) {
	    *((unsigned int *)&entry)=0;
	    entry.present=1;
	    entry.wp=1;
	    entry.access=1;
	    entry.accessed=0;
	    entry.dirty=0;
	    entry.address=i*PAGETAB_SIZE+j;
	    PAGETAB[i*PAGETAB_SIZE+j]=entry;
	}
    }
    /* Fix up the first page so that accesses generate GPF; helps
       trap NULL pointer dereferences */
    PAGETAB[0].present=0;
}
