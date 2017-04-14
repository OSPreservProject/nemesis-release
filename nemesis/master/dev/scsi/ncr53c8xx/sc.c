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
**      dev/scsi/ncr53c8xx
**
** FUNCTIONAL DESCRIPTION:
** 
**      What does it do?
** 
** ENVIRONMENT: 
**
**      Where does it do it?
** 
** FILE :	sc.c
** CREATED :	Thu Jan 16 1997
** AUTHOR :	Paul Barham (Paul.Barham@cl.cam.ac.uk)
** ID : 	$Id: sc.c 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <SCSIController.ih>

#include "fake.h"

extern int ncr_reset_bus (Scsi_Cmnd *);
extern int ncr_abort_command (Scsi_Cmnd *);
extern int ncr53c8xx_queue_command (Scsi_Cmnd *, void (* done)(Scsi_Cmnd *));


#define TRC(_x) _x
#define UNIMPLEMENTED \
printf("%s: UNIMPLEMENTED\n", __FUNCTION__);

/*
 * Module stuff
 */
static	SCSIController_Reset_fn 		SCSIController_Reset_m;
static	SCSIController_Queue_fn 		SCSIController_Queue_m;
static	SCSIController_Abort_fn 		SCSIController_Abort_m;

static	SCSIController_op ms = {
    SCSIController_Reset_m,
    SCSIController_Queue_m,
    SCSIController_Abort_m
};

static  Scsi_Cmnd sc_cmd;
typedef struct Scsi_Host SCSIController_st;


extern struct Scsi_Host **ncr53c8xx_init();

SCSIController_clp *NewController(int *ncontrollers) 
{
    SCSIController_clp  *res;
    struct Scsi_Host   **shs;
    int                  n;

    shs = ncr53c8xx_init(ncontrollers);
    if (ncontrollers == 0) return NULL;

    res = Heap$Malloc(Pvs(heap), 
		      *ncontrollers * sizeof(SCSIController_clp));
    if (res == NULL) {
      printf("NewController; out of memory.\n");
      return NULL;
    }
    
    for (n = 0; n != *ncontrollers; n++) {
      res[n] = Heap$Malloc(Pvs(heap), sizeof(SCSIController_cl));
      if (res[n] == NULL) {
	printf("NewController; out of memory.\n");
	return NULL;
      }

      res[n]->op = &ms;
      res[n]->st = shs[n];
    }
    Heap$Free(Pvs(heap), shs);
    return res;
}

static void done(Scsi_Cmnd *sc)
{
    /* Upcall the higher layers */
    SCSICallback$Done(sc->scb, sc, sc->result);

}

/*---------------------------------------------------- Entry Points ----*/

static void SCSIController_Reset_m(SCSIController_cl *self)
{
    struct Scsi_Host   *host = (struct Scsi_Host *) self->st;
    Scsi_Cmnd           sc;
    
    sc.host = host;
    
    ncr_reset_bus(&sc);
}

static SCSI_ResultCode 
SCSIController_Queue_m (
	SCSIController_cl	*self,
	SCSI_Target             target,
	SCSI_LUN                lun,
	const SCSI_Command	*command,	
	addr_t                   buf,
	uint32_t                 len,
	SCSICallback_clp         scb,
	SCSI_CommHandle       	*h )
{
    struct Scsi_Host   *host = (struct Scsi_Host *) self->st;
    Scsi_Cmnd          *sc;
    SCSI_ResultCode     rc;
    
    sc = &sc_cmd;
    memset(sc, 0, sizeof(*sc));
    
    sc->host                = host;
    sc->target              = target;
    sc->lun                 = lun;
    sc->timeout_per_command = SECONDS(2);
    
    memcpy(&sc->cmnd, SEQ_START(command), SEQ_LEN(command));
    sc->cmd_len = SEQ_LEN(command);
    
    sc->request_buffer  = buf; 
    sc->request_bufflen = len;
    /* XXX  if (len) memset(buf, 0xaa, len); */


    sc->scb             = scb;
    
    rc = ncr53c8xx_queue_command(sc, done);
    
    *h = sc;
    
    return rc;
}

static SCSI_AbortCode 
SCSIController_Abort_m(
    SCSIController_cl	*self,
    SCSI_CommHandle	 h	/* IN */ )
{
/*    struct Scsi_Host   *host = (struct Scsi_Host *) self->st; */
    
    return ncr_abort_command(h);
}

/*
 * End 
 */
