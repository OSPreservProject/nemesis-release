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
#	ix86_linux_1.99.4.pfm.mk
# 
# DESCRIPTION:
#
# 	This file contains the definitions needed to build the nemesis
# 	tree on an ix86_linux_1.3.79 build platform.  It is included from
#	rules.mk via the link "platform.mk".
# 
# ID : $Id: ix86_linux.pfm.mk 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
# 

PLATFORM	= ix86_linux_1.99.4

CC		= gcc -fdollars-in-identifiers
CPP		= gcc -E
AS		= gcc -c
LD		= ld
AR		= ar
RANLIB		= ranlib
NM		= nm -Bn

SED		= sed
YACC		= bison
BISON		= bison
FLEX		= flex
AWK		= gawk

MIDDLC		= middlc
MIDDL2TEX	= middl2tex
MIDDL2MANUAL	= middl2manual

CL 		= cl
ASTGEN		= astgen

LATEX		= latex
DVIPS		= dvips
MAKEINDEX	= makeindex
BIBTEX		= bibtex
TGIF		= tgif

RM		= rm -f
MV		= mv
LN		= ln -s
INSTALL		= install
TOUCH		= touch

AS86            = as86 -0 -a
AS386           = as86 -3
LD86            = ld86 -0
OBJDUMP         = objdump
ENCAPS          = encaps

# The compiler support libraries for gcc. use ':=' to only run this once here,
# not on every reference.
ifndef LIBGCCA
LIBGCCA		:= $(shell gcc -print-libgcc-file-name)
export LIBGCCA
endif
