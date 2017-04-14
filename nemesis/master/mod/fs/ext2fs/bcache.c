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
**      Simple buffer cache.
** 
** ENVIRONMENT: 
**
**      In the ext2fs server (for now). 
** 
** ID : $Id: bcache.c 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include "ext2mod.h"

//#define DEBUG_BCACHE
#ifdef DEBUG_BCACHE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif


/* 
** XXX PRB: This is a minimalist hack to turn the original
** direct-mapped buffer cache nightmare into a LRU scheme.  It 
** should perform a lot better in the readonly case... I've tried to 
** leave in all of the really hideous multiple reader/single writer 
** locking stuff but since the file system is readonly, I'm not sure 
** if it'll work! 
** 
** Should it all screw up, RCS is your friend! :-) 
*/


/* See if a block is in the cache... inefficient but who cares!
   Frequently referenced blocks are at the front of the list.  */
struct buffer_head *bfind(ext2fs_st *st, uint32_t block)
{
    struct buffer_head *b;

    TRC(printf("bfind %d -> ", block));
    b = st->cache.bufs.next;
    while (b->next != st->cache.bufs.next) {
	if ((b->b_blocknr == block) &&
	    (b->state != buf_empty)) {
	    TRC(printf("%p\n", b));
	    return b;
	}
	b = b->next;
    }
    TRC(printf("NULL\n"));
    return NULL;
}

/* Scan the cache in reverse order starting from the least recently
   used.  If we find a suitable victim, return that. */
struct buffer_head *balloc(ext2fs_st *st)
{
    struct buffer_head *b;

    TRC(printf("balloc -> "));
    b = st->cache.bufs.prev;
    while (b->prev != st->cache.bufs.prev) {
	if (b->state <= buf_unlocked) {
	    TRC(printf("%p\n", b));
	    return b;
	}
	b = b->prev;
    }
    TRC(printf("NULL\n"));
    return NULL;
}

/* Read a block from the fs, returning a pointer to a struct blockbuf. The
   block is locked as appropriate. YOU MUST CALL brelse() once you are
   finished with the block, otherwise it will sit around for ages. */
struct buffer_head *bread(ext2fs_st *st, uint32_t block, bool_t rw)
{
    struct buffer_head *buf;

    TRC(printf("bread %d\n", block));
    MU_LOCK(&st->cache.mu);

    buf = bfind(st, block);

    if (buf) {

	LINK_REMOVE(buf);
	MU_LOCK(&buf->mu);	/* Lock the buffer */
	LINK_ADD_TO_HEAD(&st->cache.bufs, buf);	/* Pull to front */
	MU_RELEASE(&st->cache.mu); /* Don't need the cache lacked any more */

	if (rw) {
	    /* We're going for exclusive access. Anything else just
	       isn't good enough. */
	retry:
	    if (buf->state == buf_unlocked) {
		buf->state = buf_locked_readwrite;
		TRC(printf("ext2fs: cached block %d now locked readwrite\n",
			   block));
		/* ASSERT b_count==0 */
		MU_RELEASE(&buf->mu);
		return buf;
	    }
	    
	    TRC(printf("Correct block cached, but already inuse -> retry\n"));
	    
	    /* We can't get it now; wait for a signal and try again */
	    WAIT(&buf->mu, &buf->cv);
	    goto retry;
	} else {
	    buf->state = buf_locked_readonly;
	}
	buf->b_count++;

	buf->lastused = NOW();
	buf->refd++;

	MU_RELEASE(&buf->mu);
	return buf;

    } else {
	/* Wait on the LRU buffer */
	while (! (buf = balloc(st))) {
	    TRC(printf("waiting...\n"));
	    WAIT(&st->cache.mu, &st->cache.freebufs);
	    TRC(printf("retry...\n"));
	}
	LINK_REMOVE(buf);

	if (buf->state) {
	    TRC(printf("E %d %d %d %d\n", 
		   buf->b_blocknr, buf->refd,
		   (buf->lastused - buf->firstused) / MILLISECS(1),
		   (NOW() - buf->lastused) / MILLISECS(1)));
	}

	MU_RELEASE(&st->cache.mu);
	/* At this point the block is either unlocked or empty. We can fetch
	   a new block into it. */
	TRC(printf("reading %d\n", block));
	memset(buf->b_data, 0xaa, st->fsc.block_size);
	if (!logical_read(st, block, 1, buf->b_data, st->fsc.block_size)) {
	    printf("ext2fs: getblock: read of block %d failed\n",
		   block);
	    /* XXX what now? */
	}
	buf->firstused = buf->lastused  = NOW();
	buf->refd      = 1;
	buf->b_size    = st->fsc.block_size;
	buf->b_blocknr = block;
	buf->b_count++;
	if (rw) {
	    buf->state = buf_locked_readwrite;
	} else {
	    buf->state = buf_locked_readonly;
	    /* ASSERT refcount==1 */
	}
	MU_LOCK(&st->cache.mu);
	LINK_ADD_TO_HEAD(&st->cache.bufs, buf);	/* Pull to front */
	MU_RELEASE(&st->cache.mu);
    }

    return buf;
}

void brelse(struct buffer_head *buf)
{
    ext2fs_st *st = buf->st;

    TRC(printf("brelse %d\n", buf->b_blocknr));
    /* We ASSUME that whoever is releasing the block is allowed to do so. */
    MU_LOCK(&buf->mu);

    switch (buf->state) {
    case buf_empty:
	printf("ext2fs: releasing empty buffer. This is bad.\n");
	break;
    case buf_unlocked:
	printf("ext2fs: releasing unlocked buffer. This is bad.\n");
	break;
    case buf_locked_readonly:
	buf->b_count--;
	if (buf->b_count==0) {
	    buf->state=buf_unlocked;
	    TRC(printf("ext2fs: cached block %d(ro) now unlocked\n",
		       buf->b_blocknr));
	    MU_RELEASE(&buf->mu);
	    SIGNAL(&buf->cv);
	    SIGNAL(&st->cache.freebufs);
	}
	return;
	break;
    case buf_locked_readwrite:
	buf->b_count--;
	buf->state=buf_unlocked;
	TRC(printf("ext2fs: cached block %d(rw) now unlocked\n",
		   buf->b_blocknr));
	MU_RELEASE(&buf->mu);
	SIGNAL(&buf->cv);
	return;
    case buf_locked_dirty:
	printf("ext2fs: unlocking dirty block NOT IMPLEMENTED\n");
	/* logical_write(); */
	buf->state=buf_unlocked;
	MU_RELEASE(&buf->mu);
	SIGNAL(&buf->cv);
	return;
    }
    /* NOTREACHED, hopebufsy */
    printf("brelse: NOTREACHED\n");
  
}
	
/* If a block is dirty then flush it to disk */
void bflush(struct buffer_head *buf)
{
    printf("ext2fs: bflush: unimplemented\n");
}

void init_block_cache(ext2fs_st *st)
{
    int i;

    st->cache.blocks = st->cache.size / st->fsc.block_size;
    DBO(printf("ext2fs: block cache contains %d blocks\n",st->cache.blocks));
    st->cache.block_cache = Heap$Malloc(st->heap,
					(sizeof(*st->cache.block_cache) * 
					 st->cache.blocks));
    DBO(printf("ext2fs: block cache descriptors at [%p,%p)\n",
	       st->cache.block_cache, (void *)st->cache.block_cache + 
	       (sizeof(*st->cache.block_cache) * st->cache.blocks) - 1));
    MU_INIT(&st->cache.mu);
    CV_INIT(&st->cache.freebufs);
    LINK_INIT(&st->cache.bufs);

    for (i = 0; i < st->cache.blocks; i++) {
	struct buffer_head *buf;

	buf = &st->cache.block_cache[i];
	LINK_ADD_TO_TAIL(&st->cache.bufs, buf);
	MU_INIT(&buf->mu);
	CV_INIT(&buf->cv);
	buf->st        = st;
	buf->b_blocknr = 0;
	buf->state     = buf_empty;
	buf->b_count   = 0;
	buf->b_data    = st->cache.buf+(i*st->fsc.block_size);
	buf->b_size    = st->fsc.block_size;
    }
}
