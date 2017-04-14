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
**      dev/USD
**
** FUNCTIONAL DESCRIPTION:
** 
**      Read disk partition table
** 
** ENVIRONMENT: 
**
**      Part of user-safe disk interface
** 
** FILE :	partition.c
** CREATED :	Tue Jan 21 1997
** AUTHOR :	Paul Barham (Paul.Barham@cl.cam.ac.uk)
** ID : 	$Id: partition.c 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <bstring.h>
#include <salloc.h>    

#include "usd.h"

typedef union __bootblock {
    struct {
        char            __pad1[64];
        struct disklabel    __label;
    } __u1;
    struct {
    unsigned long       __pad2[63];
    unsigned long       __checksum;
    } __u2;
    char        bootblock_bytes[512];
    unsigned long   bootblock_quadwords[64];
} bootblock;

#define bootblock_label     __u1.__label
#define bootblock_checksum  __u2.__checksum



#ifdef PTAB_TRACE
#define PTRC(_x) _x
#else
#define PTRC(_x) _x
#endif

static const char * const osf_fstypenames[] = {
        "unused",
        "swap",
        "Version 6",
        "Version 7",
        "System V",
        "4.1BSD",
        "Eighth Edition",
        "4.2BSD",
        "ext2",
        "resrvd9",
        "resrvd10",
        "resrvd11",
        "resrvd12",
        "resrvd13",
        "resrvd14",
        "resrvd15",
        "ADVfs",
        "LSMpubl",
        "LSMpriv",
        "LSMsimp",
        0
};
#define FSMAXTYPES (sizeof(osf_fstypenames) / sizeof(osf_fstypenames[0]) - 1)

PUBLIC bool_t osf1_partition(disk_t *disk)
{ 
    struct disklabel   *label = &disk->label;
    struct d_partition *partition;
    Stretch_clp bufstr; 
    Stretch_Size bufsz; 
    unsigned char *buf;
    uint32_t blksz;
    int i;

    PTRC(printf("Searching for OSF1 partition table\n"));

    blksz = BlockDev$BlockSize((BlockDev_cl *)disk->device);

    bufstr = STR_NEW(blksz);
    buf    = STR_RANGE(bufstr, &bufsz);
#ifdef CONFIG_MEMSYS_EXPT
    /* need to map the first page of the buffer */
    StretchDriver$Map(Pvs(sdriver), bufstr, buf);
#endif
    bzero(buf, blksz);

    BlockDev$Read((BlockDev_cl *)disk->device, 0, 1, buf);
    bcopy(buf+64, label, sizeof(struct disklabel));

    STR_DESTROY(bufstr);

    if (label->d_magic != DISKLABELMAGIC) {
	PTRC(printf("magic: %08x\n", label->d_magic));
	return False;
    }
    if (label->d_magic2 != DISKLABELMAGIC) {
	PTRC(printf("magic2: %08x\n", label->d_magic2));
	return False;
    }
    printf("Found OSF partition table (MAGIC:%x)\n", 
	   label->d_magic);
    PTRC(printf("d_subtype=%x\n", label->d_subtype));
    PTRC(printf("d_type=%x\n", label->d_type));
    PTRC(printf("d_typename=%s\n", label->d_typename));
    PTRC(printf("d_packname=%s\n", label->d_packname));
    PTRC(printf("d_secsize=%x\n", label->d_secsize));
    PTRC(printf("d_nsectors=%x\n", label->d_nsectors));
    PTRC(printf("d_ntracks=%x\n", label->d_ntracks));
    PTRC(printf("d_ncylinders=%x\n", label->d_ncylinders));
    PTRC(printf("d_secpercyl=%x\n", label->d_secpercyl));
    PTRC(printf("d_secprtunit=%x\n", label->d_secprtunit));
    PTRC(printf("d_sparespertrack=%x\n", label->d_sparespertrack));
    PTRC(printf("d_sparespercyl=%x\n", label->d_sparespercyl));
    PTRC(printf("d_cylskew=%x\n", label->d_cylskew));
    PTRC(printf("d_trackskew=%x\n", label->d_trackskew));
    PTRC(printf("d_interleave=%x\n", label->d_interleave));
    PTRC(printf("d_rpm=%x\n", label->d_rpm));
    PTRC(printf("d_flags=%x\n", label->d_flags));
    PTRC(printf("d_trkseek=%x\n", label->d_trkseek));
    PTRC(printf("d_headswitch=%x\n", label->d_headswitch));
    PTRC(printf("d_npartitions=%x\n", label->d_npartitions));

    partition = label->d_partitions;
    for (i = 0 ; i < label->d_npartitions; i++, partition++) {
	if (partition->p_size) {
	    PTRC(printf("p_size:\t%x\n", partition->p_size));
	    PTRC(printf("p_offset:\t%x\n", partition->p_offset));
	    PTRC(printf("p_fsize:\t%x\n", partition->p_fsize));
	    PTRC(printf("p_fstype:\t%x\n", partition->p_fstype));
	    PTRC(printf("p_frag:\t%x\n", partition->p_frag));
	    PTRC(printf("p_cpg:\t%x\n", partition->p_cpg));
	    PTRC(printf("PARTITION: %8d +%8d type %x (%s)\n", 
		   partition->p_offset,
		   partition->p_size,
		   partition->p_fstype, 
		   osf_fstypenames[partition->p_fstype]));
	}
    }
    PTRC(printf("\n"));
	
    return True;
}




/* ------------------------------------------------------------ */
/* MSDOS (and Linux/ix86) partition tables */


char *msdos_fstypenames(uint8_t ptype)
{
    switch(ptype) {
    case MSDOS_FS_FAT12:
	return "DOS FAT12"; 
	break;

    case MSDOS_FS_FAT16:
	return "DOS FAT16"; 
	break;

    case MSDOS_FS_EXTENDED:
	return "Extended"; 
	break;

    case MSDOS_FS_FAT16B:
	return "DOS FAT16 (>=32MB)"; 
	break;

    case MSDOS_FS_SWAP:
	return "Linux Swap"; 
	break;

    case MSDOS_FS_EXT2:
	return "Linux Native (ext2fs)"; 
	break;

    case MSDOS_FS_AMOEBA:
	return "Amoeba/Nemesis Native";
	break;

    default:
	return "Unknown";
	break;
    }
}


PUBLIC bool_t msdos_partition(disk_t *disk)
{ 
    struct disklabel   *label = &disk->label;
    parttbl_entry *partition;
    partsect *sbuf;
    Stretch_clp sbufstr; 
    Stretch_Size sbufsz; 
    uint32_t blksz;
    int i, n;
    uint32_t nsecs;
    uint32_t startsec;

    PTRC(printf("Searching for PC partition table\n"));

    blksz = BlockDev$BlockSize((BlockDev_cl *)disk->device);

    sbufstr = STR_NEW(blksz);
    sbuf    = STR_RANGE(sbufstr, &sbufsz);
#ifdef CONFIG_MEMSYS_EXPT
    /* need to map the first page of the buffer */
    StretchDriver$Map(Pvs(sdriver), sbufstr, sbuf);
#endif
    bzero(sbuf, blksz);
    bzero(label, sizeof(struct disklabel));

    if (BlockDev$Read((BlockDev_cl *)disk->device, 0, 1, sbuf) == False) {
	printf("USD: read of partition table failed\n");
	STR_DESTROY(sbufstr);
	return False;
    }

    if (sbuf->magic != PARTTBL_MAGIC) {
	PTRC(printf("USD: bad magic (%04x != %04x)\n",
		    sbuf->magic, PARTTBL_MAGIC));
	STR_DESTROY(sbufstr);
	return False;
    }

    printf("Found MSDOS partition table (MAGIC:%04x)\n", sbuf->magic);

    partition = sbuf->ptbl.p;
    for (i = 0 ; i < 4; i++, partition++) {

	if (partition->ostype != 0) {

	    /* AND: I believe these fields to be the only ones
	     * actually used (and furthermore, the p_size field is
	     * only ever printed: I don't think it ever goes near the
	     * extent caches). 
             */

	    n = label->d_npartitions;

	    nsecs = partition->nsecsHI << 16 | partition->nsecsLO;
	    label->d_partitions[n].p_size = nsecs;

	    startsec = partition->startSecHI << 16 | partition->startSecLO;
	    label->d_partitions[n].p_offset = startsec;

	    label->d_partitions[n].p_fstype = partition->ostype;

	    label->d_npartitions++;
	    PTRC(printf("PARTITION: %8d +%8d type %02x (%s)\n", 
		   startsec,
		   nsecs,
		   partition->ostype,
		   msdos_fstypenames(partition->ostype)));
	}
    }

    STR_DESTROY(sbufstr);
    return True;
}
