ROOT	= ..

CFILES	= nmerge.c load_ecoff.c load_exec.c

INSTALL_BINS := nmerge

include $(ROOT)/mk/rules.mk

nmerge: $(OBJS)
	$(CC) $(CFLAGS) -o nmerge $(OBJS) -lmld

