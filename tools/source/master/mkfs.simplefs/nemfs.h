/*
 ****************************************************************************
 * (C) 1998                                                                 *
 *                                                                          *
 * University of Cambridge Computer Laboratory /                            *
 * University of Glasgow Department of Computing Science                    *
 ****************************************************************************
 *
 *        File: nemfs.h
 *      Author: Matt Holgate
 *     Changes: 
 *              
 *        Date: 1998
 * 
 * Environment: SimpleFS module / UNIX tool
 * Description: SimpleFS - file system structures
 *
 * XXX    note! This file is shared between the SimpleFS module code   XXX
 * XXX and the UNIX file system creation tool. Any changes here should XXX
 * XXX           be duplicated in mod/fs/simplefs/nemfs.h              XXX
 *
 ****************************************************************************
 * $Id: nemfs.h 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 ****************************************************************************
*/

#ifndef _nemfs_h_
#define _nemfs_h_

typedef uint32_t block_t;      /* block number (or no. of blocks) */

/* ----------structures held on disk and in memory--------------- */

/* XXX clean flag etc. */

typedef struct _superblock_t {

    uint32_t  blksize;         /* block size */
    block_t   nblks;           /* no of blocks */

    uint32_t  ninodes;         /* no of inodes */
    block_t   inodesize;       /* inode size in blocks */
    block_t   inodetbl_base;   /* block of inode table */
    block_t   inodetbl_len;    /* size of inode table in blocks */

    uint32_t  maxfreelist;     /* no of entries in free list */
    block_t   fslist_base;     /* block of free space list */
    block_t   fslist_len;      /* size of free space list in blocks */

    uint32_t  maxext;          /* maximum no of extents per file */

    block_t   datastart;       /* block of first data block */
    uint32_t  rootdir;         /* inode of root directory */


} superblock_t;

typedef struct _extent_t {
    block_t    base;           /* base block */
    block_t    len;            /* length in blocks */
} extent_t;

#define INODE_IN_USE ((uint32_t) ~0)

/* this is a hack, but I can't think of a better, neater
   way to have dynamically variable extent list sizes... */

#define INODE_STRUCT_DEF(nextents)  \
typedef struct _extentlist_t {      \
    extent_t      ext[(nextents)];  \
} extentlist_t;                     \
                                    \
typedef struct _inode_t {           \
    uint32_t      nextfree;         \
    uint64_t      size;             \
    extentlist_t  extlist;          \
} inode_t


#endif /* _nemfs_h_ */
