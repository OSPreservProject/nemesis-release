ROOT	= ..

CFILES	= build.c misc.c descrip.tab.c

INSTALL_BINS = $(BINARYFORMAT)nembuild

BISONFLAGS = -dv

include $(ROOT)/mk/rules.mk

descrip.tab.o: descrip.tab.c descrip.tab.h descriplex.c descrip.h

descrip.tab.c: descrip.y
	$(YACC) -dv -b descrip descrip.y

descriplex.c: descrip.l
	$(LEX) -n -t descrip.l > descriplex.c || (rm -f descriplex.c; exit 1)

clean::
	$(RM) $(OBJS) descriplex.c descrip.tab.c descrip.tab.h build descrip.output

$(BINARYFORMAT)nembuild: $(OBJS)
	$(CC) $(CFLAGS) -o $(BINARYFORMAT)nembuild $(OBJS) -lbfd -liberty $(LEXLIB)
