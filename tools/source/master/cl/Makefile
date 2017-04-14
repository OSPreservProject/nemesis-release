######################################################################
# CLUless compiler front end
######################################################################
# Copyright 1997 University of Cambridge Computer Laboratory
# All Rights Reserved
# Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)
#          Richard Mortier (rmm1002@cam.ac.uk)
######################################################################
# CLUless
#
# Dickon Reed, 1996
# Richard Mortier, 1997
#
# $Id: Makefile 1.2 Thu, 18 Feb 1999 15:03:33 +0000 dr10009 $
#
# $Log: Makefile,v $
# Revision 1.7  1998/04/28 14:36:23  dr10009
# sort out for libc6
#
# Revision 1.6  1997/10/15 16:49:45  dr10009
# *** empty log message ***
#
# Revision 1.5  1997/09/12 13:41:07  rjb17
# use flexlib
#
# Revision 1.4  1997/08/01 16:44:55  rjb17
# another fix
#
# Revision 1.3  1997/08/01 16:13:35  rjb17
# fixed
#
# Revision 1.2  1997/08/01 16:12:02  dr10009
# *** empty log message ***
#
# Revision 1.1  1997/06/25 17:06:44  dr10009
# Initial revision
#
# Revision 2.4  1997/04/10 22:58:49  rmm1002
# Fixed TABLE makes.
# ,
#
# Revision 2.3  1997/04/09 14:18:42  rmm1002
# Fixed make always doing a re-link, regardless of necessity
#
# Revision 2.2  1997/04/07 20:49:22  rmm1002
# Quite major re-write; more or less by accident...
# Basically much layout change; no different functionality (I think)
#
# Revision 2.1  1997/04/04 15:34:04  rmm1002
# Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
#
# Revision 1.12  1997/04/04 15:30:17  rmm1002
# Now makes on Alphas as well (use $* instead of GNU extension with %)
#
# Revision 1.11  1997/02/26 00:08:13  rmm1002
# Includes fp_reduce
#
# Revision 1.10  1997/02/04 16:48:14  rmm1002
# Finished first look and layout fixup
#
# Revision 1.5  1997/02/04 14:26:24  rmm1002
# Changed call of gentables to include current dir (in case not on path)
#
# Revision 1.4  1997/02/03 21:54:17  rmm1002
# Added grammar option
#
# Revision 1.3  1997/02/03 20:14:34  rmm1002
# Finally cleaned up & generates from .tab files
#
# Revision 1.2  1997/02/03 18:31:16  rmm1002
# More cleaning up
#
# Revision 1.1  1997/01/30 21:17:03  rmm1002
# Initial revision
#

ROOT = ../

HEADERS = cluless.h dpython.h dtree.h parsupport.h stackobj.h tables.h \
          typesupport.h
OBJECTS = cl.tab.o dpdecl.o dpexpr.o dpython.o dpspec.o dptype.o \
          dtree.o lex.yy.o parsupport.o symtab.o typesupport.o dast.o md5.o brokenprintf.o
TABLES = decl.tab expr.tab group.tab list.tab output.tab spec.tab \
         statement.tab type.tab
TABLE_H = $(TABLES:.tab=.h)
TABLE_C = $(TABLES:.tab=.c)
TABLE_O = $(TABLES:.tab=.o)
TABLE_PY = $(TABLES:.tab=.py)



#
# Main targets
#
all:: tables grammar cl mastertable.c


%.o:	%.c
	$(CC) -g -o $@ -c $< 

cl: $(OBJECTS) $(TABLE_O) cluless.o
	$(CC) -o cl $(OBJECTS) cluless.o $(TABLE_O) $(LEXLIB)

genlist: $(OBJECTS) $(TABLE_O) genlist.o
	$(CC) -o genlist genlist.o $(TABLE_O)

mastertable.c: genlist
	./genlist > mastertable.c

clean:: 
	$(RM) cl.output cl.tab.[ch] cl lex.*.c fp_reduce $(TABLE_H) \
            $(TABLE_C) *~ $(TABLE_PY)

.PHONY : grammar
grammar: cl.tab.o lex.yy.o

.PHONY : tables
tables: $(TABLE_C) $(TABLE_H)

fp_reduce: fp_reduce.c
	$(CC) -o fp_reduce fp_reduce.c

#
# Subsidiary targets -- tables, lexical analyser, grammar
#
%.c %.h %.py: %.tab
	python gentables.py $<

cl.tab.o: cl.tab.c
cl.tab.c: cl.y $(TABLE_H)
	$(BISON) -v -d cl.y

lex.yy.o: lex.yy.c
lex.yy.c: cl.l cl.tab.h $(TABLE_H)
	$(FLEX) cl.l
#
# Tree output facility
#
#lex.tree.o: lex.tree.c
#lex.tree.c: tree.l 
#	$(FLEX) -Ptree tree.l

INSTALL_BINS := cl

include $(ROOT)/mk/rules.mk

