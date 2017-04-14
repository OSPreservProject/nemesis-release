ROOT	= ..

CFILES	= alphabuild.c
INC     = -I.

INSTALL_BINS := alphabuild

include $(ROOT)/mk/rules.mk

alphabuild: $(OBJS)
	$(CC) $(CFLAGS) -g -o alphabuild $(OBJS) -lmld

