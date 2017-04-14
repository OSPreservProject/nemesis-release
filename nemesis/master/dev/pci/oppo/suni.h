#ifndef SUNI_H
#define SUNI_H

/* Definitions for SUNI interface.

$Id: suni.h 1.1 Thu, 18 Feb 1999 15:09:21 +0000 dr10009 $

$Log: suni.h,v $
Revision 1.1  1997/02/14 14:40:00  nas1007
Initial revision

Revision 1.1  1997/02/12 13:05:04  nas1007
Initial revision

 * Revision 1.1  1995/10/12  11:08:46  prb12
 * Initial revision
 *
 * Revision 1.1  1995/03/01  11:56:40  prb12
 * Initial revision
 *
 * Revision 1.1  1994/06/30  18:24:26  burrows
 * Initial revision
 *
 * Revision 1.1  1994/04/28  03:24:36  tomr
 * Initial revision
 *
 */


    /* SUNI registers */
#define SUNI_MRI    0x00        /* master reset and identity */

#define SUNI_MCONF  0x01        /* master configuration */
#define     SUNI_MCONF_GPINE     0x80   /* RW 0 */
#define     SUNI_MCONF_AUTOFEBE  0x40   /* RW 1 */
#define     SUNI_MCONF_AUTOFERF  0x20   /* RW 1 */
#define     SUNI_MCONF_AUTOYEL   0x10   /* RW 1 */
#define     SUNI_MCONF_TCAINV    0x08   /* RW 0 */
#define     SUNI_MCONF_RCAINV    0x04   /* RW 0 */
#define     SUNI_MCONF_MODE1     0x02   /* RW 0 */
#define     SUNI_MCONF_MODE0     0x01   /* RW 1 */

#define SUNI_MIS    0x02        /* master interrupt status */
#define     SUNI_MIS_GPINV       0x80   /* R  X */
#define     SUNI_MIS_PFERFI      0x40   /* R  X */
#define     SUNI_MIS_GPINI       0x20   /* R  X */
#define     SUNI_MIS_TACPI       0x10   /* R  X */
#define     SUNI_MIS_RACPI       0x08   /* R  X */
#define     SUNI_MIS_RPOPI       0x04   /* R  X */
#define     SUNI_MIS_RLOPI       0x02   /* R  X */
#define     SUNI_MIS_RSOPI       0x01   /* R  X */

#define SUNI_MCM    0x04        /* master clock monitor */
#define     SUNI_MCM_RCLKA       0x02   /* R  X */
#define     SUNI_MCM_POCLKA      0x01   /* R  X */

#define SUNI_MC     0x05        /* master control */
#define     SUNI_MC_PFERFE       0x80   /* RW 0 */
#define     SUNI_MC_PFERFV       0x40   /* R  X */
#define     SUNI_MC_TPFERF       0x20   /* RW 0 */
#define     SUNI_MC_LLE          0x04   /* RW 0 */
#define     SUNI_MC_DLE          0x02   /* RW 0 */
#define     SUNI_MC_LOOPT        0x01   /* RW 0 */

#define SUNI_RSOPCIE 0x10       /* RSOP control/interrupt enable */
#define     SUNI_RSOPCIE_MBZ7    0x80   /* RW 0 */
#define     SUNI_RSOPCIE_DDS     0x40   /* RW 0 */
#define     SUNI_RSOPCIE_FOOF    0x20   /*  W X */
#define     SUNI_RSOPCIE_MBZ4    0x10   /* RW 0 */
#define     SUNI_RSOPCIE_BIPEE   0x08   /* RW 0 */
#define     SUNI_RSOPCIE_LOSE    0x04   /* RW 0 */
#define     SUNI_RSOPCIE_LOFE    0x02   /* RW 0 */
#define     SUNI_RSOPCIE_OOFE    0x01   /* RW 0 */

#define SUNI_RSOPSIS 0x11       /* RSOP status/interrupt status */
#define     SUNI_RSOPSIS_UNU7    0x80   /*    X */
#define     SUNI_RSOPSIS_BIPEI   0x40   /* R  X */
#define     SUNI_RSOPSIS_LOSI    0x20   /* R  X */
#define     SUNI_RSOPSIS_LOFI    0x10   /* R  X */
#define     SUNI_RSOPSIS_OOFI    0x08   /* R  X */
#define     SUNI_RSOPSIS_LOSV    0x04   /* R  X */
#define     SUNI_RSOPSIS_LOFV    0x02   /* R  X */
#define     SUNI_RSOPSIS_OOFV    0x01   /* R  X */

#define SUNI_RSOPSBE0  0x12       /* RSOP section BIP-8 errors byte 0 */
#define SUNI_RSOPSBE1  0x13       /* RSOP section BIP-8 errors byte 1 */

#define SUNI_TSOPC   0x14       /* TSOP control */
#define     SUNI_TSOPC_UNU7      0x80   /*    X */
#define     SUNI_TSOPC_DS        0x40   /* RW 0 */
#define     SUNI_TSOPC_RES5      0x20   /* RW 0 */
#define     SUNI_TSOPC_RES4      0x10   /* RW 0 */
#define     SUNI_TSOPC_RES3      0x08   /* RW 0 */
#define     SUNI_TSOPC_RES2      0x04   /* RW 0 */
#define     SUNI_TSOPC_RES1      0x02   /* RW 0 */
#define     SUNI_TSOPC_LAIS      0x01   /* RW 0 */

#define SUNI_TSOPD   0x15       /* TSOP diagnostic */
#define     SUNI_TSOPD_UNU7      0x80   /*    X */
#define     SUNI_TSOPD_UNU6      0x40   /*    X */
#define     SUNI_TSOPD_UNU5      0x20   /*    X */
#define     SUNI_TSOPD_UNU4      0x10   /*    X */
#define     SUNI_TSOPD_UNU3      0x08   /*    X */
#define     SUNI_TSOPD_DLOS      0x04   /* RW 0 */
#define     SUNI_TSOPD_DBIP8     0x02   /* RW 0 */
#define     SUNI_TSOPD_DFP       0x01   /* RW 0 */

#define SUNI_RLOPCS  0x18       /* RLOP control/status */
#define     SUNI_RLOPCS_RES7     0x80   /* RW 0 */
#define     SUNI_RLOPCS_RES6     0x40   /* RW 0 */
#define     SUNI_RLOPCS_RES5     0x20   /* RW 0 */
#define     SUNI_RLOPCS_RES4     0x10   /* RW 0 */
#define     SUNI_RLOPCS_RES3     0x08   /* RW 0 */
#define     SUNI_RLOPCS_UNU2     0x04   /*    X */
#define     SUNI_RLOPCS_LAISV    0x02   /* R  0 */
#define     SUNI_RLOPCS_FERFV    0x01   /* R  0 */

#define SUNI_RLOPIES 0x19       /* RLOP interrupt enable/interrupt status */
#define     SUNI_RLOPIES_FEBEE   0x80   /* RW 0 */
#define     SUNI_RLOPIES_BIPEE   0x40   /* RW 0 */
#define     SUNI_RLOPIES_LAISE   0x20   /* RW 0 */
#define     SUNI_RLOPIES_FERFE   0x10   /* RW 0 */
#define     SUNI_RLOPIES_FEBEI   0x08   /* R  X */
#define     SUNI_RLOPIES_BIPEI   0x04   /* R  X */
#define     SUNI_RLOPIES_LAISI   0x02   /* R  X */
#define     SUNI_RLOPIES_FERFI   0x01   /* R  X */

#define SUNI_RLOPLBE0  0x1a     /* RLOP line BIP-24 errors byte 0 */
#define SUNI_RLOPLBE1  0x1b     /* RLOP line BIP-24 errors byte 1 */
#define SUNI_RLOPLBE2  0x1c     /* RLOP line BIP-24 errors byte 2 */
                                      /* top 4 bits of byte 2 are X */

#define SUNI_RLOPLFE0  0x1d     /* RLOP line FEBE errors byte 0 */
#define SUNI_RLOPLFE1  0x1e     /* RLOP line FEBE errors byte 1 */
#define SUNI_RLOPLFE2  0x1f     /* RLOP line FEBE errors byte 2 */
                                      /* top 4 bits of byte 2 are X */

#define SUNI_TLOPC   0x20       /* TLOP control */
#define     SUNI_TLOPC_UNU7      0x80   /*    X */
#define     SUNI_TLOPC_RES6      0x40   /* RW 0 */
#define     SUNI_TLOPC_RES5      0x20   /* RW 0 */
#define     SUNI_TLOPC_RES4      0x10   /* RW 0 */
#define     SUNI_TLOPC_RES3      0x08   /* RW 0 */
#define     SUNI_TLOPC_RES2      0x04   /* RW 0 */
#define     SUNI_TLOPC_RES1      0x02   /* RW 0 */
#define     SUNI_TLOPC_FERF      0x01   /* RW 0 */

#define SUNI_TLOPD   0x21       /* TLOP diagnostic */
#define     SUNI_TLOPD_UNU7      0x80   /*    X */
#define     SUNI_TLOPD_UNU6      0x40   /*    X */
#define     SUNI_TLOPD_UNU5      0x20   /*    X */
#define     SUNI_TLOPD_UNU4      0x10   /*    X */
#define     SUNI_TLOPD_UNU3      0x08   /*    X */
#define     SUNI_TLOPD_UNU2      0x04   /*    X */
#define     SUNI_TLOPD_UNU1      0x02   /*    X */
#define     SUNI_TLOPD_DBIP24    0x01   /* RW 0 */

#define SUNI_RPOPSC  0x30       /* RPOP status/control */
#define     SUNI_RPOPSC_RES7     0x80   /* RW 0 */
#define     SUNI_RPOPSC_UNU6     0x40   /*    X */
#define     SUNI_RPOPSC_LOP      0x20   /* R  X */
#define     SUNI_RPOPSC_UNU4     0x10   /*    X */
#define     SUNI_RPOPSC_PAIS     0x08   /* R  X */
#define     SUNI_RPOPSC_PYEL     0x04   /* R  X */
#define     SUNI_RPOPSC_UNU1     0x02   /*    X */
#define     SUNI_RPOPSC_RES0     0x01   /* RW 0 */

#define SUNI_RPOPIS  0x31       /* RPOP interrupt status */
#define     SUNI_RPOPIS_PLSI     0x80   /* R  X */
#define     SUNI_RPOPIS_UNU6     0x40   /*    X */
#define     SUNI_RPOPIS_LOPI     0x20   /* R  X */
#define     SUNI_RPOPIS_UNU4     0x10   /*    X */
#define     SUNI_RPOPIS_PAISI    0x08   /* R  X */
#define     SUNI_RPOPIS_PYELI    0x04   /* R  X */
#define     SUNI_RPOPIS_BPIEI    0x02   /* R  X */
#define     SUNI_RPOPIS_FEBEI    0x01   /* R  X */

#define SUNI_RPOPIE  0x33       /* RPOP interrupt enable */
#define     SUNI_RPOPIE_PLSE     0x80   /* RW 0 */
#define     SUNI_RPOPIE_RES6     0x40   /* RW 0 */
#define     SUNI_RPOPIE_LOPE     0x20   /* RW 0 */
#define     SUNI_RPOPIE_RES4     0x10   /* RW 0 */
#define     SUNI_RPOPIE_PAISE    0x08   /* RW 0 */
#define     SUNI_RPOPIE_PYELE    0x04   /* RW 0 */
#define     SUNI_RPOPIE_BPIEE    0x02   /* RW 0 */
#define     SUNI_RPOPIE_FEBEE    0x01   /* RW 0 */

#define SUNI_RPOPPSL 0x37       /* RPOP path signal label */

#define SUNI_LM       0x38      /* load meters (alias to RPOPPBE0) */

#define SUNI_RPOPPBE0 0x38      /* RPOP path BIP-8 errors byte 0 */
#define SUNI_RPOPPBE1 0x39      /* RPOP path BIP-8 errors byte 1 */

#define SUNI_RPOPPFE0 0x3a      /* RPOP path FEBE errors byte 0 */
#define SUNI_RPOPPFE1 0x3b      /* RPOP path FEBE errors byte 1 */

#define SUNI_TPOPCS  0x40       /* TPOP control/status */
#define     SUNI_TPOPCS_RES7     0x80   /* RW 0 */
#define     SUNI_TPOPCS_UNU6     0x40   /*    X */
#define     SUNI_TPOPCS_UNU5     0x20   /*    X */
#define     SUNI_TPOPCS_UNU4     0x10   /*    X */
#define     SUNI_TPOPCS_UNU3     0x08   /*    X */
#define     SUNI_TPOPCS_RES2     0x04   /* RW 0 */
#define     SUNI_TPOPCS_DB3      0x02   /* RW 0 */
#define     SUNI_TPOPCS_PAIS     0x01   /* RW 0 */

#define SUNI_TPOPPC  0x41       /* TPOP pointer control */
#define     SUNI_TPOPPC_UNU7     0x80   /*    X */
#define     SUNI_TPOPPC_UNU6     0x40   /*    X */
#define     SUNI_TPOPPC_SOS      0x20   /* RW 0 */
#define     SUNI_TPOPPC_PLD      0x10   /* RW 0 */
#define     SUNI_TPOPPC_NDF      0x08   /* RW 0 */
#define     SUNI_TPOPPC_NSE      0x04   /* RW 0 */
#define     SUNI_TPOPPC_PSE      0x02   /* RW 0 */
#define     SUNI_TPOPPC_RES0     0x01   /* RW 0 */

#define SUNI_TPOPSC  0x42       /* TPOP source control */
#define     SUNI_TPOPSC_UNU7     0x80   /*    X */
#define     SUNI_TPOPSC_SRCZ5    0x40   /* RW 0 */
#define     SUNI_TPOPSC_SRCZ4    0x20   /* RW 0 */
#define     SUNI_TPOPSC_SRCZ3    0x10   /* RW 0 */
#define     SUNI_TPOPSC_SRCC2    0x08   /* RW 0 */
#define     SUNI_TPOPSC_SRCG1    0x04   /* RW 0 */
#define     SUNI_TPOPSC_SRCF2    0x02   /* RW 0 */
#define     SUNI_TPOPSC_SRCJ1    0x01   /* RW 0 */

#define SUNI_TPOPAP0 0x45       /* TPOP arbitrary pointer byte 0 */
#define SUNI_TPOPAP1 0x46       /* TPOP arbitrary pointer byte 1 */
                                    /* byte 1 also has NDF and S */

#define SUNI_TPOPPS  0x49       /* TPOP path status */
#define     SUNI_TPOPPS_FEBE3    0x80   /* RW 0 */
#define     SUNI_TPOPPS_FEBE2    0x40   /* RW 0 */
#define     SUNI_TPOPPS_FEBE1    0x20   /* RW 0 */
#define     SUNI_TPOPPS_FEBE0    0x10   /* RW 0 */
#define     SUNI_TPOPPS_PYEL     0x08   /* RW 0 */
#define     SUNI_TPOPPS_G12      0x04   /* RW 0 */
#define     SUNI_TPOPPS_G11      0x02   /* RW 0 */
#define     SUNI_TPOPPS_G10      0x01   /* RW 0 */

#define SUNI_RACPCS  0x50       /* RACP control/status */
#define     SUNI_RACPCS_OOCDV    0x80   /* R  X */
#define     SUNI_RACPCS_UNU6     0x40   /*    X */
#define     SUNI_RACPCS_PASS     0x20   /* RW 0 */
#define     SUNI_RACPCS_DISCOR   0x10   /* RW 0 */
#define     SUNI_RACPCS_HCSPASS  0x08   /* RW 0 */
#define     SUNI_RACPCS_HCSADD   0x04   /* RW 1 */
#define     SUNI_RACPCS_DDSCR    0x02   /* RW 0 */
#define     SUNI_RACPCS_FIFORST  0x01   /* RW 0 */

#define SUNI_RACPIES 0x51       /* RACP interrupt enable/status */
#define     SUNI_RACPIES_OOCDE   0x80   /* RW 0 */
#define     SUNI_RACPIES_HCSE    0x40   /* RW 0 */
#define     SUNI_RACPIES_FIFOE   0x20   /* RW 0 */
#define     SUNI_RACPIES_OOCDI   0x10   /* R  X */
#define     SUNI_RACPIES_CHCSI   0x08   /* R  X */
#define     SUNI_RACPIES_UHCSI   0x04   /* R  X */
#define     SUNI_RACPIES_FOVRI   0x02   /* R  X */
#define     SUNI_RACPIES_FUDRI   0x01   /* R  X */

#define SUNI_RACPMHP 0x52       /* RACP match header pattern */
#define     SUNI_RACPMHP_GFC3    0x80   /* RW 0 */
#define     SUNI_RACPMHP_GFC2    0x40   /* RW 0 */
#define     SUNI_RACPMHP_GFC1    0x20   /* RW 0 */
#define     SUNI_RACPMHP_GFC0    0x10   /* RW 0 */
#define     SUNI_RACPMHP_PTI2    0x08   /* RW 0 */
#define     SUNI_RACPMHP_PTI1    0x04   /* RW 0 */
#define     SUNI_RACPMHP_PTI0    0x02   /* RW 0 */
#define     SUNI_RACPMHP_CLP     0x01   /* RW 0 */

#define SUNI_RACPMHM 0x53       /* RACP match header mask */
#define     SUNI_RACPMHM_MGFC3   0x80   /* RW 0 */
#define     SUNI_RACPMHM_MGFC2   0x40   /* RW 0 */
#define     SUNI_RACPMHM_MGFC1   0x20   /* RW 0 */
#define     SUNI_RACPMHM_MGFC0   0x10   /* RW 0 */
#define     SUNI_RACPMHM_MPTI2   0x08   /* RW 0 */
#define     SUNI_RACPMHM_MPTI1   0x04   /* RW 0 */
#define     SUNI_RACPMHM_MPTI0   0x02   /* RW 0 */
#define     SUNI_RACPMHM_MCLP    0x01   /* RW 0 */

#define SUNI_RACPCHCS 0x54      /* RACP correctable HCS error count */
#define SUNI_RACPUHCS 0x55      /* RACP uncorrectable HCS error count */

#define SUNI_TACPCS  0x60       /* TACP control/status */
#define     SUNI_TACPCS_FIFOE    0x80   /* RW 0 */
#define     SUNI_TACPCS_TSOCI    0x40   /* R  X */
#define     SUNI_TACPCS_FOVRI    0x20   /* R  X */
#define     SUNI_TACPCS_DHCS     0x10   /* RW 0 */
#define     SUNI_TACPCS_HCS      0x08   /* RW 0 */
#define     SUNI_TACPCS_HCSADD   0x04   /* RW 1 */
#define     SUNI_TACPCS_DSCR     0x02   /* RW 0 */
#define     SUNI_TACPCS_FIFORST  0x01   /* RW 0 */

#define SUNI_TACPIHP 0x61       /* TACP idle/unassigned cell header pattern */
#define     SUNI_TACPIHP_GFC3    0x80   /* RW 0 */
#define     SUNI_TACPIHP_GFC2    0x40   /* RW 0 */
#define     SUNI_TACPIHP_GFC1    0x20   /* RW 0 */
#define     SUNI_TACPIHP_GFC0    0x10   /* RW 0 */
#define     SUNI_TACPIHP_PTI2    0x08   /* RW 0 */
#define     SUNI_TACPIHP_PTI1    0x04   /* RW 0 */
#define     SUNI_TACPIHP_PTI0    0x02   /* RW 0 */
#define     SUNI_TACPIHP_CLP     0x01   /* RW 0 */

#define SUNI_TACPICP 0x62       /* TACP idle/unassigned cell payload pattern */



#endif /* SUNI_H */
