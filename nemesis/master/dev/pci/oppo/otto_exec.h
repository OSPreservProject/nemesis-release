/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.		                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      dev/pci/oppo
**
** FUNCTIONAL DESCRIPTION:
** 
**      Oppo device driver
** 
*/

#ifndef OTTOEXEC_H
#define OTTOEXEC_H

#define OTTOEXEC_ADDR 0xC0000


/* ucode instr fields                                  */
/*                                  AAA                */
/*                                  LOTGHYPS           */

#define OTTOEXEC_read_rxbufctr    0xFFF0F2FA
#define OTTOEXEC_load_rxbufctr    0xFFF0AFF2
#define OTTOEXEC_incr_rxbufctr    0xFFF0BFFF
#define OTTOEXEC_decr_rxbufctr    0xFFF0CFFF

#define OTTOEXEC_read_txbufctr    0xFFF1F2FA
#define OTTOEXEC_load_txbufctr    0xFFF1AFF2
#define OTTOEXEC_incr_txbufctr    0xFFF1BFFF
#define OTTOEXEC_decr_txbufctr    0xFFF1CFFF

#define OTTOEXEC_read_rxclksub    0xFFF4F2FA
#define OTTOEXEC_load_rxclksub    0xFFF4AFF2

#define OTTOEXEC_read_rxprectr    0xFFF5F2FA
#define OTTOEXEC_load_rxprectr    0xFFF5AFF2   /* se: rxclk++ */

#define OTTOEXEC_read_rxlowbuf    0xFFF6F2FA   /* [A] se: rxlow = rxbuf */

#define OTTOEXEC_read_rxcngctr    0xFFF7F2FA
#define OTTOEXEC_incr_rxcngctr    0xFFF7BFF2   /* decr if b<12> = 1 */
#define OTTOEXEC_zero_rxcngctr    0xFFF7CFFF


#ifdef OTTOEXEC_OBSOLETE
#define OTTOEXEC_al_rxbufpdma OTTOEXEC_load_rxbufpdma   /* obsolete */
#define OTTOEXEC_al_rxbuftail OTTOEXEC_load_rxbuftail   /* obsolete */
#define OTTOEXEC_al_rxbufhead OTTOEXEC_load_rxbufhead   /* obsolete */
#define OTTOEXEC_al_txbufhead OTTOEXEC_load_txbufhead   /* obsolete */
#define OTTOEXEC_al_txbuftail OTTOEXEC_load_txbuftail   /* obsolete */
#endif

#define OTTOEXEC_load_rxbufpdma   0x57FFFFF2
#define OTTOEXEC_load_rxbuftail   0x77FFFFF2
#define OTTOEXEC_load_rxbufhead   0x37FFFFF2
#define OTTOEXEC_load_txbufhead   0x87FFFFF2
#define OTTOEXEC_load_txbuftail   0x97FFFFF2
#define OTTOEXEC_load_recvmode    0xC7FFFFF2

#define OTTOEXEC_read_rxbufhead   0xF0EFF2F3
#define OTTOEXEC_read_rxbufpdma   0xF1EFF2F3
#define OTTOEXEC_read_rxbuftail   0xF2EFF2F3
#define OTTOEXEC_read_txbufhead   0xF3EFF2F3
#define OTTOEXEC_read_txbuftail   0xF4EFF2F3
#define OTTOEXEC_read_a3c         0xF5EFF2F3
#define OTTOEXEC_read_a4c         0xF6EFF2F3
#define OTTOEXEC_read_recvmode    0xF2FFF2F3


#define OTTOEXEC_ackff_advs       0xFFFF0FFF
#define OTTOEXEC_ackff_swap       0xFFFF1FFF
#define OTTOEXEC_dmaff_advs       0xFFFF2FFF
#define OTTOEXEC_dmaff_swap       0xFFFF3FFF
#define OTTOEXEC_rdyff_advs       0xFFFF4FFF
#define OTTOEXEC_rdyff_swap       0xFFFF5FFF
#define OTTOEXEC_rptff_advs       0xFFFF6FFF
#define OTTOEXEC_rptff_swap       0xFFFF7FFF

#define OTTOEXEC_sched_incr       0xFFFF8FFF
#define OTTOEXEC_sched_load       0xF7FF9FF2

#define OTTOEXEC_ackff_read       0xF0CFF2F3
#define OTTOEXEC_dmaff_read       0xF1DFF2F3
#define OTTOEXEC_rdyff_read       0xF2DFF2F3
#define OTTOEXEC_rptff_read       0xF3DFF2F3

#define OTTOEXEC_sched_read       0xF4CFF2F3

#define OTTOEXEC_dma_wr_fragsize  0xFFFFFF02   /* se: dismiss dma needfrag */
#define OTTOEXEC_dma_wr_fragaddr  0xFFFFFF12
#define OTTOEXEC_dma_wr_piodata   0xFFFFFF22   /* se: dismiss piord */
#define OTTOEXEC_dma_wr_txlength  0xFFFFFF32
#define OTTOEXEC_dma_wr_report    0xFFFFFF42   /* se: dismiss dma needfrag */
#define OTTOEXEC_dma_poke_rx      0xFFFFFF54   /* start rx dma */
#define OTTOEXEC_dma_poke_tx      0xFFFFFF55   /* start tx dma */
#define OTTOEXEC_dma_poke_rpt     0xFFFFFF56   /* start rpt dma */

#define OTTOEXEC_cell_wr_rxbuf    0xFFFFFF82   /* se: dismiss rxready */
#define OTTOEXEC_cell_wr_txvc     0xFFFFFF92
#define OTTOEXEC_cell_wr_txack    0xFFFFFFA2   /* se: dismiss txready */
#define OTTOEXEC_cell_wr_txbuf    0xFFFFFFB2
#define OTTOEXEC_cell_wr_debug    0xFFFFFFC2
#define OTTOEXEC_cell_wr_dmabuf   0xFFFFFFD2   /* se: dismiss needcell */
#define OTTOEXEC_cell_poke_reset  0xFFFFFFE1   /* setup for next dma */
#define OTTOEXEC_cell_poke_rx     0xFFFFFFE4   /* start rx dma */
#define OTTOEXEC_cell_poke_tx     0xFFFFFFE5   /* start tx dma */
#define OTTOEXEC_cell_poke_rx13   0xFFFFFFEC   /* start rx dma, w13 mode */
#define OTTOEXEC_cell_poke_tx13   0xFFFFFFED   /* start tx dma, w13 mode */

#define OTTOEXEC_cell_rd_dmastat  0xFFFFF220   /* also cc := dma stat */
#define OTTOEXEC_cell_rd_rxtxstat 0xFFFFF230   /* also cc := pio stat */
#define OTTOEXEC_cell_rd_rxvc     0xFFFFF240
#define OTTOEXEC_cell_rd_rxack    0xFFFFF250
#define OTTOEXEC_cell_rd_rxbuf    0xFFFFF260
#define OTTOEXEC_cell_rd_debug    0xFFFFF270

#endif
