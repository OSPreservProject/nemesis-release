/*
 ****************************************************************************
 * (C) 1998                                                                 *
 *                                                                          *
 * University of Cambridge Computer Laboratory /                            *
 * University of Glasgow Department of Computing Science                    *
 ****************************************************************************
 *
 *        File: mkfs.h
 *      Author: Matt Holgate
 *     Changes: 
 *              
 *        Date: 1998
 * 
 * Environment: UNIX tool
 * Description: Headers for mkfs.simplefs
 *
 *
 ****************************************************************************
 * $Id: mkfs.h 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 ****************************************************************************
*/


#ifndef _mkfs_h_
#define _mkfs_h_

/* from parseargs.c */

int parseargs(int argc, char *argv[]);


/* globals */
extern uint32_t  blksize;
extern block_t   nblks;
extern uint32_t  ninodes;
extern uint32_t  maxfreelist;
extern uint32_t  maxext;
extern char      *device;
extern char      **inputfiles;
extern char      *progname;
extern int       verbose;

#endif /* _mkfs_h_ */
