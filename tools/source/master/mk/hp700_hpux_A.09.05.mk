#
# This file defines the commands needed to build a nemesis
# TOOLS tree on an hp700_hpux_A.09.05 platform.
#
# (i.e. a snake running HP-UX9)
#
# It is included by rules.mk
#

ARCH		= hp700_hpux_A.09.05

CC		= gcc -Wall
CPP		= gcc -E
AS		= as
LD		= ld
AR		= ar rv
RANLIB		= ranlib
INSTALL		= bsdinst

SED		= sed
YACC		= bison
BISON		= bison
# This is ugly.
FLEX		= flex
LEX		= flex -l
LEXLIB		= -lfl

LATEX		= latex2e
DVIPS		= dvips
MAKEINDEX	= makeindex
BIBTEX		= bibtex

RM		= rm -f
MV		= mv
LN		= ln -s

