/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      hosts.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Sets up the initial hostname <-> IP address mapping.
**
**      XXX SMH: For now, a trivial implementation using a fixed table.
**               Later on, might be able to use something else (yp-like).
** 
** ENVIRONMENT: 
**
**      Network initialisation time
** 
**
*/

#include <nemesis.h>
#include <stdio.h>

#include "flowman_st.h"

#include <Net.ih>

#define TRC(_x) _x
#define TRC_HOSTS(_x)

/* ------------------------------------------------------------ */

#define MAX_LEN 28

typedef struct {
    Net_IPHostAddr ipaddr;
    char hostname[MAX_LEN];
} hostinfo;

/* 
** XXX SMH: the below is a tad little-endian oriented, which is ok for now, 
** but should prob have a macro or sim below. 
*/
static const hostinfo host_table[]= {

    { { 0x80e80072 }, "ivatt.cl.cam.ac.uk"    }, 
    { { 0x80e80073 }, "kirtley.cl.cam.ac.uk"  },
    { { 0x80e80074 }, "maunsell.cl.cam.ac.uk" },
    { { 0x80e8006b }, "fletcher.cl.cam.ac.uk" },
    { { 0x80e8006c }, "fowler.cl.cam.ac.uk"   },
    { { 0x80e8006d }, "gooch.cl.cam.ac.uk"    }, 
    { { 0x80e8006e }, "gresley.cl.cam.ac.uk"  },
    { { 0x80e80411 }, "rope.cl.cam.ac.uk"     },
    { { 0x80e80412 }, "dagger.cl.cam.ac.uk"   },
    { { 0x80e80108 }, "madras.cl.cam.ac.uk"   },
    { { 0x80e80109 }, "ceylon.cl.cam.ac.uk"   },
    { { 0x80e8010A }, "jalfrezi.cl.cam.ac.uk" },
    { { 0x80e8010B }, "balti.cl.cam.ac.uk"    },
    { { 0x80e8010C }, "sosias.cl.cam.ac.uk"   },
    { { 0x80e8010D }, "xanthias.cl.cam.ac.uk" },
    { { 0x80e8011A }, "lelaps.cl.cam.ac.uk"   },
    { { 0x80e8011B }, "nape.cl.cam.ac.uk"     },
    { { 0x80e80122 }, "dorceus.cl.cam.ac.uk"  },
    { { 0x80e80123 }, "lachne.cl.cam.ac.uk"   },
    { { 0x80e80124 }, "sticte.cl.cam.ac.uk"   },
    { { 0x80e80125 }, "argos.cl.cam.ac.uk"    },
    { { 0x80e80126 }, "maera.cl.cam.ac.uk"    },
    { { 0x80e80127 }, "twente.cl.cam.ac.uk"   },
    { { 0x80e80128 }, "agre.cl.cam.ac.uk"     },
    { { 0x80e80129 }, "leucon.cl.cam.ac.uk"   },
    { { 0x80e80150 }, "ely.cl.cam.ac.uk"      },
    { { 0x80e80156 }, "nene.cl.cam.ac.uk"     },
    { { 0x80e80157 }, "ouse.cl.cam.ac.uk"     },
    { { 0x80e80159 }, "styx.cl.cam.ac.uk"     },
    { { 0xE0F }, "" }

};

#define DOMAIN_NAME "cl.cam.ac.uk"


/*
** XXX The below is dead simple and horrible at the moment, but is a placeholder
** for when we can do some proper querying of host tables from somewhere.
*/
#if 0
void hosts_init(FlowMan_st *st, const char *intfname, Context_clp netcx)
{
    Context_clp hostcx;
    int i;

    TRC(printf("hosts_init: entered -- interface is %s\n", intfname));

    TRC(printf("hosts_init: adding domain name (i.e. %s)\n", DOMAIN_NAME));
    CX_ADD_IN_CX(netcx, "domainname", DOMAIN_NAME, string_t);

    hostcx= CX_NEW_IN_CX(netcx, "hosts");
    TRC(printf("hosts_init: created 'sys>net>hosts' context (at %p)\n", hostcx));

    for(i=0; host_table[i].ipaddr.a != 0xE0F; i++) {
	TRC_HOSTS(printf("hosts_init: IP Address %d.%d.%d.%d -- host = %s\n", 
		((host_table[i].ipaddr.a & 0xff000000) >> 24),
		((host_table[i].ipaddr.a & 0x00ff0000) >> 16),
		((host_table[i].ipaddr.a & 0x0000ff00) >>  8),
		(host_table[i].ipaddr.a & 0x000000ff),
		  host_table[i].hostname));
	CX_ADD_IN_CX(hostcx, host_table[i].hostname, 
		     &(host_table[i].ipaddr), Net_IPHostAddrP);
    }

    TRC(printf("hosts_init: done.\n"));
    return;
}
#endif /* 0 */
