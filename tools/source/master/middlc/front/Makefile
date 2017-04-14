ROOT	= ../..

#
# DO NOT REMOVE strdup FROM THIS LIST! RJB.
#

CFILES	= Args.c Candidates.c Decls.c Errors.c Exceptions.c Fields.c \
          Identifiers.c Interfaces.c Main.c Specs.c ScopedNames.c \
          Types.c SymbolTable.c FileStack.c Grammar.c Lexer.c strdup.c

INSTALL_BINS := middlc_front

include $(ROOT)/mk/rules.mk

Grammar.tab.h: Grammar.c

Grammar.c: Grammar.y
	$(BISON) -v -d Grammar.y
	$(MV) Grammar.tab.c Grammar.c

Lexer.c: Lexer.l
	$(FLEX) -s Lexer.l
	$(MV) lex.yy.c Lexer.c

clean::
	$(RM) Lexer.c Grammar.c Grammar.output Grammar.tab.h middlc_front


middlc_front: $(OBJS)
	$(CC) $(CFLAGS) -o middlc_front $(OBJS)

