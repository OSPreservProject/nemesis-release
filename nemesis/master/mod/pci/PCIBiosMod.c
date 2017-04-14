/*
*****************************************************************************
**                                                                          *
**  Copyright 1995, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	sys/PCI/PCIBiosMod.c
**
** FUNCTIONAL DESCRIPTION:
**
**	PCI bus and device management functions.
**      Provides methods for locating and configuring PCI devices.
**
** ENVIRONMENT: 
**
**      Called directly by domains with kernel privileges.
**
** ID : $Id: PCIBiosMod.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
**
*/

#include <nemesis.h>

#include <PCIBiosMod.ih>
#include <IDCMacros.h>

#include <io.h>
#include <pci.h>

#include "pci_st.h"
#include "bios32.h"

#ifdef DEBUG
#define TRC(x) x
#else
#define TRC(x)
#endif

#define PCI_MEM_SIZE 16<<10

/*
 * Module stuff
 */
static	PCIBiosMod_New_fn New_m;

static	PCIBiosMod_op	pcimod_ms = {
    New_m
};

static	const PCIBiosMod_cl	pcimod_cl = { &pcimod_ms, NULL };

CL_EXPORT (PCIBiosMod, PCIBiosMod, pcimod_cl);

/*
 * Module stuff
 */
static	PCIBios_FindClass_fn 		PCIBios_FindClass_m;
static	PCIBios_FindDevice_fn 		PCIBios_FindDevice_m;
static	PCIBios_ReadConfigByte_fn 	PCIBios_ReadConfigByte_m;
static	PCIBios_ReadConfigWord_fn 	PCIBios_ReadConfigWord_m;
static	PCIBios_ReadConfigDWord_fn 	PCIBios_ReadConfigDWord_m;
static	PCIBios_WriteConfigByte_fn 	PCIBios_WriteConfigByte_m;
static	PCIBios_WriteConfigWord_fn 	PCIBios_WriteConfigWord_m;
static	PCIBios_WriteConfigDWord_fn 	PCIBios_WriteConfigDWord_m;

static	PCIBios_op	pcibios_ms = {
  PCIBios_FindClass_m,
  PCIBios_FindDevice_m,
  PCIBios_ReadConfigByte_m,
  PCIBios_ReadConfigWord_m,
  PCIBios_ReadConfigDWord_m,
  PCIBios_WriteConfigByte_m,
  PCIBios_WriteConfigWord_m,
  PCIBios_WriteConfigDWord_m
};

typedef struct pci_st PCIBios_st;

extern void map_pci_devices(struct pci_st *st, FramesF_cl *framesF);

/*---------------------------------------------------- Entry Points ----*/

static PCIBios_clp New_m(PCIBiosMod_cl *self, Heap_cl *heap, 
			 FramesF_cl *framesF)
{
  struct pci_st *st;
  unsigned long mem_start, mem_end, mem_address;
  PCIBios_clp   pcibios;
  uint32_t val; 

  pcibios = Heap$Malloc(heap, sizeof(*pcibios) + sizeof(*st)); 
  st = (struct pci_st *)(pcibios + 1);

  pcibios->op = &pcibios_ms;
  pcibios->st = st;

  TRC(eprintf ("PCI: starting: self->st is %p\n", self->st));

  mem_address = mem_start = (unsigned long)Heap$Malloc(heap, PCI_MEM_SIZE); 
  mem_end = mem_start + (PCI_MEM_SIZE);

  TRC(eprintf("PCI: PCI init\n"));
  mem_start = pci_init(st, mem_start, mem_end);
/* It is possible for an Intel machine not to have a PCI BIOS */
#ifdef __IX86__
  if (!st->bios32_entry) {
      /* There's no PCI BIOS on this machine */
      Heap$Free(heap, pcibios);
      Heap$Free(heap, mem_address);
      return 0;
  }
#endif /* __IX86__ */

  /* Now arrange the mapping of stretches over the IO memory */
  map_pci_devices(st, framesF);
  
  TRC(eprintf ("PCI: done\n"));
  
  return pcibios;
}

/*---------------------------------------------------- Entry Points ----*/

static PCIBios_Error 
PCIBios_FindClass_m (
	PCIBios_cl	*self,
	PCIBios_Class	class	/* IN */,
	uint32_t	index	/* IN */
   /* RETURNS */,
	PCIBios_Bus	*bus,
	PCIBios_DevFn	*devfn )
{
  PCIBios_st	*st = (PCIBios_st *) self->st;

  return pcibios_find_class (st, class, index, bus, devfn) & 0x7f;
}

static PCIBios_Error 
PCIBios_FindDevice_m (
	PCIBios_cl	*self,
	PCIBios_VendorID	vendor	/* IN */,
	PCIBios_DeviceID	device	/* IN */,
	uint32_t	index	/* IN */
   /* RETURNS */,
	PCIBios_Bus	*bus,
	PCIBios_DevFn	*devfn )
{
  PCIBios_st	*st = (PCIBios_st *) self->st;

  return pcibios_find_device (st, vendor, device, index, bus, devfn) & 0x7f;
}

static PCIBios_Error 
PCIBios_ReadConfigByte_m (
	PCIBios_cl	*self,
	PCIBios_Bus	bus	/* IN */,
	PCIBios_DevFn	devfn	/* IN */,
	uint8_t	where	/* IN */
   /* RETURNS */,
	uint8_t	*value )
{
  PCIBios_st	*st = (PCIBios_st *) self->st;
  return pcibios_read_config_byte (st, bus, devfn, where, value) & 0x7f;
}

static PCIBios_Error 
PCIBios_ReadConfigWord_m (
	PCIBios_cl	*self,
	PCIBios_Bus	bus	/* IN */,
	PCIBios_DevFn	devfn	/* IN */,
	uint8_t	where	/* IN */
   /* RETURNS */,
	uint16_t	*value )
{
  PCIBios_st	*st = (PCIBios_st *) self->st;
  return pcibios_read_config_word (st, bus, devfn, where, value) & 0x7f;
}

static PCIBios_Error 
PCIBios_ReadConfigDWord_m (
	PCIBios_cl	*self,
	PCIBios_Bus	bus	/* IN */,
	PCIBios_DevFn	devfn	/* IN */,
	uint8_t	where	/* IN */
   /* RETURNS */,
	uint32_t	*value )
{
  PCIBios_st	*st = (PCIBios_st *) self->st;
  return pcibios_read_config_dword (st, bus, devfn, where, value) & 0x7f;
}

static PCIBios_Error 
PCIBios_WriteConfigByte_m (
	PCIBios_cl	*self,
	PCIBios_Bus	bus	/* IN */,
	PCIBios_DevFn	devfn	/* IN */,
	uint8_t	where	/* IN */,
	uint8_t	value	/* IN */ )
{
  PCIBios_st	*st = (PCIBios_st *) self->st;
  return pcibios_write_config_byte (st, bus, devfn, where, value) & 0x7f;
}

static PCIBios_Error 
PCIBios_WriteConfigWord_m (
	PCIBios_cl	*self,
	PCIBios_Bus	bus	/* IN */,
	PCIBios_DevFn	devfn	/* IN */,
	uint8_t	where	/* IN */,
	uint16_t	value	/* IN */ )
{
  PCIBios_st	*st = (PCIBios_st *) self->st;
  return pcibios_write_config_word (st, bus, devfn, where, value) & 0x7f;
}

static PCIBios_Error 
PCIBios_WriteConfigDWord_m (
	PCIBios_cl	*self,
	PCIBios_Bus	bus	/* IN */,
	PCIBios_DevFn	devfn	/* IN */,
	uint8_t	where	/* IN */,
	uint32_t	value	/* IN */ )
{
  PCIBios_st	*st = (PCIBios_st *) self->st;
  return pcibios_write_config_dword (st, bus, devfn, where, value) & 0x7f;
}

/*
 * End  of PciBiosMod implementation
 */


static Closure_Apply_fn Instantiate;

static Closure_op Instantiate_ms = {
    Instantiate
};

static const Closure_cl pcibiosinstantiate_cl = { &Instantiate_ms, NULL };

CL_EXPORT(Closure, PCIBiosInstantiate, pcibiosinstantiate_cl);

void Instantiate(Closure_cl *self) {
    PCIBiosMod_clp pcimod;
    PCIBios_clp pcibios;
    FramesF_clp framesF; 
    
    /* Find the PCIBios allocation module */
    pcimod = NAME_FIND("modules>PCIBiosMod", PCIBiosMod_clp);
    TRC(eprintf( " .. at %p\n", pcimod));
    
    /* Find the FramesF closure */
    framesF = NAME_FIND("sys>FramesF", FramesF_clp);
    
    pcibios = PCIBiosMod$New(pcimod, Pvs(heap), framesF); 
    if (pcibios) {
	TRC(eprintf(" + Created PCIBios at %p\n", pcibios));
	
	/* Export 'PCIBios' interface via IDC */
	TRC(eprintf( " ... adding to namespace.... \n"));
	CX_ADD ("sys>PCIBios", pcibios, PCIBios_clp);
	TRC(eprintf( " ... done.\n"));
    }
    
}
