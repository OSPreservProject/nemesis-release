/*
 *  linux/fs/ext2/inode.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/inode.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Goal-directed block allocation by Stephen Tweedie (sct@dcs.ed.ac.uk), 1993
 */

/*
**
** FACILITY:
**
**      mod/fs/ext2fs
** 
** FUNCTIONAL DESCRIPTION:
** 
**      ext2fs
** 
** ENVIRONMENT: 
**
**      User-space filesystem
** 
** ID : $Id: inode.c 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include "ext2mod.h"


#ifdef DEBUG_INODE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define inode_bmap(inode, nr) ((inode)->u.ext2_i.i_data[(nr)])

static inline uint32_t block_bmap (ext2fs_st *st, uint32_t i, uint32_t nr)
{
    int                 tmp;
    struct buffer_head *buf;
    
    buf = bread(st, i, False);

    tmp = ((uint32_t *) buf->b_data)[nr];

    brelse (buf);
    return tmp;
}

/* This function looks like it returns a disk block number for a block
   in an inode. All blocks are logical blocks, I think. */

PUBLIC uint32_t ext2_bmap (ext2fs_st *st, struct inode * inode, int block)
{
    int i;
    int addr_per_block      = EXT2_ADDR_PER_BLOCK(st);
    int addr_per_block_bits = EXT2_ADDR_PER_BLOCK_BITS(st);

    if (block < 0) {
	printf("ext2fs: ext2_bmap: block < 0\n");
	return 0;
    }
    if (block >= EXT2_NDIR_BLOCKS + addr_per_block +
	(1 << (addr_per_block_bits * 2)) +
	((1 << (addr_per_block_bits * 2)) << addr_per_block_bits)) {
	printf("ext2fs: ext2_bmap: block > big\n");
	return 0;
    }
    if (block < EXT2_NDIR_BLOCKS)
	return inode_bmap (inode, block);
    block -= EXT2_NDIR_BLOCKS;
    if (block < addr_per_block) {
	i = inode_bmap (inode, EXT2_IND_BLOCK);
	if (!i)
	    return 0;
	return block_bmap (st, i, block);
    }
    block -= addr_per_block;
    if (block < (1 << (addr_per_block_bits * 2))) {
	i = inode_bmap (inode, EXT2_DIND_BLOCK);
	if (!i)
	    return 0;
	i = block_bmap (st, i,
			block >> addr_per_block_bits);
	if (!i)
	    return 0;
	return block_bmap (st, i,
			   block & (addr_per_block - 1));
    }
    block -= (1 << (addr_per_block_bits * 2));
    i = inode_bmap (inode, EXT2_TIND_BLOCK);
    if (!i)
	return 0;
    i = block_bmap (st, i,
		    block >> (addr_per_block_bits * 2));
    if (!i)
	return 0;
    i = block_bmap (st, i,
		    (block >> addr_per_block_bits) & (addr_per_block - 1));
    if (!i)
	return 0;
    return block_bmap (st, i,
		       block & (addr_per_block - 1));
}

PUBLIC bool_t init_inodes(ext2fs_st *st)
{
    int i; 

    if(!(st->in_cache = Heap$Malloc(Pvs(heap), INOHSZ*sizeof(struct inode)))) 
	return False;

    for(i = 0; i < INOHSZ; i++) {
	st->in_cache[i].i_ino = EXT2_BAD_INO;
	Q_INIT(&(st->in_cache[i]));
    }

    return True;
}


/* 
** get_inode: if we have a cached copy of inode number 'inum', 
** return it immediately; otherwise compute the group number 
** and read from disk, storing in cache. 
*/
PUBLIC struct inode *get_inode(ext2fs_st *st, uint32_t inum)
{
    uint32_t            group_no, group_inum;
    block_t             block, boffset;
    struct inode       *hd, *cur, *inode;
    struct buffer_head *buf;
    int                 i;
    inode_t            *raw_inode;
    
    if (inum < EXT2_ROOT_INO) /* smallest valid inode */
	return NULL;
    
    /* check if we have a copy in the cache */
    hd = &(st->in_cache[INOHASH(inum)]);
    for (cur = hd->next; cur != hd; cur = cur->next) {
	if (cur->i_ino == inum) {
	    TRC(printf("ext2fs: found inode %d in cache.\n", inum));
	    MU_LOCK(&cur->mu);
	    cur->i_count++;
	    MU_RELEASE(&cur->mu);
	    return cur;
	}
    }
    
    TRC(printf("ext2fs: inode %d is not in the cache\n", inum));
    
    /* which group is it in? */
    group_no   = (inum - 1) / st->fsc.inodes_per_group; 
    
    /* offset within that group */    
    group_inum = (inum - 1) % st->fsc.inodes_per_group; 
    
    TRC(printf("ext2fs: inode %d => (group, off) = (%d, %d)\n", 
	       inum, group_no, group_inum));
    TRC(printf(" && inode table at block %d\n", 
	       st->gdesc[group_no]->bg_inode_table));
    
    block = st->gdesc[group_no]->bg_inode_table + 
	(group_inum / st->fsc.inodes_per_block);
    boffset = group_inum % st->fsc.inodes_per_block;
    
    TRC(printf("block is %d, boffset is %d\n", block, boffset));
    
    buf = bread(st, block, False);

    TRC(printf("fetched block\n"));
    
    inode = Heap$Malloc(st->heap, sizeof(struct inode));
    
    raw_inode = (inode_t *)(buf->b_data + (boffset * sizeof(inode_t)));

    inode->i_ino = inum;

/* From linux inode.c */
    inode->i_mode               = raw_inode->i_mode;
    inode->i_uid                = raw_inode->i_uid;
    inode->i_gid                = raw_inode->i_gid;
    inode->i_nlink              = raw_inode->i_links_count;
    inode->i_size               = raw_inode->i_size;
    inode->i_atime              = raw_inode->i_atime;
    inode->i_ctime              = raw_inode->i_ctime;
    inode->i_mtime              = raw_inode->i_mtime;
    inode->u.ext2_i.i_dtime     = raw_inode->i_dtime;
    inode->i_blocks             = raw_inode->i_blocks;
    inode->u.ext2_i.i_flags     = raw_inode->i_flags;
    inode->u.ext2_i.i_faddr     = raw_inode->i_faddr;
#if 0
    inode->u.ext2_i.i_frag_no   = raw_inode->i_frag;
    inode->u.ext2_i.i_frag_size = raw_inode->i_fsize;
#endif /* 0 */
    inode->u.ext2_i.i_osync     = 0;
    inode->u.ext2_i.i_file_acl  = raw_inode->i_file_acl;
    inode->u.ext2_i.i_dir_acl   = raw_inode->i_dir_acl;
    inode->u.ext2_i.i_version   = raw_inode->i_version;

    inode->u.ext2_i.i_block_group = block;

    inode->u.ext2_i.i_next_alloc_block = 0;
    inode->u.ext2_i.i_next_alloc_goal  = 0;
    for (i = 0; i < EXT2_N_BLOCKS; i++)
	inode->u.ext2_i.i_data[i] = raw_inode->i_block[i];
/* End of bit nicked from linux inode.c */

    brelse(buf);

    MU_INIT(&inode->mu);
    inode->i_count=1;

    /* XXX Probably need to get a mutex to do this */
    LINK_ADD_TO_HEAD(hd, inode);

    return inode;
}

/* Assume inode is not locked */
PUBLIC void release_inode(ext2fs_st *st, struct inode *inode)
{
    TRC(printf("release_inode: inode %d\n",inode->i_ino));

    MU_LOCK(&inode->mu);
    inode->i_count--;
    MU_RELEASE(&inode->mu);

    if (inode->i_count) {
	TRC(printf("release_inode: ref count > 0\n"));
	return; /* Can't do any more work */
    }

    /* It's free - consider writing it back to disk, etc. */
    TRC(printf("release_inode: ref count is zero\n"));

    /* XXX PRB Really ought to free the damned inode!! */
    /* check if we have a copy in the cache */
    LINK_REMOVE(inode);
    MU_FREE(&inode->mu);
    FREE(inode);
}
