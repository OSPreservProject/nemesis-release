#
# This file defines the commands needed to build a nemesis
# alpha_osf1_3.2 TOOLS tree on a alpha_osf1_3.2 platform.
#
# It is included by rules.mk
#

ARCH             = alpha_osf1_3.2

CC               = gcc -Wall -g
CPP              = gcc -E
AS               = as
LD               = ld
AR               = ar rv
RANLIB           = ranlib
INSTALL          = installbsd -g pegasus

SED              = sed
YACC             = bison
BISON            = bison
# This is UGLY
FLEX             = flex
LEX		 = lex
LEXLIB		 = -ll

LATEX            = latex2e
DVIPS            = dvips
MAKEINDEX        = makeindex
BIBTEX           = bibtex

RM               = rm -f
MV               = mv
LN               = ln -s

