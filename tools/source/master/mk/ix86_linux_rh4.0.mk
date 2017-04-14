#
# This file defines the commands needed to build a nemesis
# TOOLS tree on a ix86_linux_rh4.0.mk platform.
#
# (i.e. linux running RedHat release 4.0)
#
# It is included by rules.mk
#

ARCH		= ix86_linux_rh4.0

CC		= gcc -Wall -g
CPP		= gcc -E
AS		= as
LD		= ld
AR		= ar rv
RANLIB		= ranlib
INSTALL		= install

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

