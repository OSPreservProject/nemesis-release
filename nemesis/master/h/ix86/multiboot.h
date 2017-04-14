/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      multiboot.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Interface between bootloader and kernel image
** 
** ENVIRONMENT: 
**
**      Kernel space/boot time
** 
** ID : $Id: multiboot.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#ifndef _multiboot_h_
#define _multiboot_h_

#define MULTIBOOT_IMAGE_MAGIC  0x1badb002
#define MULTIBOOT_LOADER_MAGIC 0x2badb002

#define MULTIBOOT_IMAGEFLAG_MODALIGN 0x00001
#define MULTIBOOT_IMAGEFLAG_MEMINFO  0x00002
#define MULTIBOOT_IMAGEFLAG_ADDRS    0x10000

#define MULTIBOOT_FLAG_MEMORY  0x0001
#define MULTIBOOT_FLAG_BOOTDEV 0x0002
#define MULTIBOOT_FLAG_CMDLINE 0x0004
#define MULTIBOOT_FLAG_MODS    0x0008
#define MULTIBOOT_FLAG_AOUT    0x0010
#define MULTIBOOT_FLAG_ELF     0x0020
#define MULTIBOOT_FLAG_MMAP    0x0040

struct mod_info {
    addr_t   mod_start;
    addr_t   mod_end;
    char *   string;
    uint32_t res0;
};

struct mem_info {
    uint32_t size;
    uint32_t baseaddrlow;
    uint32_t baseaddrhigh;
    uint32_t lengthlow;
    uint32_t lengthhigh;
    uint32_t type;
};

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    char *   cmdline;
    uint32_t mods_count;
    struct mod_info *mods_addr;
    union {
	struct {
	    uint32_t tabsize;
	    uint32_t strsize;
	    addr_t   addr;
	    uint32_t res0;
	} aout;
	struct {
	    uint32_t num;
	    uint32_t size;
	    addr_t   addr;
	    uint32_t shndx;
	} elf;
    } syms;
    uint32_t mmap_length;
    addr_t   mmap_addr;
};


#endif /* _multiboot_h_ */
