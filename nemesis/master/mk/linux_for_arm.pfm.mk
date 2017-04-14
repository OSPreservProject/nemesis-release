#########################################################################
#                                                                       #
#  Copyright 1997, University of Glasgow, Computing Science Dept.	#
#                                                                       #
#  All Rights Reserved.						        #
#                                                                       #
#########################################################################
#
# FILE:
#
#	linux_for_arm.pfm.mk
# 
# DESCRIPTION:
#
#	This file contains the definitions needed to build the nemesis
#	tree for an arm coff target on a linux build platform.
#	It is included from rules.mk via the link "platform.mk".
# 
# ID : $Id: linux_for_arm.pfm.mk 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
# 

PLATFORM	= linux_for_arm

CC		= arm-unknown-coff-gcc -fdollars-in-identifiers -msoft-float
CPP		= arm-unknown-coff-gcc -E
AS		= arm-unknown-coff-gcc -x assembler-with-cpp -c
LD		= arm-unknown-coff-ld 
AR		= arm-unknown-coff-ar
RANLIB		= arm-unknown-coff-ranlib
NM		= arm-unknown-coff-nm
STRIP		= arm-unknown-coff-strip

SED		= sed
YACC		= bison
BISON		= bison
FLEX		= flex
AWK		= gawk

MIDDLC		= middlc
MIDDL2TEX	= middl2tex
MIDDL2MANUAL	= middl2manual
CL		= cl
ASTGEN		= astgen
NEMBUILD	= arm-unknown-coff-nembuild

LATEX		= latex2e
DVIPS		= dvips
MAKEINDEX	= makeindex
BIBTEX		= bibtex
TGIF		= tgif

RM		= rm -f
MV		= mv
LN		= ln -s
INSTALL		= install
TOUCH		= touch


# The compiler support libraries for gcc. use ':=' to only run this once here,
# not on every reference.
ifndef LIBGCCA
LIBGCCA		:= $(shell $(CC) -print-libgcc-file-name)
export LIBGCCA
endif

# End
