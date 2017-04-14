# Makefile -- Makefile for mkfs.simplefs
# Matt Holgate <holgate@dcs.gla.ac.uk>

ROOT    = ..

CFILES  = mkfs.c parseargs.c

INSTALL_BINS := mkfs.simplefs

include $(ROOT)/mk/rules.mk

# CFLAGS += -DDEBUG

mkfs.simplefs: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

clean::
	$(RM) $(OBJS)

