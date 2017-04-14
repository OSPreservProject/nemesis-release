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
**      app/test/ext2fs
**
** FUNCTIONAL DESCRIPTION:
** 
**      Definitions and structures for the 2nd Extended File System.
** 
** ENVIRONMENT: 
**
**      User-land.
** 
** 
**
*/

#ifndef _ext2fs_h
#define _ext2fs_h

#include <FSClient.ih>
#include <IO.ih>
#include <IOMacros.h>
#include <USD.ih>
#include <USDCtl.ih>
#include <IOMacros.h>
#include <USDCallback.ih>


/*
 * The second extended file system version
 */
#define EXT2FS_DATE		"95/08/09"
#define EXT2FS_VERSION		"0.5b"

/*
 * File system states
 */
#define EXT2_VALID_FS                   0x0001  /* Unmounted cleanly */
#define EXT2_ERROR_FS                   0x0002  /* Errors detected */

/*
 * ACL structures
 */
struct ext2_acl_header	/* Header of Access Control Lists */
{
    uint32_t	aclh_size;
    uint32_t	aclh_file_count;
    uint32_t	aclh_acle_count;
    uint32_t	aclh_first_acle;
};

struct ext2_acl_entry	/* Access Control List Entry */
{
    uint32_t	acle_size;
    uint16_t	acle_perms;	/* Access permissions */
    uint16_t	acle_type;	/* Type of entry */
    uint16_t	acle_tag;	/* User or group identity */
    uint16_t	acle_pad1;
    uint32_t	acle_next;	/* Pointer on next entry for the */
                                /* same inode or on next free entry */
};

/*
 * Structure of a blocks group descriptor
 */
typedef struct ext2_group_desc {
    uint32_t	bg_block_bitmap;	/* Blocks bitmap block */
    uint32_t	bg_inode_bitmap;	/* Inodes bitmap block */
    uint32_t	bg_inode_table;		/* Inodes table block */
    uint16_t	bg_free_blocks_count;	/* Free blocks count */
    uint16_t	bg_free_inodes_count;	/* Free inodes count */
    uint16_t	bg_used_dirs_count;	/* Directories count */
    uint16_t	bg_pad;
    uint32_t	bg_reserved[3];
} gdesc_t;


/*
 * Constants relative to the data blocks
 */
#define EXT2_NDIR_BLOCKS                12
#define EXT2_IND_BLOCK                  EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK                 (EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK                 (EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS                   (EXT2_TIND_BLOCK + 1)

/*
 * Inode flags
 */
#define EXT2_SECRM_FL                   0x00000001 /* Secure deletion */
#define EXT2_UNRM_FL                    0x00000002 /* Undelete */
#define EXT2_COMPR_FL                   0x00000004 /* Compress file */
#define EXT2_SYNC_FL                    0x00000008 /* Synchronous updates */
#define EXT2_IMMUTABLE_FL               0x00000010 /* Immutable file */
#define EXT2_APPEND_FL                  0x00000020 /* writes to file may only ap
pend */
#define EXT2_NODUMP_FL                  0x00000040 /* do not dump file */
#define EXT2_RESERVED_FL                0x80000000 /* reserved for ext2 lib */


/*
 * Structure of an inode on the disk
 */
typedef struct ext2_inode {
    uint16_t	i_mode;		/* File mode */
    uint16_t	i_uid;		/* Owner Uid */
    uint32_t	i_size;		/* Size in bytes */
    uint32_t	i_atime;	/* Access time */
    uint32_t	i_ctime;	/* Creation time */
    uint32_t	i_mtime;	/* Modification time */
    uint32_t	i_dtime;	/* Deletion Time */
    uint16_t	i_gid;		/* Group Id */
    uint16_t	i_links_count;	/* Links count */
    uint32_t	i_blocks;	/* Blocks count XXX NB: of 512 byte blocks */
    uint32_t	i_flags;	/* File flags */
    union {
	struct {
	    uint32_t  l_i_reserved1;
	} linux1;
	struct {
	    uint32_t  h_i_translator;
	} hurd1;
	struct {
	    uint32_t  m_i_reserved1;
	} masix1;
    } osd1;				/* OS dependent 1 */
    uint32_t	i_block[EXT2_N_BLOCKS]; /* Ptrs to blocks XXX NB: fs blocks */
    uint32_t	i_version;	/* File version (for NFS) */
    uint32_t	i_file_acl;	/* File ACL */
    uint32_t	i_dir_acl;	/* Directory ACL */
    uint32_t	i_faddr;	/* Fragment address */
    union {
	struct {
	    uint8_t	l_i_frag;	/* Fragment number */
	    uint8_t	l_i_fsize;	/* Fragment size */
	    uint16_t	i_pad1;
	    uint32_t	l_i_reserved2[2];
	} linux2;
	struct {
	    uint8_t	h_i_frag;	/* Fragment number */
	    uint8_t	h_i_fsize;	/* Fragment size */
	    uint16_t	h_i_mode_high;
	    uint16_t	h_i_uid_high;
	    uint16_t	h_i_gid_high;
	    uint32_t	h_i_author;
	} hurd2;
	struct {
	    uint8_t	m_i_frag;	/* Fragment number */
	    uint8_t	m_i_fsize;	/* Fragment size */
	    uint16_t	m_pad1;
	    uint32_t	m_i_reserved2[2];
	} masix2;
    } osd2;				/* OS dependent 2 */
} inode_t;


/* definitions for the i_mode field of inodes */
#define S_IFMT   0170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001


/*
 * Structure of a directory entry
 */
#define EXT2_NAME_LEN 255

typedef struct ext2_dir_entry {
    uint32_t inode;			/* Inode number */
    uint16_t rec_len;		        /* Directory entry length */
    uint16_t name_len;		        /* Name length */
    char    name[EXT2_NAME_LEN];	/* File name */
} ext2_dir_t;


/*
 * Special inodes numbers
 */
#define	EXT2_BAD_INO		 1	/* Bad blocks inode */
#define EXT2_ROOT_INO		 2	/* Root inode */
#define EXT2_ACL_IDX_INO	 3	/* ACL inode */
#define EXT2_ACL_DATA_INO	 4	/* ACL inode */
#define EXT2_BOOT_LOADER_INO	 5	/* Boot loader inode */
#define EXT2_UNDEL_DIR_INO	 6	/* Undelete directory inode */

/* First non-reserved inode for old ext2 filesystems */
#define EXT2_GOOD_OLD_FIRST_INO	11

/*
 * The second extended file system magic number
 */
#define EXT2_SUPER_MAGIC	0xEF53

/*
 * Maximal count of links to a file
 */
#define EXT2_LINK_MAX		32000

/*
 * Macro-instructions used to manage several block sizes
 */
#define EXT2_MIN_BLOCK_SIZE		1024
#define	EXT2_MAX_BLOCK_SIZE		4096
#define EXT2_MIN_BLOCK_LOG_SIZE		  10


/*
 * Revision levels
 */
#define EXT2_GOOD_OLD_REV	0	/* The good old (original) format */
#define EXT2_DYNAMIC_REV	1 	/* V2 format w/ dynamic inode sizes */

#define EXT2_CURRENT_REV	EXT2_GOOD_OLD_REV
#define EXT2_MAX_SUPP_REV	EXT2_DYNAMIC_REV

#define EXT2_GOOD_OLD_INODE_SIZE 128

/* Pass fs state pointer */
#define EXT2_BLOCK_SIZE(s)	((s)->fsc.block_size)
#define	EXT2_ADDR_PER_BLOCK(s)	((s)->fsc.addr_per_block)
#define EXT2_ADDR_PER_BLOCK_BITS(s) ((s)->fsc.addr_per_block_bits)

/* Pass superblock pointer */
#define EXT2_INODE_SIZE(s)	(((s)->s_rev_level==EXT2_GOOD_OLD_REV) ? \
				 EXT2_GOOD_OLD_INODE_SIZE : \
				 (s)->s_inode_size)
#define EXT2_FIRST_INO(s)	(((s)->s_rev_level==EXT2_GOOD_OLD_REV) ? \
				 EXT2_GOOD_OLD_FIRST_INO : \
				 (s)->s_first_ino)

/*
 * Macro-instructions used to manage fragments
 */
#define EXT2_MIN_FRAG_SIZE		1024
#define	EXT2_MAX_FRAG_SIZE		4096
#define EXT2_MIN_FRAG_LOG_SIZE		  10
#define EXT2_FRAG_SIZE(_st)		(_st->fsc.frag_size)
#define EXT2_FRAGS_PER_BLOCK(s)	        (_st->fsc.frags_per_block)

/* Superblock stuff */
typedef struct ext2_super_block sblock_t;
struct ext2_super_block {
    uint32_t   s_inodes_count;     /* Inodes count */
    uint32_t   s_blocks_count;     /* Blocks count */
    uint32_t   s_r_blocks_count;   /* Reserved blocks count */
    uint32_t   s_free_blocks_count;    /* Free blocks count */
    uint32_t   s_free_inodes_count;    /* Free inodes count */
    uint32_t   s_first_data_block; /* First Data Block */
    uint32_t   s_log_block_size;   /* Block size */
    int32_t    s_log_frag_size;    /* Fragment size */
    uint32_t   s_blocks_per_group; /* # Blocks per group */
    uint32_t   s_frags_per_group;  /* # Fragments per group */
    uint32_t   s_inodes_per_group; /* # Inodes per group */
    uint32_t   s_mtime;        /* Mount time */
    uint32_t   s_wtime;        /* Write time */
    uint16_t   s_mnt_count;        /* Mount count */
    int16_t    s_max_mnt_count;    /* Maximal mount count */
    uint16_t   s_magic;        /* Magic signature */
    uint16_t   s_state;        /* File system state */
    uint16_t   s_errors;       /* Behaviour when detecting errors */
    uint16_t   s_minor_rev_level;  /* minor revision level */
    uint32_t   s_lastcheck;        /* time of last check */
    uint32_t   s_checkinterval;    /* max. time between checks */
    uint32_t   s_creator_os;       /* OS */
    uint32_t   s_rev_level;        /* Revision level */
    uint16_t   s_def_resuid;       /* Default uid for reserved blocks */
    uint16_t   s_def_resgid;       /* Default gid for reserved blocks */
    /*
     * These fields are for EXT2_DYNAMIC_REV superblocks only.
     *
     * Note: the difference between the compatible feature set and
     * the incompatible feature set is that if there is a bit set
     * in the incompatible feature set that the kernel doesn't
     * know about, it should refuse to mount the filesystem.
     *
     * e2fsck's requirements are more strict; if it doesn't know
     * about a feature in either the compatible or incompatible
     * feature set, it must abort and not try to meddle with
     * things it doesn't understand...
     */
    uint32_t   s_first_ino;        /* First non-reserved inode */
    uint16_t   s_inode_size;       /* size of inode structure */
    uint16_t   s_block_group_nr;   /* block group # of this superblock */
    uint32_t   s_feature_compat;   /* compatible feature set */
    uint32_t   s_feature_incompat;     /* incompatible feature set */
    uint32_t   s_feature_ro_compat;    /* readonly-compatible feature set */
    uint8_t    s_uuid[16];         /* 128-bit uuid for volume */
    uint8_t    s_volume_name[16];  /* volume name */
    uint8_t    s_last_mounted[64]; /* directory where last mounted */
    uint32_t   s_reserved[206];    /* Padding to the end of the block */
};



#define SBLOCK_BLOCK 1                  /* Superblock offset in fs blocks  */
#define SBLOCK_SIZE  (sizeof(sblock_t)) /* Size in bytes of superblock     */

#define SBLOCK_DOFF(_s) \
((SBLOCK_BLOCK * EXT2_MIN_BLOCK_SIZE) / (_s)->disk.phys_block_size)
#define SBLOCK_DBLKS(_s) (EXT2_MIN_BLOCK_SIZE / (_s)->disk.phys_block_size)

/* Operating system types used in superblock s_creator_os field */
#define EXT2_OS_LINUX       0
#define EXT2_OS_HURD        1
#define EXT2_OS_MASIX       2
#define EXT2_OS_FREEBSD     3
#define EXT2_OS_LITES       4
#define EXT2_OS_MAX         4

typedef uint32_t block_t;    /* a block number within our fs */

#define EXT2_MAX_GROUP_LOADED 8

#endif /* _ext2fs_h */
