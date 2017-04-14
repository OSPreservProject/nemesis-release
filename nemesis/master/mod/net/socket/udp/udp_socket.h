/*
** udp_socket.h; prototypes, etc, for the udp-specific socket code.
*/

/*
** XXX SMH: this is far too full by far. bad news. clean it up later. 
*/
struct udp_data {
    IO_cl          *txio;   /* place to send the data on this socket */
    IO_cl          *rxio;   /* place to recv the data from this socket */
    Heap_cl        *rxheap; /* per-socket rx heap. oh yeah. */
    Heap_cl        *txheap; /* per-socket tx heap. oh yeah. */
    Ether_cl       *eth;    /* XXX an ethernet module (bit specific or what) */
    IP_cl          *ip;     /* XXX an IP module; this should all be encaps'd */
    UDP_cl         *udp;    /* XXX a UDP module; as above, should be hidden  */
    uint32_t        head;   /* sizes of headers etc in this stack */
    uint32_t        mtu;
    uint32_t        tail;

    /* (concurrent tx/rx on a socket is a bad idea: no locking done) */
    char          *rxbuf;   /* buffer to recv into */
    char          *txbuf;   /* buffer to build tx headers in */

    Net_EtherAddr  dst_hw;  /* 6-bytes of destination hw address (urk!) */
    Net_EtherAddr  src_hw;  /* 6-bytes of source hw address */

    FlowMan_clp	   fman;    /* cached binding to FlowMan */
    FlowMan_Flow   flow;    /* FlowMan_Flow */
    char	  *curif;   /* which interface we're currently using */
    pktcx_t	  *pktcx;
};

extern const proto_ops udp_ops;
