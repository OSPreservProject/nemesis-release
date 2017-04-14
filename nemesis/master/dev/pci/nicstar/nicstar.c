/* nicstar.c, M. Welsh (matt.welsh@cl.cam.ac.uk)
 *
 * Linux driver for the IDT77201 NICStAR PCI ATM controller.
 * PHY component is expected to be 155 Mbps S/UNI-Lite or IDT 77155;
 * see init_nicstar() for PHY initialization to change this. This driver
 * expects the Linux ATM stack to support scatter-gather lists 
 * (skb->atm.iovcnt != 0) for Rx skb's passed to vcc->push.
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * M. Welsh, 6 July 1996
 *
 */


#include "nicstar.h"

#define PRINTK(args...) printk(args)

#define CMD_BUSY(st) (readl((st)->membase + STAT) & 0x0200)

/* Global/static function declarations *************************************/
static int init_nicstar(Nicstar_st *st);
static void nicstar_interrupt(Nicstar_st* st);
static bool_t process_tx_statq(Nicstar_st* st, bool_t wait);
static bool_t process_rx_statq(Nicstar_st* st);
static void push_rxbufs(Nicstar_st* st, int lg, 
			uint32_t handle1, uint32_t addr1, uint32_t handle2, 
			uint32_t addr2);
void close_rx_connection(Nicstar_st* st, int index);
static void push_scq_entry(Nicstar_st* st, nicstar_tbd *entry,
			   int xmit_now);
void Main(Closure_clp self);

static void get_ci(Nicstar_st* st, AALPod_Dir mode,
		   short *vpi, int *vci);
static void dequeue_rx(Nicstar_st* st);
static void drain_scq(Nicstar_st* st, int index);
  
/* Module entry points *****************************************************/

const char nibble_table[] = { '0', '1', '2', '3', '4', '5', '6', '7', 
			'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
void print_data(char* addr, int len)
{
    int  bpos = 0;
    char buffer[49];
    return;

    while (len --)
    {
	buffer[bpos] = nibble_table[((*addr) >> 4) & 0xf];
	buffer[bpos+1] = nibble_table[(*addr) & 0xf];
	bpos += 2;
	addr ++;

	if((bpos == 48 ) || (len == 0))
	{
	    buffer[bpos] = 0;
	    PRINT_DATA("%s\n", buffer);
	    bpos = 0;
	}
    }
}

void nicstar_open(Nicstar_st* st, AALPod_Dir mode,short vpi, int vci) {

  get_ci(st, mode, &vpi, &vci);

}

/* XXX It is possible here that when pushing multiple TBD's, the 
 * next TSI will be placed between them, causing an incomplete PDU xmit.
 * Probably fix by not allowing a TSI to be pushed onto the SCQ when
 * force_xmit == 0.
 */
int nicstar_send_aal5(Nicstar_st* st, int vci, struct rbuf* r, bool_t raw) {

    int         sindex;
    nicstar_tbd tbd;
    
    int len;  /* data length rounded up to next word */

    int pdu_len; /* length including control/CRC */
    
    int ncells; /* number of 48 byte cells */
    int cell_len; /* total length rounded up to next cell */
    int pad;

    int rate = TBD_UBR_M | TBD_UBR_N;
    uint16_t aal5_len;
		   
    if (!st->vci_map[vci].tx) {
	PRINT_TX("Tx on a non-Tx VC?\n");
	PRINT_TX("Must dispose of erroneous IO_Recs\n");
	return -EINVAL;
    }

    if (r->r_nrecs>1) {
	PRINT_TX("S/G not supported, sorry.\n");
	PRINT_TX("Must dispose of erroneous rbuf\n");

	return -EINVAL;
    }
    
#ifdef NICSTAR_STATS
    *((uint32_t*) r->r_recs->base) = st->tx_count;
#endif

    st->tx_count++;

    if(!(st->tx_count % 16384)) {
	PRINT_STATS("Queued %d PDUs\n", st->tx_count);
    }

    sindex = st->scq_next - st->scq;

    if(raw) {
	aal5_len = *((uint16_t*) (r->r_recs->base + r->r_len - 6));
	aal5_len = ((aal5_len & 0xff) << 8) | ((aal5_len & 0xff00) >> 8);
	
	PRINT_TX("AAL5 len = %d\n", aal5_len);
	
	tbd.buf_addr = (uint32_t) r->r_recs->base;
	tbd.ctl_len = aal5_len;
	tbd.cell_hdr = (vci << 4);
	tbd.status = TBD_AAL5 | TBD_ENDPDU | rate | r->r_len;
	st->scq_shadow[sindex].rbuf = r;

	push_scq_entry (st, &tbd, 1);
    } else {

	/* Round up to 4-byte boundary */
	len = (((r->r_len + 3) / 4) * 4);
	
	/* Accommodate the length/control field and CRC */
	pdu_len = len + 8;
	
	ncells = (pdu_len + 47) / 48;
	cell_len = (ncells * 48);
	    
	pad = cell_len - len;
	PRINT_TX ("new len %d rbuf len %ld\n",cell_len,r->r_len);
	
	tbd.status = TBD_AAL5 | rate | len;
	if (pad < 48) {
	    /* The 8 byte control field fits in the same cell as the end
	       of this buffer - so this TBD contributes to the last cell,
	       and must have the TBD_ENDPDU flag set */
	    
	    tbd.status |= TBD_ENDPDU;
	}
	
	
	tbd.buf_addr = (uint32_t)r->r_recs->base;
	tbd.ctl_len = r->r_len;
	tbd.cell_hdr = (vci << 4);
	st->scq_shadow[sindex].rbuf = r;
	
	PRINT_TX("TBD status (main cell) : 0x%lx\n", tbd.status); 
	
	PRINT_TX("Main buffer : ctl_len = %d, len = %d\n", r->r_len, len);
	
	push_scq_entry(st, &tbd, (pad == 8));
	
	/* For some reason, if the pad is 8 (i.e. just ctl/len and CRC)
	   the nicstar doesn't like it if we add in a pad tbd, as you
	   should do according to the manual) */
	
	if (pad != 8) {
	    
	    tbd.status = TBD_AAL5 | TBD_ENDPDU | rate | pad;
	    tbd.buf_addr = (uint32_t) st->tmp_cell;
	    tbd.ctl_len = r->r_len;
	    tbd.cell_hdr = (vci << 4);
	    
	    PRINT_TX("Temp cell: TBD len %d, ctl_len %d, status 0x%lx\n", 
		     pad, r->r_len, tbd.status);
	    push_scq_entry(st, &tbd, 1);
	}
	    
    }	    
    return 0;

}

int nicstar_send_aal0(Nicstar_st * st, int vci, struct rbuf* r) {

    int sindex;
    nicstar_tbd tbd;

    int rate = TBD_UBR_M | TBD_UBR_N;

    if (!st->vci_map[vci].tx) {
	PRINT_TX("Tx on a non-Tx VC?\n");
	PRINT_TX("Must dispose of erroneous IO_Recs\n");
	return -EINVAL;
    }

    if (r->r_nrecs>1) {
	PRINT_TX("S/G not supported, sorry.\n");
	PRINT_TX("Must dispose of erroneous rbuf\n");

	return -EINVAL;
    }

    if(r->r_len != 48) {

	PRINT_ERR("Attempt to send cell of size %d on AAL0 channel\n", r->r_len);
	
	return -EINVAL;
	
    }

#ifdef NICSTAR_STATS
    *((uint32_t*) r->r_recs->base) = st->tx_count;
#endif

    st->tx_count++;

    if(!(st->tx_count % 16384)) {
	PRINT_STATS("Queued %d PDUs\n", st->tx_count);
    }

    sindex = st->scq_next - st->scq;

    tbd.buf_addr = (uint32_t) r->r_recs->base;
    tbd.status = TBD_AAL0 | TBD_ENDPDU | rate | 48;
    tbd.ctl_len = 0;
    tbd.cell_hdr = (vci<<4) | (r->r_value & 15);

    st->scq_shadow[sindex].rbuf = r;

    push_scq_entry(st, &tbd, 1);

    return 0;

}

int nicstar_send_aal34(Nicstar_st * st, int vci, struct rbuf* r) {

    int sindex;
    nicstar_tbd tbd;

    int rate = TBD_UBR_M | TBD_UBR_N;

    if (!st->vci_map[vci].tx) {
	PRINT_TX("Tx on a non-Tx VC?\n");
	PRINT_TX("Must dispose of erroneous IO_Recs\n");
	return -EINVAL;
    }

    if (r->r_nrecs>1) {
	PRINT_TX("S/G not supported, sorry.\n");
	PRINT_TX("Must dispose of erroneous rbuf\n");

	return -EINVAL;
    }

#ifdef NICSTAR_STATS
    *((uint32_t*) r->r_recs->base) = st->tx_count;
#endif

    st->tx_count++;

    if(!(st->tx_count % 16384)) {
	PRINT_STATS("Queued %d PDUs\n", st->tx_count);
    }

    sindex = st->scq_next - st->scq;

    tbd.buf_addr = (uint32_t) r->r_recs->base;
    tbd.status = TBD_AAL34 | rate | r->r_len;
    tbd.ctl_len = 0;
    tbd.cell_hdr = (vci<<4);

    st->scq_shadow[sindex].rbuf = r;

    push_scq_entry(st, &tbd, 1);

    return 0;

}

static void NicstarHeartbeatThread(Nicstar_st *st)
{
    while(1) {
	int lg_buf_count, sm_buf_count;
	
	PAUSE(SECONDS(5));
	
	lg_buf_count = (readl(st->membase + STAT) & 0x00ff0000) >> 15;
	lg_buf_count = (readl(st->membase + STAT) & 0x00ff0000) >> 15;
	sm_buf_count = (readl(st->membase + STAT) & 0xff000000) >> 23;
	
	PRINT_NORM("Lub-dub (TXQ:%d TXA:%d RX:%d LBQ:%d SBQ:%d)\n", 
		   st->tx_count, st->tx_ack_count, st->rx_count, 
		   lg_buf_count, sm_buf_count);
    }
}


void Main(Closure_clp self) {

  int                    error;
  int NOCLOBBER          num_found = 0;
  PCIBios_Bus            pci_bus; 
  PCIBios_DevFn          pci_devfn;
  int NOCLOBBER          pci_index;
  Nicstar_st NOCLOBBER  *st = NULL;
  unsigned short         pci_command;
  unsigned char          pci_latency;
  unsigned char          irq;
  int                    i;
  PCIBios_clp NOCLOBBER  pci = NULL;
  Interrupt_clp          ipt;
  Interrupt_AllocCode    rc;
  Domain_ID              dom;
  uint32_t               gp, new_gp;

  char                   err_str[256];

  PRINT_INIT("Initialising\n");
  
  TRY {
    pci = (PCIBios_clp) NAME_FIND("sys>PCIBios", PCIBios_clp);
  } 
  CATCH_Context$NotFound(name) {
    PRINT_INIT("PCIBios not present\n");
    return;
  }
  ENDTRY;
    
  PRINT_INIT("Installing %s of %s %s.\n",__FILE__,__DATE__,__TIME__);
  
  PRINT_INIT("Searching for IDT77201 NICStAR: vendor 0x%x, device 0x%x\n",
	 PCI_VENDOR_ID_IDT,
	 PCI_DEVICE_ID_IDT_IDT77201);

  for (pci_index = 0; pci_index < 8; pci_index++) {

      ENTER_KERNEL_CRITICAL_SECTION();
      error = PCIBios$FindDevice(pci, PCI_VENDOR_ID_IDT,
				 PCI_DEVICE_ID_IDT_IDT77201,
				 pci_index, &pci_bus, &pci_devfn);
      LEAVE_KERNEL_CRITICAL_SECTION();

      if (error != PCIBios_Error_OK)
	  break;

      /* Allocate a device structure */
      st = Heap$Malloc(Pvs(heap), sizeof(Nicstar_st));
      if (st == NULL) {
	  PRINT_INIT("Can't allocate device struct for device %d!\n",
		 pci_index);
	  return;
      }
      else
      {
	  PRINT_INIT("state allocated at %x\n", st);
      }
    
      memset(st, 0, sizeof(Nicstar_st));

      st->index        = num_found;
      st->pci_bus      = pci_bus;
      st->pci_devfn    = pci_devfn;
      st->tx_count     = 0;
      st->tx_ack_count = 0;
      st->rx_count     = 0;
      
      st->allow_client_master = GET_BOOL_ARG("allow_client_master", True);
      st->default_num_buffers = GET_INT_ARG("buffers", DEFAULT_NUM_BUFS);
      st->facistly_disable_slowpokes = GET_BOOL_ARG("facist", False);

      ENTER_KERNEL_CRITICAL_SECTION();

      do {

	  error = PCIBios$ReadConfigByte(pci, pci_bus,pci_devfn,
					 PCI_REVISION_ID,
					 &(st->revid));
	  if (error) {
	      sprintf(err_str, 
		      "Can't read REVID from device %d\n",pci_index);
	      break;
	  }
	  
	  error = PCIBios$ReadConfigDWord(pci, pci_bus,pci_devfn,
					  PCI_BASE_ADDRESS_0,
					  (unsigned int *)&(st->iobase));
	  if (error) {
	      sprintf(err_str,
		      "Can't read iobase from device %d.\n",
		      pci_index);
	      break;
	  }
	  
	  error = PCIBios$ReadConfigDWord(pci, pci_bus,pci_devfn,
					  PCI_BASE_ADDRESS_1,
					  (unsigned int *)&(st->membase));
	  if (error) {
	      sprintf(err_str, 
		      "Can't read membase from device %d.\n",
		      pci_index);
	      break;
	  }
      
	  error = PCIBios$ReadConfigByte(pci, pci_bus,pci_devfn,
					 PCI_INTERRUPT_LINE,
					 (unsigned char *)&irq);
	  if(error) {
	      sprintf(err_str, 
		      "Can't read INTLN from device %d.\n",pci_index);
	      break;
	  }
	  
      } while (0);

      LEAVE_KERNEL_CRITICAL_SECTION();

      if(error)
      {
	  PRINT_ERR(err_str);
	  continue;
      }

      st->irq = irq;
      
      st->iobase &= PCI_BASE_ADDRESS_IO_MASK;
      st->membase &= PCI_BASE_ADDRESS_MEM_MASK;
      
      if ((unsigned long *)st->membase == NULL) {
	  PRINT_ERR("Invalid membase for board %d.\n");
	  continue;
      }

      PRINT_INIT("Found device index %d, bus %d, function %d.\n",
	     pci_index,pci_bus,pci_devfn);
      PRINT_INIT("iobase: 0x%lx, membase: 0x%lx\n",
	     st->iobase,st->membase);
      
      PRINT_INIT("Revision 0x%x, using IRQ %d.\n",st->revid,st->irq);

      /* Check for Nicstar chipset revision */
      
      if (st->revid == 2)
      {
	  ENTER_KERNEL_CRITICAL_SECTION();
	  gp = readl(st->membase + GP);
	  writel(gp | (1<<15), (st->membase) + GP);
	  new_gp = readl(st->membase + GP);
	  writel(gp, st->membase + GP);
	  LEAVE_KERNEL_CRITICAL_SECTION();

	  if(( new_gp & (1<<15)) == (1<<15))
	  {
	      PRINT_INIT("Chipset revision D\n");
	      st->rev_C3 = False;
	  }
	  else
	  {
	      PRINT_ERR("Chipset C3 not supported well - get a rev D!\n");
	      st->rev_C3 = True;
	  }
	  
      } else {
	  st->rev_C3 = False;
      }

      ENTER_KERNEL_CRITICAL_SECTION();

      do {
      
	  /* Enable memory space and busmastering */
	  error = PCIBios$ReadConfigWord(pci, pci_bus,pci_devfn,PCI_COMMAND,
					 &pci_command);
	  if (error) {
	      sprintf(err_str, 
		      "Can't read PCI_COMMAND from device %d.\n",
		      pci_index);
	      break;
	  }

	  pci_command |= 
	      (PCI_COMMAND_MEMORY | PCI_COMMAND_SPECIAL | PCI_COMMAND_MASTER);
	  error = PCIBios$WriteConfigWord(pci, pci_bus,pci_devfn,PCI_COMMAND,
					  pci_command);
	  if (error) {
	      sprintf(err_str,
		      "Can't enable board memory for device %d.\n",
		      pci_index);
	      break;
	  }
      
	  /* Check latency timer - must be at least 32 */
	  error = PCIBios$ReadConfigByte(pci, pci_bus,pci_devfn,
					 PCI_LATENCY_TIMER,
					 &pci_latency);

	  if(error) {
	      sprintf(err_str, 
		      "Can't read latency timer from device %d.\n",
		      pci_index);
	      break;
	  }

	  if (pci_latency < 32) {
	      PCIBios$WriteConfigByte(pci, pci_bus,pci_devfn,
				      PCI_LATENCY_TIMER,32);
	  }
      } while (0);

      LEAVE_KERNEL_CRITICAL_SECTION();

      if(error) {
	  PRINT_ERR(err_str);
	  continue;
      }

      num_found++;
      
      ipt = IDC_OPEN("sys>InterruptOffer", Interrupt_clp);

      /* Interrupt event channels */
      dom = RO(Pvs(vp));
      st->stub.dcb   = dom;
      st->stub.ep    = Events$AllocChannel (Pvs(evs));
      PRINT_INIT("Interrupt channel %lx, %lx\n", st->stub.dcb, st->stub.ep);
      st->ints       = 0;
      st->event      = EC_NEW();
      st->ack        = 1;
      Events$Attach(Pvs(evs), st->event, st->stub.ep, Channel_EPType_RX);

      PRINT_INIT("Allocating interrupt... ");
      rc = Interrupt$Allocate(ipt, st->irq, _generic_stub, &st->stub);
      PRINT_INIT(" ... allocate returns %x\n", rc);
 
      memset(st->rxclient, 0, sizeof(st->rxclient));
      memset(st->txclient, 0, sizeof(st->txclient));

      /* Init the per-client state */
      st->rxclientfreelist = 1;
      for (i = 1; i < NICSTAR_NRXCLIENTS; i++) {
	  st->rxclient[i].next = i+1;
	  st->rxclient[i].io   = NULL;
      }
      st->rxclient[NICSTAR_NRXCLIENTS-1].next = 0;

      st->txclientfreelist = 1;
      for (i = 1; i < NICSTAR_NTXCLIENTS; i++) {
	  st->txclient[i].next = i+1;
	  st->txclient[i].io   = NULL;
	  st->txclient[i].ec   = NULL;
	  st->txclient[i].ev   = 0;
	  memset(st->txclient[i].rb, 0, sizeof(st->txclient[i].rb));
      }
      st->txclient[NICSTAR_NTXCLIENTS-1].next = 0;
      
      /* Fork the interrupt thread */
      PRINT_INIT("Forking InterruptThread... ");
      Threads$Fork(Pvs(thds), InterruptThread_m, st, 0 ); 
      PRINT_INIT("Done\n");

      if (init_nicstar(st)) {
	  PRINT_ERR("Error initializing device %d.\n",pci_index);
	  Interrupt$Free(ipt, st->irq);
	  continue;
      }

      /* Initialise the Nemesis top end and export the interface */

      nicstar_aal_init(st);

      if(GET_BOOL_ARG("show_heartbeat", False)) {
	  Threads$Fork(Pvs(thds), NicstarHeartbeatThread, st, 0);
      }
      
  }
  
  if (num_found == 0) {
      PRINT_INIT("No devices found in system\n");
  } else {
      PRINT_INIT("%d devices found.\n",num_found);
  }
  
  return;
  
}

#if 0
void cleanup_module(void) {
  int i, error;
  Nicstar_st* st;
  unsigned short pci_command;
  
  PRINTK("cleanup_module called\n");

  if (MOD_IN_USE) 
    PRINT_NORM("Device busy, remove delayed.\n");

  /* Free up the device resources */
  for (i = 0; i < NICSTAR_MAX_DEVS; i++) {
    if (nicstar_st[i] != NULL) {
      st = nicstar_st[i];
      /* Turn everything off */
      writel(0x00000000, (st->membase)+CFG);

      /* Disable PCI busmastering */
      if (pcibios_read_config_word(st->pci_bus,st->pci_devfn,PCI_COMMAND,&pci_command)) {
        printk("nicstar: Can't read PCI_COMMAND from device %d.\n");
	continue;
      }
      pci_command &= ~PCI_COMMAND_MASTER;
      error = pcibios_write_config_word(st->pci_bus,st->pci_devfn,
        PCI_COMMAND, pci_command);
      if (error) {
        printk("nicstar: Can't disable busmastering for device %d.\n");
        continue;
      }
      
      kfree(st->rx_statq_orig);
      kfree(st->tx_statq_orig);
      kfree(st->sm_bufs);
      kfree(st->lg_bufs);
      kfree(st->scq_orig);
      free_irq(st->irq, st);

      {
	  IO_Rec *rec;
	  
	while ((skb = skb_dequeue(&st->rx_skb_queue)))
	  dev_kfree_skb(skb, FREE_READ);
      }

      atm_dev_deregister(st->dev);

      kfree(nicstar_st[i]);
    }
  }

  printk("nicstar: Module cleanup succeeded.\n");

}

#endif

/* NICStAR functions *******************************************************/

#if 0
static  uint32_t read_sram(Nicstar_st* st, uint32_t addr) {
  uint32_t data;
  uint32_t x;
  
  while (CMD_BUSY(st)) {};
  writel(((0x50000000) | ((addr & 0x1ffff) << 2)), st->membase + CMD);
  while (CMD_BUSY(st)) {};
  data = readl(st->membase + DR0);

  return data;
}
#endif

static void write_sram(Nicstar_st* st, int count, uint32_t addr,
			     uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3) {

  while (CMD_BUSY(st)) ;
  switch (count) {
  case 4:
    writel(d3, st->membase + DR3);
  case 3:
    writel(d2, st->membase + DR2);
  case 2:
    writel(d1, st->membase + DR1);
  case 1:
    writel(d0, st->membase + DR0);
    writel((0x40000000) | ((addr & 0x1ffff) << 2) | (count-1), st->membase+CMD);
    break;
  default:
    break;
  }

  return;
}
    
static int init_nicstar(Nicstar_st *st) {
  uint32_t i, d;
  unsigned short pci_command;
  int error; 
  uint32_t tmp;
  PCIBios_clp NOCLOBBER pci = NULL;
  uint32_t cfg;
  word_t size;

  char err_str[256];

  d = readl(st->membase + STAT);
  /* Clear timer if overflow */
  if (d & 0x00000800) {
    writel(0x00000800, st->membase+STAT);
  }


  /* S/W reset */
  writel(0x80000000, st->membase+CFG);
  /*  SLOW_DOWN_IO; SLOW_DOWN_IO; SLOW_DOWN_IO;*/
  writel(0x00000000, st->membase+CFG);
  
  /* Turn everything off, but use 4k recv status queue */
  writel(0x00400000, st->membase+CFG);
  d = readl(st->membase + CFG);


  /* Determine PHY type/speed by reading PHY reg 0 (from IDT) */
  /* YYY This tested only for 77105 (SWD) */
  writel(0x80000200,st->membase + CMD); /* read PHY reg 0 command */
  while (CMD_BUSY(st)) ;
  tmp = readl(st->membase + DR0);

  if (tmp == 0x9) {
    PRINT_INIT ("PHY device appears to be 25Mbps\n");
  }
  else if (tmp == 0x30) {
    PRINT_INIT ("PHY device appears to be 155Mbps\n");
  }

  /* PHY reset */
  d = readl(st->membase+GP);
  writel(d & 0xfffffff7, st->membase+GP); /* Out of reset */
  { int j; for (j = 0; j < 100000; j++) ; }
  writel(d | 0x00000008, st->membase+GP); /* Reset */
  { int j; for (j = 0; j < 100000; j++) ; }
  writel(d & 0xfffffff7, st->membase+GP); /* Out of reset */

  while (CMD_BUSY(st)) ;
  writel(0x80000100, st->membase+CMD);    /* Sync UTOPIA with SAR clock */
  { int j; for (j = 0; j < 100000; j++) ; }

  /* Normal mode */
  while(CMD_BUSY(st)) ;
  writel(0x00000020, st->membase + DR0);
  writel(0x90000205, st->membase + CMD);

  /* Re-enable memory space and busmastering --- in case the PCI reset
   * turned us off ...
   */

  TRY {
    pci = NAME_FIND("sys>PCIBios", PCIBios_clp);
  } 
  CATCH_Context$NotFound(name) {
    PRINT_ERR("pci_probe: PCIBios not present\n");
    return -EIO;
  }
  ENDTRY;

  ENTER_KERNEL_CRITICAL_SECTION();

  do {
      error = PCIBios$ReadConfigWord(pci, st->pci_bus,st->pci_devfn,
				     PCI_COMMAND,&pci_command);
      if (error) {
	  sprintf(err_str, "Can't read PCI_COMMAND from device\n");
	  break;
      }
      
      pci_command |= 
	  (PCI_COMMAND_MEMORY | PCI_COMMAND_SPECIAL | PCI_COMMAND_MASTER);
      error = PCIBios$WriteConfigWord(pci, st->pci_bus,st->pci_devfn,
				      PCI_COMMAND, pci_command);
      
      if (error) {
	  
	  sprintf(err_str,"Can't enable board memory for device\n");
	  break;
      }

      /* Set up the recv connection table */
      for (i = 0; i < RX_CTABLE_SIZE; i+=4) {
	  write_sram(st, 4, i, 0x00000000, 0x00000000, 0x00000000, 0xffffffff);
      }

      writel(0x0, st->membase + VPM);

  } while (0);
  
  LEAVE_KERNEL_CRITICAL_SECTION();

  if (error) {
      printf(err_str);
      return -EIO;
  }

  /* Clear VCI map and SCQ shadow */
  memset(st->vci_map, 0, sizeof(st->vci_map));
  memset(st->scq_shadow, 0, sizeof(st->scq_shadow));
  memset(st->tmp_cell, 0, sizeof(st->tmp_cell));

  /* Allocate Rx status queue */
  /* XXX What about 64-bit pointers? */
  st->rx_statq_orig = Heap$Malloc(Pvs(heap), 2*8192);
  if (!st->rx_statq_orig) {
      PRINT_ERR("Can't allocate Rx status queue.\n");
      return -ENOMEM;
  }
  st->rx_statq = 
      (nicstar_rsqe *)(((uint32_t)st->rx_statq_orig + (8192-1)) & ~(8192 - 1));

  PRINT_INIT("Rx status queue at 0x%lx (0x%lx).\n",
	st->rx_statq, st->rx_statq_orig);

  for (i = 0; i < RX_STATQ_ENTRIES; i++) {
      st->rx_statq[i].vpi_vci = 0x0;
      st->rx_statq[i].buf_handle = 0x0;
      st->rx_statq[i].crc = 0x0;
      st->rx_statq[i].status = 0x0;
  }

  st->rx_statq_next = st->rx_statq;
  st->rx_statq_last = st->rx_statq + (RX_STATQ_ENTRIES - 1);

  /* Allocate Tx status queue */
  /* XXX What about 64-bit pointers? */
  st->tx_statq_orig = (caddr_t)kmalloc(2*8192, GFP_KERNEL);
  if (!st->tx_statq_orig) {
      PRINT_ERR("Can't allocate Tx status queue.\n");
      return -ENOMEM;
  }
  st->tx_statq = 
      (nicstar_tsi *)(((uint32_t)st->tx_statq_orig + (8192-1)) & ~(8192-1));

  PRINT_INIT("Tx status queue at 0x%lx (0x%lx).\n",
	  st->tx_statq, st->tx_statq_orig);

  for (i = 0; i < TX_STATQ_ENTRIES; i++) {
      st->tx_statq[i].timestamp = 0x80000000;
  }
  st->tx_statq_next = st->tx_statq;
  st->tx_statq_last = st->tx_statq + (TX_STATQ_ENTRIES - 1);
  
  writel((uint32_t)st->rx_statq, st->membase + RSQB);
  writel((uint32_t)0x0, st->membase + RSQH);
  writel((uint32_t)st->tx_statq, st->membase + TSQB);
  writel((uint32_t)0x0, st->membase + TSQH);

  /* Allocate buffer queues */

  st->str = STR_NEW(LG_BUFSZ * NUM_LG_BUFS);
  STR_SETGLOBAL(st->str, SET_ELEM(Stretch_Right_Read) |
		SET_ELEM(Stretch_Right_Write));

  st->lg_bufs = STR_RANGE(st->str, &size);
  st->lg_bufs_limit = st->lg_bufs + size;

  if (!st->lg_bufs) {
      PRINT_ERR("Can't allocate %d %d-byte large buffers!\n",
	      NUM_LG_BUFS, LG_BUFSZ);
      return -ENOMEM;
  }
  
  memset(st->lg_bufs, 0x0d, LG_BUFSZ * NUM_LG_BUFS);
  
  PRINT_INIT("Large buffers at 0x%lx.\n", st->lg_bufs);

  st->sm_bufs = Heap$Malloc(Pvs(heap), SM_BUFSZ * NUM_SM_BUFS);
  
  PRINT_INIT("Small buffers at %p\n", st->sm_bufs);

  for (i = 0; i < NUM_LG_BUFS; i+=2) {
      push_rxbufs(st, 1, 
		  (uint32_t)(st->lg_bufs + (i*LG_BUFSZ)),
		  (uint32_t)(st->lg_bufs + (i*LG_BUFSZ)),
		  (uint32_t)(st->lg_bufs + ((i+1)*LG_BUFSZ)),
		  (uint32_t)(st->lg_bufs + ((i+1)*LG_BUFSZ)));
#ifdef PARANOID_NICSTAR
      st->buf_table[i] = True;
      st->buf_table[i+1] = True;
#endif
  }

  for (i = 0; i < NUM_SM_BUFS; i+=2) {
      push_rxbufs(st, 0,
		  (uint32_t)(st->sm_bufs + (i*SM_BUFSZ)),
		  (uint32_t)(st->sm_bufs + (i*SM_BUFSZ)),
		  (uint32_t)(st->sm_bufs + ((i+1)*SM_BUFSZ)),
		  (uint32_t)(st->sm_bufs + ((i+1)*SM_BUFSZ)));
  }

  /* Allocate seg chan queue */
  /* XXX What about 64-bit pointers? */
  st->scq_orig = Heap$Malloc(Pvs(heap),2*8192);
  if (!st->scq_orig) {
      PRINT_ERR("Can't allocate seg chan queue.\n");
      return -ENOMEM;
  }
  st->scq = (nicstar_tbd *)(((uint32_t)st->scq_orig + (8192-1)) & ~(8192-1));
  st->scq_next = st->scq;
  st->scq_last = st->scq + (SCQ_ENTRIES - 1);
  st->scq_tail = st->scq_last; /* XXX mdw scq_next */
  st->scq_full = 0;
  
  PRINT_INIT("SCQ at 0x%lx.\n",st->scq);

  cfg = CFG_TXUIE | CFG_TXEN | CFG_TXINT | CFG_RAWIE | CFG_RQFIE |
        CFG_RXINT_314 | CFG_RXSTQ_8192 | CFG_RXPTH | CFG_SMBUF_48;

  /* cfg = 0x20801c38; */

  PRINT_INIT("Using large buffer size %d\n", LG_BUFSZ);

  switch(LG_BUFSZ)
  {
  case 2048: cfg |= CFG_LGBUF_2048; break;

  case 4096: cfg |= CFG_LGBUF_4096; break;

  case 8192: cfg |= CFG_LGBUF_8192; break;

  case 16384: cfg |= CFG_LGBUF_16384; break;

  default: PRINT_INIT("Invalid Large Buffer size %d", LG_BUFSZ);
      ntsc_halt();
      break;
  }


  write_sram(st, 4, NICSTAR_VBR_SCD0,
	     (uint32_t)st->scq,        /* SCQ base */
	     (uint32_t)0x0,            /* tail */
	     (uint32_t)0xffffffff,     /* AAL5 CRC */
	     (uint32_t)0x0);           /* rsvd */

  /* Install TST */
  write_sram(st, 2, NICSTAR_TST_REGION,
	     (uint32_t)0x40000000,     /* Tx VBR SCD */
	     /* (uint32_t)0x20000000 | NICSTAR_SCD_REGION, *//* Tx SCD */
	     (uint32_t)0x60000000 | NICSTAR_TST_REGION, /* Jump to start */
	     0x0, 0x0);

  writel((uint32_t)(NICSTAR_TST_REGION << 2), st->membase + TSTB);

  writel(cfg, st->membase+CFG);        /* Turn on everything */

  st->sm_buf = 0;
  st->lg_buf = 0;
  st->num_clients = 0;

  CV_INIT(&st->scqfull_waitc);

  return 0;
}

void nicstar_interrupt(Nicstar_st *st) {

  uint32_t stat, wr_stat;

  wr_stat = 0x0;

  stat = readl(st->membase + STAT);

  PRINT_IP("Interrupt: STAT 0x%lx.\n", stat);

  if (stat & 0x8000) {
      PRINT_IP("TSI written to Tx status queue.\n");
      if(process_tx_statq(st, True)) {
	  wr_stat |= 0x8000;
      } else {
	  PRINT_TX("No TX statq entries, (TX %d, TX ack %d)\n", 
		 st->tx_count, st->tx_ack_count);
      }
  }
  if (stat & 0x4000) {
      wr_stat |= 0x4000;
      PRINT_ERR("Incomplete CS-PDU transmitted.\n");
  }

  if (stat & 0x800) {
      wr_stat |= 0x800;
      PRINT_IP("Timer overflow.\n");
  }
  if (stat & 0x100) {
      PRINT_IP("Small buffer queue full (this is OK).\n");
  }
  if (stat & 0x80) {
      PRINT_IP("Large buffer queue full (this is OK).\n");
  }
  if (stat & 0x40) {
      PRINT_ERR("Rx status queue full.\n");
      if(process_rx_statq(st)) {
	  wr_stat |= 0x40;
      } else
      {
	  PRINT_ERR("RSQ full :No RX statq entries to process, (RX %d)\n", st->rx_count);
      }
  }
  if (stat & 0x20) {
#if 0
      writel(0x20, st->membase + STAT); /* XXX mdw ack immediately */
#endif
      PRINT_IP("End of PDU received.\n");
      if(process_rx_statq(st)) {
	  wr_stat |= 0x20;
      } else {
	  PRINT_ERR("EPDU: No RX statq entries to process, (RX %d)\n", st->rx_count);
      }	
  }
  if (stat & 0x10) {
      wr_stat |= 0x10;
      PRINT_IP("Raw cell received.\n");
  }

  if (stat & 0x8) {
#if 0
      PRINT_IP("Small buffer queue empty, disabling interrupt.\n");
      writel((readl(st->membase + CFG) & ~0x01000000), st->membase + CFG); 
#endif
      wr_stat |= 0x8;
  }
  if (stat & 0x4) {
#if 0
      PRINT_IP("Large buffer queue empty, disabling interrupt.\n");
      
      writel((readl(st->membase + CFG) & ~0x01000000), st->membase + CFG); 
#endif
      wr_stat |= 0x4;
  }
  if (stat & 0x2) {
      PRINT_IP("Rx status queue almost full.\n");
      /* XXX Turn off interrupt */

      writel((readl(st->membase + CFG) & ~0x00000400), st->membase + CFG); 

      if(process_rx_statq(st)) {
	  wr_stat |= 0x2;
      } else {
	  PRINT_ERR("RSQAF: No RX statq entries to process, (RX %d)\n", st->rx_count);
      }	
      
  }
  
  if (wr_stat != 0x0) {

      writel(wr_stat, st->membase + STAT);

  }

  return;
}

static bool_t process_tx_statq(Nicstar_st* st, bool_t wait) {

  volatile nicstar_tsi *next;
  int num_skipped = 0;
  bool_t processed = False;

  /* Spin until something arrives. If we get a timer-rollover and a
   * TSI simultaneously this could be dangerous; disabling timer 
   * interrupt would prevent that.
   */
  next = st->tx_statq_next;
  if (wait) {

      /* The errata state that there may be a delay between the TSIF
         status bit being set and the TSI being written, but that it
         will arrive within "a few" clock cycles ... How many is that
         meant to be ? */

      int delay = 200;
      while ((delay--) && (next->timestamp & 0x80000000));

  }

  while(num_skipped < 2)
  {
      if (!(next->timestamp & 0x80000000)) {
	  st->tx_statq_next = next;

	  if(num_skipped != 0) { 
	      PRINT_ERR ("Skipped %d TSI entries to %p !!!\n", 
			 num_skipped, next); 
	  }
	  
	  num_skipped = 0;
	  PRINT_IP("Tx statq entry 0x%lx 0x%lx\n",
		   (unsigned long)st->tx_statq_next->status,
		   (unsigned long)st->tx_statq_next->timestamp);
	  
	  if ((st->tx_statq_next->status & 0xffff0000) == SCQFULL_MAGIC) {
	      drain_scq(st, st->tx_statq_next->status & 0xffff);
	      
	      /* SCQ drained, wake 'er up */
	      st->scq_full = 0; 
	      BROADCAST(&st->scqfull_waitc);
	  }
	  
	  st->tx_statq_next->timestamp |= 0x80000000;
	  processed = True;
      } else {
	  num_skipped ++;
      }
      
      if (next == st->tx_statq_last)
	  next = st->tx_statq;
      else
	  next++;

      if(num_skipped == 0)
      {
	  st->tx_statq_next = next;
      }
  }

  writel((uint32_t)st->tx_statq_next - (uint32_t)st->tx_statq,
	 st->membase + TSQH);
  
  return processed;
}

static bool_t process_rx_statq(Nicstar_st* st) {
  uint32_t prev;
  bool_t processed = False;

  /* XXX Problem here: The 77201 can interrupt us (and set EPDU
   * bits in STAT) before the Rx statq entry arrives in memory.
   * The interrupt could be raised for multiple incoming PDUs,
   * and we could miss seeing those entries that are slow to 
   * arrive in memory at the end of this loop. We'll see the PDU
   * on the next Rx EPDU interrupt, of course.
   */
  while (st->rx_statq_next->status & 0x80000000) {
    PRINT_IP("Rx statq entry 0x%lx 0x%lx 0x%lx 0x%lx\n",
	   (unsigned long)st->rx_statq_next->vpi_vci,
	   (unsigned long)st->rx_statq_next->buf_handle,
	   (unsigned long)st->rx_statq_next->crc,
	   (unsigned long)st->rx_statq_next->status);
    
    dequeue_rx(st);
    st->rx_statq_next->status = 0x0;

    if (st->rx_statq_next == st->rx_statq_last)
      st->rx_statq_next = st->rx_statq;
    else
      st->rx_statq_next++;

    processed = True;
  }

  /* Update the RSQH to point to the last entry actually processed. */
  if (st->rx_statq_next == st->rx_statq) {
    prev = (uint32_t)st->rx_statq_last;
  } else {
    prev = (uint32_t)(st->rx_statq_next - 1);
  }

  writel((uint32_t)prev - (uint32_t)st->rx_statq,
	 st->membase + RSQH);

  return processed;
}

static void push_rxbufs(Nicstar_st* st, int lg, 
			uint32_t handle1, uint32_t addr1, uint32_t handle2, 
			uint32_t addr2) {

  int bc;

  bc = readl(st->membase + STAT);

  if (lg) {
      PRINT_RX("Pushing large rxbufs onto queue %x, %x\n", addr1, addr2);
      bc = (bc & 0x80);
  } else {
      PRINT_RX("Pushing small rxbufs onto queue %x, %x\n", addr1, addr2);
      bc = (bc & 0x100);
  }
  if (bc != 0) {
/*
  Intel-specific idiocy
  asm("int $3");
  */
      PRINT_ERR("Buffer queue full (bufs %x, %x) !!!\n", addr1, addr2);
      return; 
  }

  while (CMD_BUSY(st)) ;
  
  writel(handle1, st->membase + DR0);
  writel(addr1, st->membase + DR1);
  writel(handle2, st->membase + DR2);
  writel(addr2, st->membase + DR3);

  writel(0x60000000 | (lg ? 0x01 : 0x00), st->membase + CMD);

#ifdef PARANOID_NICSTAR
  bc = (readl(st->membase + STAT) & 0x00ff0000) >> 15;
#endif

#ifdef PARANOID_NICSTAR
  if (bc > NUM_LG_BUFS)
  {
      PRINT_ERR("Too many large buffers\n");
      ntsc_halt();
  }
#endif  
  return;
}

void nicstar_open_rx_connection(Nicstar_st* st,
			       int index,
			       uint32_t status) {

    uint32_t sram_addr = index * (sizeof(nicstar_rcte)/sizeof(uint32_t));
    
    PRINT_RX("Opening RX connection, index %d\n", index);
    
    
    write_sram(st, 1, sram_addr, status,
	       0x0, 0x0, 0x0);
    

}

void nicstar_enable_rx_connection(Nicstar_st* st, int index)
{
    uint32_t sram_addr = index * (sizeof(nicstar_rcte)/sizeof(uint32_t));
    while (CMD_BUSY(st)) ;
    writel(0x20080000 | (sram_addr << 2), st->membase + CMD);
}

void nicstar_disable_rx_connection(Nicstar_st* st, int index) 
{
    uint32_t sram_addr = index * (sizeof(nicstar_rcte)/sizeof(uint32_t));
    
    while (CMD_BUSY(st)) ;
    writel(0x20000000 | (sram_addr << 2), st->membase + CMD);

}

static void push_scq_entry(Nicstar_st* st,
			   nicstar_tbd *entry,
			   int xmit_now) {

  nicstar_tbd magic_bullet = {0xa0000000, SCQFULL_MAGIC, 0x0, 0x0};
  int delayed_bullet = 0;

  PRINT_TX("tail 0x%lx next 0x%lx\n",
   (long)st->scq_tail, (long)st->scq_next);

  if (st->scq_tail == st->scq_next) {
    /* Sleep until the tail moves */

      ntsc_halt();
      st->scq_full = 1;
      {
	  mutex_t mu;
	  MU_INIT(&mu);
	  MU_LOCK(&mu);
	  SRCThread$WaitUntil(Pvs(srcth), &st->mu, 
			      &st->scqfull_waitc, NOW() + SCQFULL_TIMEOUT);
	  MU_RELEASE(&mu);
	  MU_FREE(&mu);
      }
      if (st->scq_full) {
	  PRINT_ERR("SCQ drain timed out.\n");
	  st->scq_full = 0;
	  /* XXX Just proceed here, although the SAR's probably hosed ... */
    }
  }

  *st->scq_next = *entry;
  
  if (st->scq_next == st->scq_last)
    st->scq_next = st->scq;
  else
    st->scq_next++;

/*  if (((st->scq_next - st->scq) % NUM_SCQ_TRAIL) == NUM_SCQ_TRAIL-1) {*/
    delayed_bullet = 1;
/*  }*/

  if (xmit_now) {
    if (delayed_bullet) {
      PRINT_IP("Writing TSR ===============================\n");
      delayed_bullet = 0;
      magic_bullet.buf_addr |= (uint32_t)(st->scq_next - st->scq);
      *st->scq_next = magic_bullet;
      if (st->scq_next == st->scq_last)
        st->scq_next = st->scq;
      else
        st->scq_next++;
    }

    write_sram(st, 1, NICSTAR_VBR_SCD0,
	       (uint32_t)(st->scq_next),
	       0x0, 0x0, 0x0);
  }

  return;
}

/* Allocate a free connection. Must be called with conn_mu held */

static void get_ci(Nicstar_st* st,
		  AALPod_Dir mode,
		  short *vpi, int *vci) {
  nicstar_vcimap *mapval;

  if (*vpi != 0) RAISE_ATM$BadVCI(*vpi, *vci);

  if (*vci >= NUM_VCI) RAISE_ATM$BadVCI(*vpi, *vci);

  mapval = &(st->vci_map[*vci]);

  if (mode == AALPod_Dir_RX && mapval->rx) {
      PRINT_ERR("RX VCI %d in use\n", *vci);
      RAISE_ATM$VCIInUse(*vpi, *vci);
  }

  if (mode == AALPod_Dir_TX && mapval->tx) {
      PRINT_ERR("TX VCI %d in use\n", *vci);
      RAISE_ATM$VCIInUse(*vpi, *vci);
  }
      
  if (mode == AALPod_Dir_RX) {
      PRINT_NORM("Reserving RX VCI %d\n", *vci);
      mapval->rx = True; 
  } 
  if (mode == AALPod_Dir_TX) {
      PRINT_NORM("Reserving TX VCI %d\n", *vci);
      mapval->tx = True; 
  }
}

static void free_rx_buf(Nicstar_st* st, int lg, uint32_t buf_addr) {

  uint32_t tmp_buf;
  PRINT_RX("Freeing buffer %p\n", buf_addr);

#ifdef PARANOID_NICSTAR
  {
      int buf_num;
      if (!((buf_addr >= st->lg_bufs) && 
	    (buf_addr <= st->lg_bufs+(LG_BUFSZ * NUM_LG_BUFS)))) 
      {
	  PRINT_ERR("Trying to push invalid buffer - outside buffer space\n");
	  ntsc_halt();
      }
      
      buf_num = (((void *)buf_addr)-(st->lg_bufs)) / LG_BUFSZ;
      if (st->lg_bufs + (buf_num * LG_BUFSZ) != (void *)buf_addr)
      {
	  PRINT_ERR("Trying to push invalid buffer - not aligned\n");
	  ntsc_halt();      
      }
      
      if ((buf_num < 0)  || (buf_num >= NUM_LG_BUFS))
      {
	  PRINT_ERR("Trying to push invalid buffer - invalid buffer number %d\n", 
		    buf_num);
	  ntsc_halt();
      }
      if(st->buf_table[buf_num])
      {
	  PRINT_ERR("st->lg_bufs = %p\n", st->lg_bufs);
	  PRINT_ERR("Trying to push buffer %d twice\n", buf_num);
	  ntsc_halt();      
      }
      
      PRINT_RX("Pushing buffer %d\n", buf_num);
      
      st->buf_table[buf_num] = True;
  }
#endif
  
  if (lg) {
      /* Large buffer */
      if (st->lg_buf == 0x0) {
      st->lg_buf = buf_addr;
    } else {
      uint32_t buf;
      buf = buf_addr;
      tmp_buf = st->lg_buf;
      st->lg_buf = 0x0;
      push_rxbufs(st, 1, tmp_buf, tmp_buf, buf, buf);
    }
  } else {
    /* Small buffer */
    if (st->sm_buf == 0x0) {
      st->sm_buf = buf_addr;
    } else {
      uint32_t buf;
      buf = buf_addr;
      tmp_buf = st->sm_buf;
      st->sm_buf = 0x0;
      push_rxbufs(st, 0, tmp_buf, tmp_buf, buf, buf);
    }
  }
  return;
}

static void dequeue_rx(Nicstar_st* st) {
  
    struct rbuf* r;
    IO_Rec *rec;
    nicstar_rxclient* rxclient;
    nicstar_vcimap *mapval;
    volatile nicstar_rsqe *entry = (volatile nicstar_rsqe *)st->rx_statq_next;
    unsigned short aal5_len;

    PRINT_RX("Dequeueing rx rbuf \n");
    
    if ((entry->vpi_vci & 0x00ff0000) ||
	((entry->vpi_vci & 0xffff) > NUM_VCI)) {
	PRINT_ERR("SDU received for out of range VC %d.%d.\n",
		  entry->vpi_vci >> 16,
		  entry->vpi_vci & 0xffff);
	return;
    }
    
    mapval = &(st->vci_map[entry->vpi_vci]);
    if (!mapval->rx) {
	PRINT_ERR("SDU received on inactive VC 0.%d.\n",
		  entry->vpi_vci);
	return;
    }
    
    rxclient = mapval->rxclient;
    
    r = &mapval->rbuf;
    
    rec = &r->r_recs[r->r_nrecs];
    rec->base = (void *) entry->buf_handle;
    
#ifdef PARANOID_NICSTAR
    {
	int buf_num;
	
	if (!(((entry->buf_handle) >= st->lg_bufs) && 
	      ((entry->buf_handle) <= st->lg_bufs+(LG_BUFSZ * NUM_LG_BUFS)))) 
	{
	    PRINT_ERR("Dequeueing invalid buffer - outside buffer space\n");
	    ntsc_halt();
	}
	
	buf_num = (((void *)(entry->buf_handle))-(st->lg_bufs)) / LG_BUFSZ;
	
	if ((buf_num < 0)  || (buf_num >= NUM_LG_BUFS))
	{
	    PRINT_ERR("Dequeueing invalid buffer - invalid buffer number\n");
	    ntsc_halt();
	}
	
	PRINT_RX("Dequeueing RX buf %d\n", buf_num);
	
	if (st->lg_bufs + (buf_num * LG_BUFSZ) != (void *)(entry->buf_handle))
	{
	    PRINT_ERR("Dequeueing invalid buffer - not aligned\n");
	    ntsc_halt();      
	}
	
	if(!st->buf_table[buf_num])
	{
	    PRINT_ERR("Dequeuing unused buffer\n");
	    ntsc_halt();      
	}
	
	st->buf_table[buf_num] = False;
    }
#endif

  switch (rxclient->mode) {

  case AALPod_Mode_AAL5:

      if (entry->status & 0x2000) {
	  /* Last buffer, get AAL5 len */
	  aal5_len = *(unsigned short *)((unsigned char *)entry->buf_handle + 
					 ((entry->status & 0x1ff)*48) - 6);
	  aal5_len = ((aal5_len & 0xff) << 8) | ((aal5_len & 0xff00) >> 8);
	  
	  if (aal5_len <= r->r_len)
	  {
	      /* This is a padding cell */
	      
	      if (r->r_nrecs == 0) {
		  PRINT_ERR("Got negative size on first cell !!!\n");
	      }
	      rec--;
	      rec->len -= (r->r_len - aal5_len);
	      PRINT_RX("Got padding cell\n");
	      
	      /* This must be a large free buffer */
	      
	      free_rx_buf(st, 1, entry->buf_handle);
	  }
	  else
	  {
	      rec->len = aal5_len - r->r_len;
	      r->r_nrecs++;
	      
	      PRINT_RX("Got data, len = %d\n", rec->len);
	  }
	  
	  r->r_len = aal5_len;
	  PRINT_RX("r = 0x%lx, r->r_nrecs = %d\n", r, r->r_nrecs);
	  /*    vcc->stats->rx++;*/
	  
	  PRINT_RX("Got end of packet, rec len %d, total len %d\n", 
		   rec->len, r->r_len);
	  
#ifdef NICSTAR_STATS
	  
	  while(*((uint32_t *) r->r_recs->base) != st->rx_count)
	  {
	      PRINT_ERR("Dropped packet %d\n", st->rx_count++);
	  }
#endif
	  
	  push_rxrbuf(st, rxclient, r);
	  r->r_len = 0;
	  r->r_nrecs = 0;
	  
      } else {
	  rec->len = (entry->status & 0x1ff) *48;
	  r->r_len += rec->len;
	  r->r_nrecs++;
	  PRINT_RX("r = 0x%lx, r->r_nrecs = %d\n", r, r->r_nrecs);
	  PRINT_RX("Got data, len %d\n", rec->len);
      }
      break;

  case AALPod_Mode_AAL5_Raw:

      rec->len = (entry->status & 0x1ff) * 48;
      r->r_len += rec->len;
      r->r_nrecs++;
      
      PRINT_RX("Got data, rec %d, len %d\n", r->r_nrecs, rec->len);
      
      if(entry->status & 0x2000) {
	  /* Last buffer */
	  
	  PRINT_RX("Got end of AAL5_PDU\n");
	  
#ifdef NICSTAR_STATS
	  
	  while(*((uint32_t *) r->r_recs->base) != st->rx_count)
	  {
	      PRINT_ERR("Dropped packet %d\n", st->rx_count++);
	  }
#endif
	  
	  push_rxrbuf(st, rxclient, r);
	  r->r_nrecs = 0;
	  r->r_len = 0;
      }
      break;

  case AALPod_Mode_AAL0:

      rec->len = (entry->status & 0x1ff) * 48;

      PRINT_RX("Received AAL0 PDU, length %d\n", rec->len);
      
      r->r_len += rec->len;
      r->r_nrecs++;
      
      PRINT_RX("EPDU bit = %d\n", entry->status & 0x2000);

      push_rxrbuf(st, rxclient, r);
      r->r_nrecs = 0;
      r->r_len = 0;
     
      break;
      
  case AALPod_Mode_AAL34:

      rec->len = (entry->status & 0x1ff) * 48;

      PRINT_RX("Received AAL3/4 PDU, length %d\n", rec->len);
      r->r_len += rec->len;
      r->r_nrecs++;

      PRINT_RX("EPDU bit = %d\n", entry->status & 0x2000);

      if(entry->status & 0x2000) {
	  
	  push_rxrbuf(st, rxclient, r);
	  r->r_nrecs = 0;
	  r->r_len = 0;
	  
      }
      break;
  }
  
  st->rx_count++;

  return;
      
}

void nicstar_free_rxrbuf(Nicstar_st* st, struct rbuf* r) {

  IO_Rec *rec;
  int i;
  int lg;

  PRINT_RX("Freeing RX Rbuf %p\n", r);

  rec = r->r_recs;
  for (i = 0; i < r->r_nrecs; i++) {
      lg = ((rec[i].base >= st->lg_bufs) && (rec[i].base < st->lg_bufs_limit));
      free_rx_buf(st, lg, (uint32_t)(rec[i].base));
      lg = 1;
  }

  r->r_len = 0;

  return;
}
    
static void drain_scq(Nicstar_st* st, int index) {

  nicstar_tbd *last = st->scq + index;
  int sindex = st->scq_tail - st->scq;
  struct rbuf *r;

  while (st->scq_tail != last) {
      if ((r = st->scq_shadow[sindex].rbuf)) {

	  st->tx_ack_count++;

	  if(!(st->tx_ack_count % 16384)) {
	      PRINT_STATS("Transmitted %u PDUs\n", (uint32_t)st->tx_ack_count);
	  }

	  PRINT_TX("Freeing rbuf %p\n", r);

	  IO$PutPkt(r->r_io, r->r_nrecs, r->r_recs, 0, FOREVER);
	  r->r_type = RT_FREE;
	  /* The txclient is stored in r->r_st */
	  EC_ADVANCE(((nicstar_txclient *)r->r_st)->ec, 1); 

	  st->scq_shadow[sindex].rbuf = 0;
      }
    
      if (st->scq_tail == st->scq_last) {
	  st->scq_tail = st->scq; sindex = 0;
      } else {
	  st->scq_tail++; sindex++;
      }

  }

}


void InterruptThread_m (Nicstar_st *st)
{
    int lg_buf_count, sm_buf_count;
    PRINT_INIT("Entered ...\n");
    
    while (1)  {
        Event_Val val;
        
        /* Wait for next interrupt event */

	ENTER_KERNEL_CRITICAL_SECTION();
        ntsc_unmask_irq(st->irq);
	LEAVE_KERNEL_CRITICAL_SECTION();
        
        val = EC_AWAIT_UNTIL(st->event, st->ack, NOW()+SECONDS(1)); 
	if (val < st->ack) {

	    lg_buf_count = (readl(st->membase + STAT) & 0x00ff0000) >> 15;
	    lg_buf_count = (readl(st->membase + STAT) & 0x00ff0000) >> 15;
	    sm_buf_count = (readl(st->membase + STAT) & 0xff000000) >> 23;
	    
	    if(st->num_clients > 0) {
		PRINT_ERR(
		    "Timeout: TX queued %d, TX Acked %d, RX %d, LBQ %d SBQ %d\n", 
		    st->tx_count, st->tx_ack_count, st->rx_count, 
		    lg_buf_count, sm_buf_count);
	    }

	} else {

	    st->ack = ++val;
	    
	    if (val > st->ack) {
		PRINT_ERR("Took %d interrupts %ux %ux\n",
			  (val - st->ack)+1, (uint32_t) val,(uint32_t)st->ack);
	    }
	}

        /* Service the interrupt */
        nicstar_interrupt(st);

	st->ints++;
    }
}

void push_rxrbuf(Nicstar_st *st, nicstar_rxclient* rxclient, 
		struct rbuf* r)
{

    void* base;
    int i;

    IO_Rec rec;
    int nrecs, value;
    
    IO_clp io = rxclient->io;

    PRINT_RX("got a packet\n");

    PRINT_RX("data length %d\n", r->r_len);
    PRINT_RX("num frags %d\n", r->r_nrecs);
    PRINT_RX("frag 1 len %d, frag 2 len %d\n", r->r_recs[0].len, r->r_recs[1].len);

    if(!rxclient->enabled) {

	nicstar_free_rxrbuf(st, r);
	return;

    }

    if(rxclient->master == AALPod_Master_Server) {

	/* We are channel master */

	while(IO$GetPkt(io, 1, &rec, 0, &nrecs, &value)) {

	    if(!((rec.base >= st->lg_bufs) && 
		 (rec.base < st->lg_bufs_limit))) {
		/* Nicstar is channel master, but client has tried to pass
		   in a buffer that it created */
		
		PRINT_ERR ("Invalid packet %p - disabling vci %d!\n", 
			   rec.base, rxclient->vci);
		
		nicstar_disable_rx_connection(st, rxclient->vci);
		rxclient->enabled = False;
		
		IO$PutPkt(io, 1, &rec, 0, 0);
		
		nicstar_free_rxrbuf(st, r);

		return;
	    }
	    
	    rxclient->bufs_allowed++;
	    free_rx_buf(st, 1, (uint32_t) rec.base);

	}
	
	/* See if they have now have any buffers allowed */
	
	if (rxclient->bufs_allowed) {
	    
	    bool_t NOCLOBBER success = True;

	    /* If we are mastering the channel, it's possible that we
	       can have something to send before the Binder finishes
	       connecting the channel - this is bad, since it raises a
	       Channel$Invalid exception. If TRYs were free, we'd just
	       do a TRY always, but they're not (yet ...) */

	    
	    if(rxclient->definitely_connected) {
		IO$PutPkt(io, 1, r->r_recs, 1, 0);
	    } else {
		TRY {
		    IO$PutPkt(io, 1, r->r_recs, 1, 0);
		    rxclient->definitely_connected = True;
		} CATCH_Channel$Invalid(ep) {
		    /* Oops ... */
		    success = False;
		} ENDTRY; 
	    }

	    if(success) {
		rxclient->bufs_allowed--;
		rxclient->pdus_missed = 0;

		rxclient->rx_count++;
		if(!(rxclient->rx_count % 16384)) {
		    PRINT_STATS("Received %d PDUs on vci %d\n", 
				(uint32_t)rxclient->rx_count, rxclient->vci);
		}

		return;
	    } 


	}

    } else {
	
	/* Client is channel master */

	if(IO$GetPkt(io, 1, &rec, 0, &nrecs, &value)) {
	    
	    if(rec.len >= r->r_len) {
		
		base = rec.base;
		
		for(i = 0; i<r->r_nrecs; i++)
		{
		    PRINT_RX("Copying data to %p, len %d\n", base, r->r_recs[i].len);
		    ENTER_KERNEL_CRITICAL_SECTION(); /* XXX Should do this properly */
		    memcpy(base, r->r_recs[i].base, r->r_recs[i].len);
		    LEAVE_KERNEL_CRITICAL_SECTION();
		    base += r->r_recs[i].len;
		}
		
		rec.len = r->r_len;
		rxclient->pdus_missed = 0;

		rxclient->rx_count++;
		if(!(rxclient->rx_count % 16384)) {
		    PRINT_STATS("Received %d PDUs on vci %d\n", 
				(uint32_t)rxclient->rx_count, rxclient->vci);
		}
		
		IO$PutPkt(io, 1, &rec, 1, 0);
	    } else {
		PRINT_ERR("Packet too large (%d) on vci %d  (rec [%p, %d])!\n",
			  r->r_len, rxclient->vci, rec.base, rec.len);
		IO$PutPkt(io, 1, &rec, 0, 0);
	    }
	    
	    nicstar_free_rxrbuf(st, r);

	    return;
	}
    }
    
    /* Client didn't supply an IO rec */

    rxclient->ditch_count++;
    if(!(rxclient->ditch_count % 512)) {
	PRINT_STATS("Ditched %d packets on VCI %d\n", 
		    rxclient->ditch_count, rxclient->vci);
    }
    PRINT_RX("IO Rec not available on vci %d - Ditching RX packet !\n", 
	     rxclient->vci);
    
    if(st->facistly_disable_slowpokes && rxclient->enabled) {
	if((++rxclient->pdus_missed) > NICSTAR_MAX_MISSES_ALLOWED) {
	    
	    PRINT_NORM("Disabling RX connection on VCI %d\n", rxclient->vci);
	    rxclient->enabled = False;
	    nicstar_disable_rx_connection(st, rxclient->vci);
	    
	    /* Wake up the RX thread to wait for an incoming packet */
	    EC_ADVANCE(rxclient->ec, 1);
	}
    }
    
    nicstar_free_rxrbuf(st, r);
    
}

/* allocate an rxclient structure */
nicstar_rxclient* nicstar_allocrxclient (Nicstar_st *st)
{
  word_t x = st->rxclientfreelist;
  if (x != 0) {
    st->rxclientfreelist = st->rxclient[x].next;
    if (st->rxclient[x].ec == 0) {
      st->rxclient[x].ec = EC_NEW();
      PRINT_NORM("Got RX EC %p\n", (void *)st->rxclient[x].ec);
    }
    return(&st->rxclient[x]);
  } else {
      return (NULL);
  }
}

/* free an rxclient */
void nicstar_freerxclient (Nicstar_st *st, nicstar_rxclient* rxclient)
{
  rxclient->next = st->rxclientfreelist;
  st->rxclientfreelist = rxclient - st->rxclient;
}

/* allocate a txclient structure */
nicstar_txclient* nicstar_alloctxclient (Nicstar_st *st)
{
  word_t x = st->txclientfreelist;
  if (x != 0) {
    st->txclientfreelist = st->txclient[x].next;
    if (st->txclient[x].ec == 0) {
      st->txclient[x].ec = EC_NEW();
      PRINT_NORM("Got TX EC %p\n", (void *)st->txclient[x].ec);
    }
    return(&st->txclient[x]);
  } else {
      return(NULL);
  }
}

/* free a txclient */
void nicstar_freetxclient (Nicstar_st *st, nicstar_txclient* txclient)
{
  txclient->next = st->txclientfreelist;
  st->txclientfreelist = txclient - st->txclient;
}

