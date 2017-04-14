ROOT	= ..

CFILES	= udptelnet.c

INSTALL_BINS := udptelnet

include $(ROOT)/mk/rules.mk

clean::
	$(RM) $(OBJS)

udptelnet: $(OBJS)
	$(CC) $(CFLAGS) -o udptelnet $(OBJS)
