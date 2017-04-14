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
**      ./dev/scsi/ncr53c8xx/
**
** FUNCTIONAL DESCRIPTION:
** 
**      Controller initialisation and device detection code.
** 
** ENVIRONMENT: 
**
**      Device drive domain.
** 
** FILE         : main.c
** CREATED      : Mon Mar 31 1997
** AUTHOR       : Steven Hand (Steven.Hand@cl.cam.ac.uk)
** ORGANISATION : University of Cambridge Computer Laboratory. 
** ID : 	$Id: main.c 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>
#include <time.h>
#include <ecs.h>
#include <dev/scsi.h>

#include <IDCMacros.h>

#include <SCSIController.ih>
#include "fake.h"

#include <autoconf/scsi_disk.h>
#ifdef CONFIG_SCSI_DISK
#include <SCSIDisk.ih>
#include <Disk.ih>
#include <USDMod.ih>
#endif

/* #define PROBE_TRACE */
/* #define DEBUG */

/* produce some output during the scsi probing phase */
#define PROBE_TRACE
#ifdef PROBE_TRACE
#define PTRC(_x) _x
#else
#define PTRC(_x)
#endif

/* produce lots more output during the probe phase */
#ifdef EXTRA_PROBE_TRACE
#define PTRC2(_x) _x
#else
#define PTRC2(_x)
#endif

/* general debugging output */
// #define DEBUG
#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif
#define DB(_x)  _x

#define UNIMPLEMENTED  printf("%s: UNIMPLEMENTED\n", __FUNCTION__);


extern SCSIController_clp *NewController(int *ncontrollers);


static SCSICallback_Done_fn Done;
static SCSICallback_op scb_ms = {  
    Done 
};

static SCSICallback_cl scb_cl = { &scb_ms, NULL };

static SCSI_CommHandle handle;
static SCSI_ResultCode the_rc;
static Event_Count     the_ec;
static union scsi_cmd  the_cmd; 

#define INQ_LEN 96
static unsigned char inqbuf[INQ_LEN];


static void Done(SCSICallback_clp self,
		 SCSI_CommHandle  handle,
		 SCSI_ResultCode  rc)
{
/*     Scsi_Cmnd *sc = (Scsi_Cmnd *)handle; */
    
    /* 
    ** The result value has two parts: the top 16 bits are the 
    ** 'host code', i.e. one of DID_xxx (defined in fake.h).
    ** The low 7 bits hold the 'scsi code', which is one
    ** of the S_xxx (defined in ncr53c8xx.h). 
    ** We only really care about the host code here 
    */
    the_rc = rc;
    EC_ADVANCE(the_ec, 1);
}


static bool_t inquire(SCSIController_clp controller, int target, int lun)
{
    union scsi_cmd *cmd = &the_cmd;
    char *buffer = (char *)(&inqbuf[0]);
    SCSI_Command command;
    SCSI_ResultCode rc;

    memset(buffer, 0, sizeof(int));

    memset(cmd, 0, sizeof(*cmd));
    cmd->generic.opcode  = INQUIRY;
    cmd->inquiry.length  = INQ_LEN;

    command.base = (uint8_t *)cmd;
    command.data = (uint8_t *)cmd;
    command.len  = sizeof(struct scsi_inquiry);

    rc = SCSIController$Queue(controller, target, lun, 
			      &command, buffer, INQ_LEN, 
			      &scb_cl, &handle);

    if(rc != DID_OK) 
	return False;

    return True;
}


PUBLIC void Main (Closure_clp self)
{
    Event_Val           val;
    int                 i, j, ncontrollers, n;
#ifdef CONFIG_SCSI_DISK
    Disk_clp            disk[8];
    int                 ndisks = 0;
#endif
    SCSIController_clp *controllers;

    Pvs(out) = Pvs(err); 
    TRC(printf("ncr53c8xx: entered.\n"));
    
    /* First get hold of a SCSIController_clp */
    controllers = NewController(&ncontrollers);
    if (ncontrollers == 0) {
	printf("ncr53c8xx: controller initialisation failed.\n");
	DB(printf("ncr53c8xx: controller initialisation failed.\n"));
	PAUSE(FOREVER);
    }

    /* SMH: now probe the bus to see what we have */
    PTRC(printf("ncr53c8xx: probing scsi bus\n"));
    the_ec = EC_NEW();

    for (n = 0; n != ncontrollers; n++) {
      /* 
      ** XXX SMH: we probably should probe all targets and luns, as given
      ** by MAX_TARGET and MAX_LUN. However, we only really probe at all
      ** currently to find the target of our quantum hard disk ;-)
      ** We can't even do anything much with other devices if we find 'em.
      ** So for now we just do targets 0..7 and lun 0. If we ever have
      ** more SCSI devices, then we can just update the below.
      */
      for(i=0; i < 8; i++) { 
	for(j=0; j < 1; j++) {

	    /* XXX Synchronous */
	    val = EC_READ(the_ec);
	    
	    if(!inquire(controllers[n], i, j)) {
		PTRC2(printf(
		    "INQUIRY failed queue: controller %p, target %d, lun %d\n",
		    controllers[n], i, j));
		continue;
	    } else {
		EC_AWAIT(the_ec, val+1);

		if((the_rc>>16) == DID_OK) {
		    inqbuf[15] = inqbuf[31] = inqbuf[36] = 0;
#define TYPE_DISK           0x00
#define TYPE_TAPE           0x01
#define TYPE_PROCESSOR      0x03    /* HP scanners use this */
#define TYPE_WORM           0x04    /* Treated as ROM by our system */
#define TYPE_ROM            0x05
#define TYPE_SCANNER        0x06
#define TYPE_MOD            0x07    /* Magneto-optical disk - 
                                     * - treated as TYPE_DISK */
#define TYPE_NO_LUN         0x7f
		    switch(inqbuf[0]&0xFF) {
		    case TYPE_MOD: 
		    case TYPE_DISK:
			PTRC(printf(
			    "+ disk   [%d,%d,%d]: vendor %s model %s rev %s\n",
			    n, i, j, inqbuf+8, inqbuf+16, inqbuf+32));
#ifdef CONFIG_SCSI_DISK 
			{
			    SCSIDisk_cl *sd;
			    
			    sd = NAME_FIND("modules>SCSIDisk", SCSIDisk_clp);
			    disk[ndisks++]= SCSIDisk$New(sd, controllers[n],
							 i, j);
			}
#endif
			break;
			
		    case TYPE_TAPE:
			PTRC(printf(
			    "+ tape   [%d,%d,%d]: vendor %s model %s rev %s\n",
			    n, i, j, inqbuf+8, inqbuf+16, inqbuf+32));
#ifdef CONFIG_SCSI_TAPE
#endif
			break;
			
		    case TYPE_ROM:
			PTRC(printf(
			    "+ cd-rom [%d,%d,%d]: vendor %s model %s rev %s\n",
			    n, i, j, inqbuf+8, inqbuf+16, inqbuf+32));
#ifdef CONFIG_SCSI_CDROM
#endif
			break;
			
		    case TYPE_PROCESSOR:
		    case TYPE_SCANNER:
			PTRC(printf(
			    "+ scanner[%d,%d,%d]: vendor %s model %s rev %s\n",
			    n, i, j, inqbuf+8, inqbuf+16, inqbuf+32));
			break;
			
		    case TYPE_WORM: 
			PTRC(printf(
			    "+ worm   [%d,%d,%d]: vendor %s model %s rev %s\n",
			    n, i, j, inqbuf+8, inqbuf+16, inqbuf+32));
			break;
			
		    default:
			PTRC2(printf(
			    "XXX unknown devive type at [%d,%d,%d]\n",
			    n, i, j));
			break;
		    }
		} else PTRC2(printf(
		    "Inquiry failed => no device [%d,%d,%d]\n",
		    n, i, j));
		
	    }
	}
      }
    }

#ifdef CONFIG_SCSI_DISK    
    if(ndisks) {
	USDMod_clp   usdmod = NULL;
	USDDrive_clp usd;

	usdmod = NAME_FIND("modules>USDMod", USDMod_clp);
	if (usdmod) {
	    char name[32];
	    for(i=0; i < ndisks; i++) {

		TRC(printf("ncr53c8xx: creating USD for disk %d\n", i));
		sprintf(name,"dev>sd%c",'a'+i);
		usd = USDMod$New(usdmod, disk[i], name);
		IDC_EXPORT(name,USDDrive_clp,usd);

	    }
	} else {
	    TRC(printf("ncr53c8xx: couldn't find USDMod\n"));
	}
    } else 
	TRC(printf("ncr53c8xx: no disks found.\n"));
#endif

    TRC(printf("ncr53c8xx: done\n"));
}
