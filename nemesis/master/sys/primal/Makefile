##########################################################################
#                                                                       #
#  Copyright 1998, University of Cambridge Computer Laboratory      #
#                                                                       #
#  All Rights Reserved.                             #
#                                                                       #
#########################################################################
#
# DIRECTORY:
#
#   sys/primal
#
# DESCRIPTION:
#
#   Primal contains boot-strapping code, run only once. 
#
#
# $Id: Makefile 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $

ROOT = ../..

LDLUMPNOU	= primal

CFILES		= NemesisPrimal.c nexus_util.c addr_init_$(MC).c

ASFILES_alpha   = pcrt0_alpha.S
ASFILES_ix86    = pcrt0_ix86.S
ASFILES_arm     = pcrt0_arm.S

LOCAL_OBJS      = ../../lib/static/debug/libdebug.a ../../lib/static/system/libsystem.a ../../lib/veneer/c/libc.a $(LIBGCCA)

include $(ROOT)/mk/rules.mk
