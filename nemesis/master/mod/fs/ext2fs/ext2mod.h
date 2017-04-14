/*
*****************************************************************************
**									    *
**  Copyright 1997, University of Cambridge Computer Laboratory		    *
**									    *
**  All Rights Reserved.						    *
**									    *
*****************************************************************************
**
** FACILITY:
**
**	ext2fs
** 
** FUNCTIONAL DESCRIPTION:
** 
**	ext2fs
** 
** ENVIRONMENT: 
**
**	User space server
** 
** ID : $Id: ext2mod.h 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#ifndef _ext2mod_h_
#define _ext2mod_h_

/* From Linux, but heavily modified */
#include "ext2fs.h"

#include <exceptions.h>
#include <mutex.h>
#include <time.h>
#include <IDCMacros.h>
#include <links.h>


#include <Heap.ih>
#include <IDCClientBinding.ih>
#include <USDCtl.ih>
#include <USDDrive.ih>
#include <IO.ih>
#include <IDCService.ih>
#include <IDCOffer.ih>
#include <IOEntry.ih>
#include <FSClient.ih>
#include <Ext2.ih>
#include <IDCServerBinding.ih>
#include <EntryMod.ih>
#include <CSClientStubMod.ih>
#include <CSIDCTransport.ih>

#define DBO(_x) {if (st->fs.debug) {_x;}}

#define MAX_CLIENT_HANDLES 64

typedef struct _ext2fs_st ext2fs_st;
typedef struct _fsclient_st Client_st;
typedef struct _linode_t linode_t;
typedef struct _handle_st handle_st;

/* An inode that has been loaded into memory */
/* NB this structure mostly came from Linux, but has had members ripped
   out because we only ever deal with one ext2fs in this filesystem server. */

struct ext2_inode_info {
    uint32_t	i_data[15];
    uint32_t	i_flags;
    uint32_t	i_faddr;
    uint8_t	i_frag_no;
    uint8_t	i_frag_size;
    uint16_t	i_osync;
    uint32_t	i_file_acl;
    uint32_t	i_dir_acl;
    uint32_t	i_dtime;
    uint32_t	i_version;
    uint32_t	i_block_group;
    uint32_t	i_next_alloc_block;
    uint32_t	i_next_alloc_goal;
    uint32_t	i_prealloc_block;
    uint32_t	i_prealloc_count;
};

struct inode {
    struct inode *next, *prev;
    SRCThread_Mutex mu;		 /* Protects access to all members of the
				    structure */

    uint32_t	    i_ino;	 /* Inode number, presumably */
    uint16_t	    i_mode;
    uint16_t	    i_nlink;
    uint16_t	    i_uid;
    uint16_t	    i_gid;
    uint32_t	    i_size;	 /* XXX isn't this a bit small? */
    uint32_t	    i_atime;
    uint32_t	    i_mtime;
    uint32_t	    i_ctime;
    uint32_t	    i_blocks;	 /* XXX phys or logical blocks? */
    uint32_t	    i_version;	 /* file version (for NFS) */

    uint16_t	    i_count;	 /* Reference count */
    uint16_t	    i_flags;	 /* Currently unused */
    bool_t	    i_dirt;	 /* True if modified */
    union {
	struct ext2_inode_info ext2_i;
    } u;
};

/* State structure for an open handle */
struct _handle_st {
    bool_t used;
    struct inode *ino; /* Inode for handle */
    bool_t open; /* Does this handle correspond to an open file? */
    uint32_t flags; /* If so, how was it opened? */
    uint32_t dirblock; /* Which block of directory to read next? */
};

/* Cached blocks of filesystem metadata */
enum blockbuf_state { buf_empty, buf_unlocked,
		      buf_locked_readonly, buf_locked_readwrite,
		      buf_locked_dirty };

struct buffer_head {
    struct buffer_head *next;
    struct buffer_head *prev;
    ext2fs_st          *st;
    Time_ns             firstused;
    Time_ns             lastused;
    uint32_t            refd;

    /* Nemesis-specific fields */
    SRCThread_Mutex mu;		 /* Controls access to this struct */
    SRCThread_Condition cv;	 /* Wait for rw access to block */
    enum blockbuf_state state;	 /* State of this buf */

    /* Field names stolen from Linux */
    uint32_t b_blocknr;		 /* Logical block in this buf */
    uint8_t *b_data;		 /* Pointer to data */
    uint32_t b_count;		 /* How many references are there to 
				    this block? */
    uint32_t b_size;		 /* Size of this block */
};

/* State structure for a filesystem client */

struct _fsclient_st {
    struct _fsclient_st *next, *prev;

    ext2fs_st *fs; /* Pointer back to main ext2fs state */
    
    Ext2_cl	     cl;      /* Server for this client */
    IDCServerBinding_clp binding; /* ...and the corresponding ServerBinding */
    
    FSTypes_QoS	    qos;       /* QoS for client */

    IDCOffer_clp    usd;       /* IDCOffer for client's connection to USD */
    USD_StreamID    usd_stream; /* Our handle on USD state for the stream */

    handle_st handles[MAX_CLIENT_HANDLES]; /* List of open handles */
};

/* USD-related stuff */
struct ext2_usd_st {
    IDCOffer_cl         *drive_offer;
    IDCClientBinding_cl *drive_binding;
    USDDrive_clp         usddrive;
    uint32_t             partition;  
    Heap_cl             *heap;	      /* Writable by USD domain */

    IDCClientBinding_clp binding;     /* Binding for USDCtl */
    USDCtl_clp	         usdctl;
    USDCallback_cl      *l1_callback; /* L1 Callbacks */
    USDCallback_cl       l2_callback; /* L2 Callbacks */

    IDCService_clp       callback_service;
    Entry_clp	         callback_entry;
    Thread_clp	         callback_thread;
    uint32_t             partition_size;   /* In physical blocks */
    uint32_t             phys_block_size;
};

/* Block cache */
struct bcache {
    uint32_t            blocks;     /* Number of logical blocks in cache */
    struct buffer_head *block_cache;
    Stretch_clp	        str;        /* Stretch of our data buffer  */
    Stretch_Size        size;       /* Size of the above buffer	   */
    char	       *buf;	    /* Read buffer itself	   */
    
    SRCThread_Mutex mu;		    /* Controls access to this struct */
    SRCThread_Condition freebufs;   /* Wait for free buffers */
    struct buffer_head  bufs;	    /* bufs */
};

struct clients {
    Entry_clp	   entry;    /* An entry for all our IDC needs... */
    IDCService_clp service;  /* Service; destroyed during unmount */
    IDCCallback_cl callback; /* Called for client connections */
    Client_st	   clients;  /* Linked list of clients */
};

/* Useful filesystem information */
struct filesystem_const {
    uint32_t  block_size;	    /* logical block size		*/
    uint32_t  ngroups;		    /* number of groups in the fs	*/
    uint32_t  frag_size;	    /* fragment size in bytes		*/
    uint32_t  frags_per_block;	    /* num frags in a (logical) block	*/
    uint32_t  inodes_per_block;	    /* inodes in a block of inode table */
    uint32_t  frags_per_group;	    /* num frags in a group		*/
    uint32_t  blocks_per_group;	    /* num blocks in a group		*/
    uint32_t  inodes_per_group;	    /* num inodes in a group		*/
    uint32_t  itb_per_group;	    /* num inode table blocks per group */
    uint32_t  desc_per_block;	    /* num group descriptors per block	*/
    uint32_t  descb_per_group;	    /* desc blocks per group		*/
    uint32_t  addr_per_block;	    /* num block addresses in a block	*/
    uint32_t  addr_per_block_bits;
    uint16_t  maj_rev;
    uint16_t  min_rev;
};

struct filesystem {
    bool_t clean;
    /* Mount flags */
    bool_t readonly;
    bool_t debug;
};

/* State structure for a mounted filesystem */

struct _ext2fs_st {
    /* A heap for filesystem state */
    Heap_clp	   heap;

    /* General IDC and IO stuff */
    EntryMod_clp       entrymod;
    IDCTransport_clp   shmtransport;
    CSIDCTransport_clp csidc;

    /* Disk-related things */
    struct ext2_usd_st disk;

    /* State related to clients */
    struct clients client;

    IOEntry_cl	*fio_entry;	    /* IO entry for FileIO operations	*/

    /* Filesystem information */
    sblock_t *superblock;	    /* Link to raw superblock data	*/
    struct filesystem_const fsc;    /* Information computed from superblock */
    struct filesystem       fs;     /* Mutable information related
				       to superblock and fs as a whole */

    /* Metadata cache */
    struct bcache cache;

    /* Group descriptors */
    gdesc_t  **gdesc;

#define INOHSZ	64
#if	((INOHSZ&(INOHSZ-1)) == 0)
#define INOHASH(ino)	((ino)&(INOHSZ-1))
#else
#define INOHASH(ino)	(((unsigned)(ino))%INOHSZ)
#endif
    /* Cache of inodes (INOHSZ lengh table w/ links) */
    struct inode *in_cache; 
};

#define BYTES(_st,_n_fsblks) \
((_n_fsblks) * ((_st)->fsc.block_size))

#define PHYS_BLKS(_st,_n_fsblks) \
(BYTES((_st),_n_fsblks)/(_st)->disk.phys_block_size)

#define LOGICAL_BLKS(_st,_n_phys) \
(((_n_phys)*(_st)->disk.phys_block_size)/(_st)->fsc.block_size)
    
    
/* 
** Some prototypes of utility functions from futils.c 
*/
extern bool_t physical_read(ext2fs_st *st, block_t start, uint32_t num, 
			    char *buf, uint32_t buf_size);
extern bool_t logical_read(ext2fs_st *st, block_t start, uint32_t num,
			   char *buf, uint32_t buf_size);

/*
** Stuff from ext2mod.c
*/
extern void ext2_warning(ext2fs_st *st, const char *function,
			 const char *fmt, ...);
extern void ext2_error(ext2fs_st *st, const char *function,
		       const char *fmt, ...);
			 
/*
** Stuff from inode.c
*/
extern bool_t init_inodes(ext2fs_st *st);
extern struct inode *get_inode(ext2fs_st *st, uint32_t inum);
extern void release_inode(ext2fs_st *st, struct inode * inode);
uint32_t ext2_bmap (ext2fs_st *st, struct inode * inode, int block);

/*
** Stuff from ext2.c 
*/
extern Ext2_op fs_ms;
extern bool_t check_handle(Client_st *cur, Ext2_Handle handle);
extern bool_t check_inode_handle(Client_st *cur, Ext2_Handle handle);
extern bool_t check_file_handle(Client_st *cur, Ext2_Handle handle);

/*
** Stuff from bcache.c
*/
extern
struct buffer_head *bread(ext2fs_st *st, uint32_t block, bool_t rw);
extern struct buffer_head *balloc(ext2fs_st *st);
extern struct buffer_head *bfind(ext2fs_st *st, uint32_t block);
extern void brelse(struct buffer_head *buf);
extern void bflush(struct buffer_head *buf);
extern void init_block_cache(ext2fs_st *st);

/* 
** Stuff from fsclient.c
*/
/*extern const CSClientStubMod_cl stubmod_cl; */


#endif /* _ext2mod_h_ */
