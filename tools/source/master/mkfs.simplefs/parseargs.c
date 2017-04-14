/*
 ****************************************************************************
 * (C) 1998                                                                 *
 *                                                                          *
 * University of Cambridge Computer Laboratory /                            *
 * University of Glasgow Department of Computing Science                    *
 ****************************************************************************
 *
 *        File: parseargs.c
 *      Author: Matt Holgate
 *     Changes: 
 *              
 *        Date: 1998
 * 
 * Environment: UNIX tool
 * Description: SimpleFS/NemFS file system creator (command line parser)
 *
 ****************************************************************************
 * $Id: parseargs.c 1.2 Thu, 15 Apr 1999 16:44:25 +0100 dr10009 $
 ****************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>

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

int parseargs(int argc, char *argv[]) {

    int i = 0;
    int fail = 0;
    char *c, *d;

    /* collect all the switches */
    while (!fail && ++i<argc && *(c = argv[i]) == '-') {
	if ((*++c != '\0') && (*(c+1) == '\0')) {
	    switch (*c) {
	    case 'v':
		verbose = 1;
		break;
	    case 'b':
		if (++i>=argc) fail = 1;
		else {
		    blksize = (uint32_t) strtoul(argv[i], &d, 10);
		    if (*d != '\0') fail = 1;
		}
		break;
	    case 'n':
		if (++i>=argc) fail = 1;
		else {
		    nblks = (block_t) strtoul(argv[i], &d, 10);
		    if (*d != '\0') fail = 1;
		}
		break;
	    case 'i':
		if (++i>=argc) fail = 1;
		else {
		    ninodes = (uint32_t) strtoul(argv[i], &d, 10);
		    if (*d != '\0') fail = 1;
		}
		break;
	    case 'e':
		if (++i>=argc) fail = 1;
		else {
		    maxext = (uint32_t) strtoul(argv[i], &d, 10);
		    if (*d != '\0') fail = 1;
		}
		break;
	    case 'f':
		if (++i>=argc) fail = 1;
		else {
		    maxfreelist = (uint32_t) strtoul(argv[i], &d, 10);
		    if (*d != '\0') fail = 1;
		}
		break;
	    default:
		fail = 1;
		break;
	    }

	}
	else fail = 1;
    }

    inputfiles = &argv[argc];  /* default for no parameters */
    if (!fail && i<argc) {
	device = argv[i];
	inputfiles = &argv[i+1];
    }
    else fail = 1;
    
    if (fail) {
	printf("Usage: %s [-v] [-b block-size] [-n no-blocks] [-i no-inodes] [-e max-extents-per-file] "
	       "[-f max-free-list-size] device [file1, ..., fileN]\n", argv[0]);
	return 0;
    }
    
    return 1;


}
