#########################################################################
#									#
#  Copyright 1996, University of Cambridge Computer Laboratory		#
#									#
#  All Rights Reserved.							#
#									#
#########################################################################
#
# DIRECTORY:
#
#	./boot/images/intel
# 
# DESCRIPTION:
#
#	
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
#

ROOT	= ../../..

NEMESIS		= nemesis
IMAGE           = image

NEMBUILD	= nembuild -b -p intel
GENSYM		= ../gensym
GENSRC		= ../gensrc

BUILD		= intelbuild
FIXUP		= multiboot-fixup
OBJCOPY         = objcopy -O binary -R .note -R .comment -S
GZIP            = gzip -9

SVGA_MODE       = -DSVGA_MODE=-1

include $(ROOT)/mk/rules.mk
include $(ROOT)/mk/autoconf.mk
#=========================================================================

# Now we have four choices of image:
#
# image      A Multiboot-compatible uncompressed image that can be loaded
#            by the Nemesis-based bootloader
# image.gz   A compressed Multiboot-compatible image
# zimage     A compressed, Linux-compatible image that looks like a
#	     standard Linux zImage to bootloaders
# bzimage    A compressed, Linux-compatible image that looks like a
#	     bzImage to bootloaders

all::	$(IMAGE)

$(IMAGE): $(NEMESIS).nem Makefile
	rm -f $(IMAGE)
	cp $(NEMESIS).nem $(IMAGE)
	$(FIXUP) $(IMAGE) || (rm -f $(IMAGE); exit 1)

$(IMAGE).gz: $(NEMESIS).nem Makefile
	rm -f $(IMAGE).gz
	$(GZIP) -c $(IMAGE) >$(IMAGE).gz || (rm -f $(IMAGE).gz ; exit 1)

z$(IMAGE): $(NEMESIS).zi.out bootsect setup Makefile
	rm -f z$(IMAGE)
	$(BUILD) bootsect setup $(NEMESIS).zi.out > z$(IMAGE) || \
		(rm -f z$(IMAGE); exit 1)

bz$(IMAGE): $(NEMESIS).bzi.out bbootsect bsetup Makefile
	rm -f bz$(IMAGE)
	$(BUILD) -b bbootsect bsetup $(NEMESIS).bzi.out >bz$(IMAGE) || \
		(rm -f bz$(IMAGE); exit 1)

clean::
	$(RM) *.run $(NEMESIS).nem $(NEMESIS).src autogen_* .symbols*
	$(RM) $(IMAGE) $(IMAGE).gz z$(IMAGE) bz$(IMAGE)

#-------------------------------------------------------------------------
# Compression rules for Linux-style images

ZLDFLAGS = -e startup_32

#
# ZIMAGE_OFFSET is the load offset of the compression loader
# BZIMAGE_OFFSET is the load offset of the high loaded compression loader
#
ZIMAGE_OFFSET = 0x1000
BZIMAGE_OFFSET = 0x100000

ZLINKFLAGS = -Ttext $(ZIMAGE_OFFSET) $(ZLDFLAGS)
BZLINKFLAGS = -Ttext $(BZIMAGE_OFFSET) $(ZLDFLAGS)

$(NEMESIS).zi: piggy.o head.o misc.o
	$(LD) $(ZLINKFLAGS) -o $(NEMESIS).zi head.o misc.o piggy.o

$(NEMESIS).bzi: piggy.o head.o misc.o
	$(LD) $(BZLINKFLAGS) -o $(NEMESIS).bzi head.o misc.o piggy.o

$(NEMESIS).zi.out: $(NEMESIS).zi
	$(OBJCOPY) $(NEMESIS).zi $(NEMESIS).zi.out

$(NEMESIS).bzi.out: $(NEMESIS).bzi
	$(OBJCOPY) $(NEMESIS).bzi $(NEMESIS).bzi.out

piggy.o: $(NEMESIS).nem
	tmppiggy=_tmp_$$$$piggy; \
	rm -f $$tmppiggy $$tmppiggy.gz $$tmppiggy.lnk; \
	cp $(NEMESIS).nem $$tmppiggy; \
	gzip -f -9 < $$tmppiggy > $$tmppiggy.gz; \
	echo "SECTIONS { .data : { input_len = .; LONG(input_data_end - input_data) input_data = .; *(.data) input_data_end = .; }}" > $$tmppiggy.lnk; \
	$(LD) -m elf_i386 -r -o piggy.o -b binary $$tmppiggy.gz -b elf32-i386 -T $$tmppiggy.lnk; \
	rm -f $$tmppiggy $$tmppiggy.gz $$tmppiggy.lnk

head.o: head.S
	$(CC) -nostdinc -I../../../h/ix86 -traditional -c head.S

misc.o: misc.c io.h
	$(CC) -O -nostdinc -I../../../h/ix86 -c misc.c

clean::
	$(RM) *.o $(NEMESIS).zi $(NEMESIS).bzi
	$(RM) $(NEMESIS).zi.out $(NEMESIS).bzi.out

#-------------------------------------------------------------------------
# Bootsector rules

bootsect: bootsect.o
	$(LD86) -s -o $@ $<

bootsect.o: bootsect.s
	$(AS86) -o $@ $<

bootsect.s: bootsect.S Makefile
	$(CPP) -traditional -I../../../h/ix86 $(SVGA_MODE) $(RAMDISK) $< -o $@

bbootsect: bbootsect.o
	$(LD86) -s -o $@ $<

bbootsect.o: bbootsect.s
	$(AS86) -o $@ $<

bbootsect.s: bootsect.S Makefile $(BOOT_INCL)
	$(CPP) -D__BIG_KERNEL__ -traditional -I../../../h/ix86 \
		$(SVGA_MODE) $(RAMDISK) $< -o $@

clean::
	$(RM) bootsect bootsect.o bootsect.s bbootsect bbootsect.o bbootsect.s

#-------------------------------------------------------------------------
# Setup code rules

setup: setup.o
	$(LD86) -s -o $@ $<

setup.o: setup.s
	$(AS86) -o $@ $<

setup.s: setup.S video.S Makefile
	$(CPP) -nostdinc -traditional -I../../../h/ix86 \
		-DCONFIG_VIDEO_SELECT $(SVGA_MODE) $(RAMDISK) $< -o $@

bsetup: bsetup.o
	$(LD86) -s -o $@ $<

bsetup.o: bsetup.s
	$(AS86) -o $@ $<

bsetup.s: setup.S video.S Makefile
	$(CPP) -nostdinc -D__BIG_KERNEL__ -traditional -I../../../h/ix86 \
		-DCONFIG_VIDEO_SELECT $(SVGA_MODE) $(RAMDISK) $< -o $@

clean::
	$(RM) setup setup.o setup.s bsetup bsetup.o bsetup.s

#=========================================================================

gdb:	$(NEMESIS).sym $(NEMESIS).src

ifeq ($(CONFIG_ROOTFS_NFS),y)
# pass
else
all:: zimage
endif

# End.
