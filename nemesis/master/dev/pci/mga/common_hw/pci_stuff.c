/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.		                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      dev/pci/mga/common_hw/pci_stuff.c
**
** FUNCTIONAL DESCRIPTION:
** 
**      Random PCI things - should be moved 
** 
** ENVIRONMENT: 
**
**      Privileged domain
**
*/

#include <ntsc.h>
#include <nemesis.h>
#include <PCIBios.ih>

#include "xf86.h"
#include "pci_stuff.h"


extern PCIBios_clp pcibios;

#define TRC(x)

void pcibusWrite(pciTagRec tag, CARD32 reg, CARD32 data)
{
    ENTER_KERNEL_CRITICAL_SECTION();
	    
    PCIBios$WriteConfigDWord(pcibios, BUS(tag.cfg1),
			   DFN(tag.cfg1), reg, data);

    
    LEAVE_KERNEL_CRITICAL_SECTION();

    TRC(printf("PCI WriteConfigDWord bus %x DFN %x reg %x data %x\n",
	   BUS(tag.cfg1),DFN(tag.cfg1), reg, data));
}

CARD32 pcibusRead(pciTagRec tag, CARD32 reg)
{
    CARD32 data;

    ENTER_KERNEL_CRITICAL_SECTION();
	    
    PCIBios$ReadConfigDWord(pcibios, BUS(tag.cfg1),
			   DFN(tag.cfg1), reg, &data);

    
    LEAVE_KERNEL_CRITICAL_SECTION();

    return data;
}
