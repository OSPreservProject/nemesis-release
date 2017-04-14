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
**      dev/pci/mga/common_hw/pci_stuff.h
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


#define PCI_OPTION_REG                     0x40

typedef union {
    CARD32 cfg1;
    struct {
 CARD8  enable;
 CARD8  forward;
 CARD16 port;
    } cfg2;
    CARD32 tag;
} pciTagRec, *pciTagPtr;


#define pciWriteLong(tag, addr, data) pcibusWrite(tag, addr, data)
#define pciReadLong(tag, addr) pcibusRead(tag, addr)

void pcibusWrite(pciTagRec tag, CARD32 reg, CARD32 data);
CARD32 pcibusRead(pciTagRec tag, CARD32 reg);

#define DFN(tag) (((tag)>>8)&0xff)
#define BUS(tag) (((tag)>>16)&0xff)
