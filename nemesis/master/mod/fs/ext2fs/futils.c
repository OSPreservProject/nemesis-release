/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/fs/ext2fs
**
** FUNCTIONAL DESCRIPTION:
** 
**      Misc. utils for dealing with ext2fs stuff.
** 
** ENVIRONMENT: 
**
**      Module.
** 
** 
**
*/

#include <nemesis.h>
#include <stdio.h>

#include <ntsc.h>
#include "ext2mod.h"

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif
#define DB(x)  x

#define UNIMPLEMENTED  printf("%s: UNIMPLEMENTED\n", __FUNCTION__);


/*
** physical_read: attempt to read 'num' physical blocks starting at physical
** block 'start' into st->buf. Fails if st->buf is not large enough, 
** suceeds otherwise (for now!) 
*/
PUBLIC bool_t physical_read(ext2fs_st *st, 
			    block_t    start, 
			    uint32_t   num,
			    char      *buf, 
			    uint32_t   buf_size)
{
    USD_Extent   extent;
    USDCtl_Error rc;
    uint32_t     readlen;
    
    readlen = num * st->disk.phys_block_size;

    /* Check we're initialised, and have enough space for the read */
    if(!st->disk.usdctl || (readlen > buf_size)) 
	return False;

    extent.base = start;
    extent.len  = num;
    TRC(printf("EXT2: %d,%d -> %p\n", start, num, buf));

    TRC(printf("[%d]\n", block));
    rc = USDCtl$Request(st->disk.usdctl, &extent, FileIO_Op_Read, buf);
    if (rc != USDCtl_Error_None) return False;

    return True;
}

/*
** logical_read: once the filesystem has been 'mounted' (i.e. the superblock
** read), we can deal with ext2fs 'logical' block sizes. 
** This function attempts to read 'num' logical blocks starting at logical
** block 'start' into st->buf. Fails if st->buf is not large enough, 
** suceeds otherwise.
*/
PUBLIC bool_t logical_read(ext2fs_st *st, 
			   block_t    start, 
			   uint32_t   num, 
			   char      *buf, 
			   uint32_t   buf_size)
{
    return(physical_read(st, PHYS_BLKS(st,start), PHYS_BLKS(st,num),
			 buf, buf_size));
}

