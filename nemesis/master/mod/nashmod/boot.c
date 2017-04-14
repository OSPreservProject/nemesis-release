/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      Chain booter
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Boots another Nemesis image
** 
** ENVIRONMENT: 
**
**      User space. Privileged.
** 
** ID : $Id: boot.c 1.2 Thu, 18 Feb 1999 14:18:09 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <ntsc.h>
#include <autoconf/bootloader.h>
#include <stdio.h>
#include <salloc.h>
#include <exceptions.h>
#include <IDCMacros.h>
#include <time.h>
#include <ecs.h>
#include <Netif.ih>

void boot(Rd_clp rd, bool_t verbose, string_t cmdline)
{
#ifdef INTEL
    uint32_t NOCLOBBER length;
    uint32_t start, slen;
    uint8_t * NOCLOBBER buff;
    Stretch_clp stretch;
    
    length=Rd$Length(rd);

    /* Read the header of the file to try to work out what it is */

    buff=Heap$Malloc(Pvs(heap), 512);
    Rd$GetChars(rd, buff, 512);

    if (buff[510]==0x55 && buff[511]==0xaa) {
	if (verbose) printf(
	    "Traditional bootable Nemesis image with %d setup blocks.\n",
	    buff[497]);
	start=512*(buff[497]+1);
    } else if (buff[0]==0xfa && buff[1]==0xfc ) {
	if (verbose) printf("Hmm... looks like a bare Nemesis image.\n");
	start=0;
    } else {
	printf("This does not look like a Nemesis image.\n");
	return;
    }

    Heap$Free(Pvs(heap), buff);

    length-=start;

    /* Get a stretch of the appropriate size */
    stretch = STR_NEW(length);
    buff    = STR_RANGE(stretch, &slen);

    /* Read the image */
    Rd$Seek(rd, start);
#define PKTSIZE 8192
    {
	char *bufptr = buff;
	int n;
	int togo = length;

	while (togo) {
	    n = Rd$GetChars(rd, bufptr, PKTSIZE);
	    togo -= n;
	    bufptr += n;
	    printf("."); fflush(stdout);
	} 
    }
    printf("\n");

    /* now shut down any Ethernet cards, so we don't get anything DMAed
     * over the image while it boots */
    /* XXX AND This is a privileged operation.  And we shouldn't really
     * dive at the Netif interface directly. */
    /* XXX SDE we might be in a UDPnash at the moment, so there should be
     * NO OUTPUT after this point */
    verbose=False;
    {
	Type_Any any;
	Netif_clp netif;
	const char * const downlist[] = {"de4x5-0",
					 "de4x5-1",
					 /* don't need 3c509, since no DMA */
					 "eth0",
					 NULL};
	/* HACK: need to lose the "const", since Context$Get() doesn't
         * have it. */
	char ** NOCLOBBER eth = (char**)downlist;
	char buf[32];

	while(*eth)
	{
	    sprintf(buf, "dev>%s>control", *eth);

	    if (Context$Get(Pvs(root), buf, &any))
	    {
		if (verbose)
		{
		    printf("Shutting down %s... ", *eth);
		    fflush(stdout);
		}

		TRY {
		    netif = IDC_OPEN(buf, Netif_clp);
		} CATCH_ALL {
		    if (verbose)
			printf("failed: caught exception\n");
		    netif = NULL;
		} ENDTRY;

		if (netif && !Netif$Down(netif) && verbose)
		{
		    printf("failed: Netif$Down() error\n");
		    netif = NULL;
		}

		if (netif && verbose)
		    printf("ok\n");
	    }

	    eth++;
	}
    }

    if (verbose)
    {
	printf("Starting image...\n");
	PAUSE(MILLISECS(1000));
    }

    /* Chain to the new image */
    ENTER_KERNEL_CRITICAL_SECTION();
    ntsc_chain(buff, length, cmdline);
    LEAVE_KERNEL_CRITICAL_SECTION();

    printf("Bogosity: the new image was not started\n");
    printf("bogosity: (n) (slang); the degree of unusallness and brokenness of some item or event\n");
#else
    printf("Boot chaining not supported\n");
#endif
}
