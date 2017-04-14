/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.		                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/fs/usd
**
** FUNCTIONAL DESCRIPTION:
** 
**      Structures and definitions for OSF1 & MSDos partition tables.
** 
** ENVIRONMENT: 
**
**      Device driver (currently).
** 
** FILE :	disklabel.h
** CREATED :	Tue Jan 21 1997
** AUTHOR :	Paul Barham (Paul.Barham@cl.cam.ac.uk)
** ID : 	$Id: disklabel.h 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#ifndef _DISKLABEL_H_
#define _DISKLABEL_H_

/*
 * OSF Filesystem type and version.
 * Used to interpret other filesystem-specific
 * per-partition information.
 */
#define OSF_FS_UNUSED       0        /* unused */
#define OSF_FS_SWAP         1        /* swap */
#define OSF_FS_V6           2        /* Sixth Edition */
#define OSF_FS_V7           3        /* Seventh Edition */
#define OSF_FS_SYSV         4        /* System V */
#define OSF_FS_V71K         5        /* V7 with 1K blocks (4.1, 2.9) */
#define OSF_FS_V8           6        /* Eighth Edition, 4K blocks */
#define OSF_FS_BSDFFS       7        /* 4.2BSD fast file system */

#define OSF_FS_EXT2         8	     /* Linux EXT2 */

/* OSF will reserve 16--31 for vendor-specific entries */
#define OSF_FS_ADVFS        16       /* Digital Advanced File System */
#define OSF_FS_LSMpubl      17       /* Digital Log. Storage public region*/
#define OSF_FS_LSMpriv      18       /* Digital Log. Storage private region */
#define OSF_FS_LSMsimp      19       /* Digital Log. Storage simple disk    */


#define DISKLABELMAGIC (0x82564557UL)
#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t

struct disklabel {
    u32 d_magic;
    u16 d_type,d_subtype;
    u8 d_typename[16];
    u8 d_packname[16];
    u32 d_secsize;
    u32 d_nsectors;
    u32 d_ntracks;
    u32 d_ncylinders;
    u32 d_secpercyl;
    u32 d_secprtunit;
    u16 d_sparespertrack;
    u16 d_sparespercyl;
    u32 d_acylinders;
    u16 d_rpm, d_interleave, d_trackskew, d_cylskew;
    u32 d_headswitch, d_trkseek, d_flags;
    u32 d_drivedata[5];
    u32 d_spare[5];
    u32 d_magic2;
    u16 d_checksum;
    u16 d_npartitions;
    u32 d_bbsize, d_sbsize;
    struct d_partition {
	u32 p_size;
	u32 p_offset;
	u32 p_fsize;
	u8  p_fstype;
	u8  p_frag;
	u16 p_cpg;
    } d_partitions[8];
};



/* MSDos Partition table stuff */
typedef struct {
    uint8_t	activeflag;	/* 0x80 == active, 0x00 == inactive */
    uint8_t	SOP[3];		/* start of partition */
    uint8_t	ostype;		/* 0 == Non-DOS, 1 == FAT12, 4 == FAT16,
				   5 == Extended DOS, 6 == FAT>32Mb */
    uint8_t	EOP[3];		/* end of partition */
    uint16_t	startSecLO;
    uint16_t	startSecHI;
    uint16_t	nsecsLO;
    uint16_t	nsecsHI;
} parttbl_entry;

typedef struct {
    parttbl_entry	p[4];	/* NB: stored in reverse order */
} parttbl;

#define PARTTBL_MAGIC 0xaa55
typedef struct {
    uint8_t	MBR[446];	/* MBR boot loader */
    parttbl	ptbl;
    uint16_t	magic;
} partsect;

/* Macros to extract head, cylinder and sector number from SOP and EOP
 * entries */
#define PTBL_HEAD(x) ((x)[0])
#define PTBL_CYL(x)  ((((int)((x)[1]) & 0xc0) << 2) | ((x)[2]))
#define PTBL_SEC(x) ((x)[1] & 0x3f)

/* Filesystem types according to MSDos */
#define MSDOS_FS_FAT12    0x01
#define MSDOS_FS_FAT16    0x04
#define MSDOS_FS_EXTENDED 0x05
#define MSDOS_FS_FAT16B   0x06    /* 'Big' FAT16 (i.e. >= 32Mb)       */
#define MSDOS_FS_SWAP     0x82    /*  Linux swap partition            */
#define MSDOS_FS_EXT2     0x83    /*  Linux native (ext2fs) partition */
#define MSDOS_FS_AMOEBA   0x93    /*  Amoeba (aka native Nemesis)     */

extern bool_t osf1_partition(disk_t *disk);
extern bool_t msdos_partition(disk_t *disk);


#endif /* _DISKLABEL_H_ */
