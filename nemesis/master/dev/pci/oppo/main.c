/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      main.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Otto Driver for DEC 3000/400 series and up. 
**      See section 9.5.2 of DEC3000 H/W Spec Manual
**
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: main.c 1.1 Thu, 18 Feb 1999 15:09:21 +0000 dr10009 $
**
*/

#include <nemesis.h>
#include <ntsc.h>
#include <stdio.h>

#include <IDCMacros.h>

#include <Interrupt.ih>

#include <pci.h>
#include <irq.h>
#include <io.h>

#include <AALPod.ih>
#include <DirectOppo.ih>

/*
** PCI Bus defines
*/
#define PCI_MAX_BUS_NUM      8

#define TRC(X)

uchar_t danger;

typedef struct _Otto_st Otto_st;

/*----------------------------------------------------- Entry Point ---*/

void
Main (Closure_clp self)
{
    AALPod_clp aalpod;
    DirectOppo_clp direct;
    extern AALPod_cl *otto_aal_init (Otto_st *otto_st);
    extern DirectOppo_cl *otto_direct_init (Otto_st *otto_st, AALPod_clp aal);
    Otto_st     *otto_st; 
    extern Otto_st    *otto_driver_init ();
    
    danger = 0;

    TRC(printf ("oppo: domain entered\n"));
    /* 
     * Initialise the OTTO 
     */
    TRC(printf("oppo: Init Driver...\n"));
    if (!(otto_st = otto_driver_init()))
	/* no otto found */
	return;
    
    TRC(printf("oppo: Driver Done\n"));
    
    /* 
    ** Initialise the AAL5 interface 
    ** 
    ** From otto_aal5.c:
    **
    **      Very simple AAL5 interface to Otto Driver.
    **
    **      XXX WARNING WARNING WARNING WARNING WARNING WARNING WARNING 
    **
    **      Note that this whole interface is currently VERY broken.
    **      The OTTO had to be beaten around the head to convince it 
    **      to RX AAL5 PDUs on different receive rings.  Currently there
    **      are some fairly tricky hardware resource limits.  
    **    
    **      Note that to reduce per-packet overheads the receive buffer
    **      rings on each VCI must be kept permanently stocked with
    **      receive buffer descriptors.  No full buffers will be sent to
    **      the client unless an empty buffer is available to replace it.
    **      
    **      XXX WARNING WARNING WARNING WARNING WARNING WARNING WARNING 
    */
    
    TRC(printf("oppo: Init AALPod...\n"));
    aalpod = otto_aal_init(otto_st);
    TRC(printf("oppo: AAL Done\n"));

    /* 
     * Export the AAL Interface
     */
    TRC(printf("oppo_aal_init: Exporting AALPod\n"));
    IDC_EXPORT ("dev>atm0", AALPod_clp, aalpod); 
    TRC(printf("Exporting Done\n"));

    TRC(printf("oppo: direct interface\n"));
    direct = otto_direct_init(otto_st, aalpod);
    IDC_EXPORT("dev>directoppo", DirectOppo_clp, direct);
}
