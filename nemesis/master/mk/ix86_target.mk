#########################################################################
#                                                                       #
#  Copyright 1994, University of Cambridge Computer Laboratory  	#
#                                                                       #
#  All Rights Reserved.						        #
#                                                                       #
#########################################################################
#
# FILE:
#
#	ix86_target.mk
# 
# DESCRIPTION:
#
# 	This file contains the definitions needed to build the nemesis
# 	tree for an ix86 architecture target machine.  It is included
#	from from target.mk (a link to a intel.tgt.mk file).
#  
# ID : $Id: ix86_target.mk 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
# 

ARCH		= ix86

#
# gawk to render nm output in standard (.sym) form:

NMFILT		= '{ if ($$3 !~ /^\$$C/ && $$1 !~ /^U/) print }'

#
# filter to remove undefined symbols which are 'ok' from module check.

SYMFILT		= cat

#
# Target-architecture-specific compiler options:
#
TGT_CFLAGS	= 
TGT_DEF		= -D__IX86__ -D__ix86 -DKDEBUG -DDEBUG_MONITOR
TGT_INC		= 

TGT_LDFLAGS	= -r -dc

ARCH_CONFIG_DAT = ix86.dat
