#########################################################################
#                                                                       #
#  Copyright 1997, University of Cambridge Computer Laboratory          #
#                                                                       #
#  All Rights Reserved.                                                 #
#                                                                       #
#########################################################################
#
# DIRECTORY:
#
#       ./mk/config.mk
#
# DESCRIPTION:
#
#	Tailor an individual user's build tree.
#
# ID : $Id: config.mk 1.2 Mon, 17 May 1999 18:37:22 +0100 dr10009 $
#

#
# Master configuration variables.  Eventually do this with 
# a menu-driven configuration script

#
# These are passed to EVERYTHING (on the command line, so keep it short)
GLOBAL_DEFINES := 
GLOBAL_CFLAGS :=
# remove this next line if you don't want support for postmortem debugging
GLOBAL_CFLAGS += -g
GLOBAL_ASFLAGS :=

# Directory where 'gnumake install' will copy files
ifdef NEMBOX 
INSTDIR := /usr/groups/pegasus/boot/${USER}/${NEMBOX}
endif

-include $(ROOT)/mk/autoconf.mk

# SUBDIRS_$(CURRENT_DIR)
# Extra subdirectories to build.
#SUBDIRS_. := 
#SUBDIRS_sys := 
#SUBDIRS_app := 
#SUBDIRS_app/test := 

# EXCLUDE_$(CURRENT_DIR)
# Prevent these subdirectories being made

# DEFINES_$(CURRENT_DIR)
# Per-directory definitions on the command line
#DEFINES_mod/venimpl/c/stdio += -DPRINTF_DOMAIN_IDS
#DEFINES_mod/net/flowman += -DBOOTP_TRACE -DFLOWMAN_TRACE -DIPCONF_DEBUG -DKILL_TRACE

# CFLAGS_$(CURRENT_DIR)
# Per-directory C compiler flags 
#CFLAGS_sys/Domains := -g
#CFLAGS_ntsc/alpha/eb164 := -g
CFLAGS_mod/clanger          := -Werror
CFLAGS_mod/fs/buildfs       := -Werror
CFLAGS_mod/fs/ramdisk       := -Werror
CFLAGS_mod/fs/usd           := -Werror
#CFLAGS_mod/fs/ext2fs        := -Werror
CFLAGS_mod/fs/util          := -Werror
CFLAGS_mod/net/flowman      := -Werror
CFLAGS_mod/net/protos/ether := -Werror
CFLAGS_mod/net/protos/ip    := -Werror
CFLAGS_mod/net/protos/udp   := -Werror
CFLAGS_mod/net/arp          := -Werror
CFLAGS_mod/net/lmpf         := -Werror
CFLAGS_mod/net/socket/udp   := -Werror
CFLAGS_mod/nemesis/builder  := -Werror
CFLAGS_mod/nemesis/qosentry := -Werror
CFLAGS_app/nash             := -Werror
CFLAGS_app/test/qosentry    := -Werror
CFLAGS_lib/static/system    := -Werror


# ASFLAGS_$(CURRENT_DIR)
# Per-directory assembler flags 


# things missing from the config system
#CONFIG_OPPO=y
#CONFIG_NBF_OPPO=y

#CONFIG_SOUND=y
#CONFIG_CALLPRIV=y
#CONFIG_NBF_KBD=y
#CONFIG_NBF_NET=y
#CONFIG_NBF_FB=y
#CONFIG_NBF_WS=y
#CONFIG_NBF_CLANGER=y
#CONFIG_KBD=y

