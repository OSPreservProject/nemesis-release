ROOT	= ..

CFILES	= intelbuild.c

INSTALL_BINS := intelbuild

include $(ROOT)/mk/rules.mk

# Another random hack. Why do I have to do this sort of junk?
CC = gcc

clean::
	$(RM) $(OBJS) intelbuild

intelbuild: $(OBJS)
	$(CC) $(CFLAS) -o intelbuild $(OBJS)
