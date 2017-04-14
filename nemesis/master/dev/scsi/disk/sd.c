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
**      dev/scsi/disk
**
** FUNCTIONAL DESCRIPTION:
** 
**      SCSI Disk specific operations
** 
** ENVIRONMENT: 
**
**      Sits on top of Generic SCSI / SCSI Controller interfaces.
** 
** FILE :	sd.c
** CREATED :	Thu Jan 16 1997
** AUTHOR :	Paul Barham (Paul.Barham@cl.cam.ac.uk)
** ID : 	$Id: sd.c 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <bstring.h>
#include <stdio.h>
#include <time.h>
#include <ecs.h>

#include <dev/scsi.h>

#include <SCSIController.ih>
#include <SCSICallback.ih>
#include <SCSIDisk.ih>

// #define __IO_INLINE__
#include <io.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED", __FUNCTION__);

typedef struct {
    Disk_cl            cl;
    Disk_Params        params;
    char               name[8];

    SCSIController_clp controller;
    SCSI_Target        target;
    SCSI_LUN           lun;

    SCSI_Command       command;
    union scsi_cmd     data;
    SCSI_CommHandle    handle;

    SCSICallback_cl    scb;
    Event_Count        ec;

    SCSI_ResultCode    rc;
} Disk_st;


static const Disk_Params default_params = 
{
    512,			/* uint32_t blksz */
    1<<20,			/* BlockDev_Block blocks */
    2048,			/* Disk_Cylinder cyls */
    8,				/* Disk_Head heads */
    MILLISECS(8),		/* Time_ns seek */
    MILLISECS(11),		/* Time_ns rotate */
    MILLISECS(2),		/* Time_ns headsw */
    MICROSECS(155)		/* Time_ns xfr */
};

static	BlockDev_BlockSize_fn 		Disk_BlockSize_m;
static	BlockDev_Read_fn 		Disk_Read_m;
static	BlockDev_Write_fn 		Disk_Write_m;
static	Disk_GetParams_fn 		Disk_GetParams_m;
static	Disk_Translate_fn 		Disk_Translate_m;
static	Disk_Where_fn 		        Disk_Where_m;
static	Disk_ReadTime_fn 		Disk_ReadTime_m;
static	Disk_WriteTime_fn 		Disk_WriteTime_m;

static	Disk_op	disk_ms = {
  Disk_BlockSize_m,
  Disk_Read_m,
  Disk_Write_m,
  Disk_GetParams_m,
  Disk_Translate_m,
  Disk_Where_m,
  Disk_ReadTime_m,
  Disk_WriteTime_m
};

static SCSICallback_Done_fn Done;
static SCSICallback_op scb_ms = {
    Done
};


/*
 * Module stuff
 */

static	SCSIDisk_New_fn 		SCSIDisk_New_m;
static	SCSIDisk_op	ms = {
    SCSIDisk_New_m
};
static	const SCSIDisk_cl	cl = { &ms, NULL };
CL_EXPORT (SCSIDisk, SCSIDisk, cl);


/*------------------------------------------------------------------*/

static void start(Disk_clp self)
{
    Disk_st	   *st = (Disk_st *) self->st;
    union scsi_cmd *cmd;
    SCSI_ResultCode rc;
    Event_Val       val;
    
    cmd = &st->data;
    
    TRC(printf("START...\n"));
    SEQ_LEN(&st->command) = sizeof(struct scsi_test_unit_ready);
    bzero(cmd, sizeof(*cmd));
    cmd->generic.opcode  = LOAD_UNLOAD;
    cmd->load.load       = LD_LOAD;
    cmd->load.immed      = 1;
    
    /* XXX Synchronous */
    val = EC_READ(st->ec);
    rc = SCSIController$Queue(st->controller, st->target, st->lun, 
			      &st->command, NULL, 0, 
			      &st->scb, &st->handle);
    EC_AWAIT(st->ec, val+1);
}

static void test_unit_ready(Disk_clp self)
{
    Disk_st	   *st = (Disk_st *) self->st;
    union scsi_cmd *cmd;
    SCSI_ResultCode rc;
    Event_Val       val;
    
    cmd = &st->data;
    
    TRC(printf("TEST_UNIT_READY...\n"));
    SEQ_LEN(&st->command) = sizeof(struct scsi_test_unit_ready);
    bzero(cmd, sizeof(*cmd));
    cmd->generic.opcode  = TEST_UNIT_READY;
    
    /* XXX Synchronous */
    val = EC_READ(st->ec);
    rc  = SCSIController$Queue(st->controller, st->target, st->lun, 
			       &st->command, NULL, 0, 
			       &st->scb, &st->handle);
    EC_AWAIT(st->ec, val+1);
}

static void inquire(Disk_clp self)
{
    Disk_st	   *st = (Disk_st *) self->st;
    union scsi_cmd *cmd;
    SCSI_ResultCode rc;
    Event_Val       val;
    
    unsigned char   inqbuf[96];
    char           *buffer = inqbuf;

    cmd = &st->data;
    
    TRC(printf("INQUIRY...\n"));
    
    SEQ_LEN(&st->command) = sizeof(struct scsi_inquiry);
    bzero(cmd, sizeof(*cmd));
    cmd->generic.opcode  = INQUIRY;
    cmd->inquiry.length  = 96;
    
    /* XXX Synchronous */
    val = EC_READ(st->ec);
    rc  = SCSIController$Queue(st->controller, st->target, st->lun, 
			       &st->command, buffer, 96, 
			       &st->scb, &st->handle);
    EC_AWAIT(st->ec, val+1);
    
/*
 *  DEVICE TYPES
 */
    
#define TYPE_DISK           0x00
#define TYPE_TAPE           0x01
#define TYPE_PROCESSOR      0x03    /* HP scanners use this */
#define TYPE_WORM           0x04    /* Treated as ROM by our system */
#define TYPE_ROM            0x05
#define TYPE_SCANNER        0x06
#define TYPE_MOD            0x07    /* Magneto-optical disk - 
                                     * - treated as TYPE_DISK */
#define TYPE_NO_LUN         0x7f

    buffer[15] = buffer[31] = buffer[36] = 0;
    TRC(printf("type %d  vendor %s  model %s  rev %s\n",
	       buffer[0]& 0x1f, buffer+8, buffer+16, buffer+32));
    
}


/*---------------------------------------------------- Entry Points ----*/

static Disk_clp SCSIDisk_New_m (
	SCSIDisk_cl	       *self,
	SCSIController_clp	controller	/* IN */,
	SCSI_Target	        target	        /* IN */,
	SCSI_LUN	        lun	        /* IN */ )
{
    Disk_st  *st;
    Disk_clp  disk;

    TRC(printf("SCSIDisk$New: controller=%p : target,lun [%d,%d]\n", 
	   controller, target, lun));

    st = Heap$Malloc(Pvs(heap), sizeof(Disk_st));
    if(!st) { 
	eprintf("SCSDisk$New: out of memory.\n");
	ntsc_dbgstop();
    }
    st->controller = controller;
    st->target     = target;
    st->lun        = lun;

    bcopy(&default_params, &st->params, sizeof(Disk_Params));

    st->ec = EC_NEW();
    st->command.base = (uint8_t *) &st->data;
    st->command.data = (uint8_t *) &st->data;

    sprintf(st->name, "sd%c\0", 'a' + target);

    CL_INIT(st->scb, &scb_ms, st);
    CL_INIT(st->cl, &disk_ms, st);
    disk = &st->cl;

    TRC(printf("SCSIDisk$New: created %s\n", st->name));

    test_unit_ready(disk);
    test_unit_ready(disk);
    inquire(disk);
    test_unit_ready(disk);
    start(disk);
    test_unit_ready(disk);

    TRC(printf("SCSIDisk$New: returning Disk_clp at %p\n", disk));
    return disk;
}


static void Done(SCSICallback_clp self,
		 SCSI_CommHandle  handle,
		 SCSI_ResultCode  rc)
{
    Disk_st	*st = (Disk_st *) self->st;

    /* XXX this is a bit simplistic */
    EC_ADVANCE(st->ec, 1);
}

/*---------------------------------------------------- Entry Points ----*/

static uint32_t Disk_BlockSize_m (BlockDev_cl	*self )
{
    Disk_st	*st = (Disk_st *) self->st;

    return st->params.blksz;
}


static bool_t Disk_Read_m (BlockDev_cl	       *self,
			   BlockDev_Block	blkno	/* IN */,
			   uint32_t             nblks	/* IN */,
			   addr_t	        buf	/* IN */ )
{
    Disk_st	   *st = (Disk_st *) self->st;
    union scsi_cmd *cmd;
    SCSI_ResultCode rc;
    Event_Val       val;

    TRC(printf("READ %d\n", blkno));

#ifdef CONFIG_MEMSYS_EXPT
    {
	word_t start_va, last_va; 
	word_t start_pa, last_pa;
	word_t buflen; 

	start_va = (word_t)buf; 
	buflen   = nblks * st->params.blksz; 
        last_va  = (word_t)((void *)buf + buflen - 1);

	if((addr_t)(start_pa = (word_t)virt_to_phys(start_va)) == NO_ADDRESS) {
	    word_t pte; 
	    eprintf("BlockDev$Read: blkno=%d, nblks=%d, buf at VA [%p, %p)\n", 
		    blkno, nblks, start_va, last_va);
	    eprintf("start va %p is bad (TNV or PAGE)\n", start_va);
	    pte = ntsc_trans(start_va >> PAGE_WIDTH); 
	    eprintf("trans of start va=%p gives pte = %lx\n", start_va, pte);
	    ntsc_dbgstop();
	}
	if((addr_t)(last_pa = (word_t)virt_to_phys(last_va)) == NO_ADDRESS) {
	    word_t pte; 
	    eprintf("BlockDev$Read: blkno=%d, nblks=%d, buf at VA [%p, %p)\n", 
		    blkno, nblks, start_va, last_va);
	    eprintf("last va %p is bad (TNV or PAGE)\n", last_va);
	    pte = ntsc_trans(last_va >> PAGE_WIDTH); 
	    eprintf("trans of  last va=%p gives pte = %lx\n", last_va, pte);
	    ntsc_dbgstop();
	}

	if(last_pa != (start_pa + buflen - 1)) {
	    eprintf("BlockDev$Read: blkno=%d, nblks=%d, buf at VA [%p, %p)\n", 
		    blkno, nblks, start_va, last_va);
	    eprintf("                                 buf at PA [%p, %p)\n", 
		    start_pa, last_pa);
	    eprintf("=> not contiguous. Death.\n");
	    ntsc_dbgstop();
	}
    }
#endif

    cmd = &st->data;
    SEQ_LEN(&st->command) = sizeof(struct scsi_rw_big);

    bzero(cmd, sizeof(*cmd));
    cmd->generic.opcode  = READ_BIG;
    cmd->rw_big.addr_3   =  (blkno & 0xff000000) >> 24;
    cmd->rw_big.addr_2   =  (blkno & 0xff0000) >> 16;
    cmd->rw_big.addr_1   =  (blkno & 0xff00) >> 8;
    cmd->rw_big.addr_0   =  blkno & 0xff;
    cmd->rw_big.length2  =  (nblks & 0xff00) >> 8;
    cmd->rw_big.length1  =  (nblks & 0xff);

    /* XXX Synchronous */
    val = EC_READ(st->ec);
    rc = SCSIController$Queue(st->controller, st->target, st->lun, 
			      &st->command, buf, nblks * st->params.blksz, 
			      &st->scb, &st->handle);
    EC_AWAIT(st->ec, val+1);

    TRC({
	int i;
	unsigned char *p = buf;
	for (i = 0; i < 512; i+=16) {
	    printf("%03x: %02x %02x %02x %02x %02x %02x %02x %02x"
		         "%02x %02x %02x %02x %02x %02x %02x %02x\n",
		   i,
		   p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],
		   p[8],p[9],p[10],p[11],p[12],p[13],p[14],p[15]);
	    p+=16;
	}
    });

    return True;
}

static bool_t Disk_Write_m (BlockDev_cl		*self,
			    BlockDev_Block	blkno	/* IN */,
			    uint32_t            nblks	/* IN */,
			    addr_t	        buf	/* IN */ )
{
    Disk_st	   *st = (Disk_st *) self->st;
    union scsi_cmd *cmd;
    SCSI_ResultCode rc;
    Event_Val       val;
    
    TRC(printf("WRITE %d\n", blkno));
  
#ifdef CONFIG_MEMSYS_EXPT
    {
	word_t start_va, last_va; 
	word_t start_pa, last_pa;
	word_t buflen; 

	start_va = (word_t)buf; 
	buflen   = nblks * st->params.blksz; 
        last_va  = (word_t)((void *)buf + buflen - 1);

	if((addr_t)(start_pa = (word_t)virt_to_phys(start_va)) == NO_ADDRESS) {
	    word_t pte; 
	    eprintf("BlockDev$Write: blkno=%d nblks=%d, buf at VA [%p, %p)\n", 
		    blkno, nblks, start_va, last_va);
	    eprintf("start va %p is bad (TNV or PAGE)\n", start_va);
	    pte = ntsc_trans(start_va >> PAGE_WIDTH); 
	    eprintf("trans of start va=%p gives pte = %lx\n", start_va, pte);
	    ntsc_dbgstop();
	}
	if((addr_t)(last_pa = (word_t)virt_to_phys(last_va)) == NO_ADDRESS) {
	    word_t pte; 
	    eprintf("BlockDev$Write: blkno=%d nblks=%d, buf at VA [%p, %p)\n", 
		    blkno, nblks, start_va, last_va);
	    eprintf("last va %p is bad (TNV or PAGE)\n", last_va);
	    pte = ntsc_trans(last_va >> PAGE_WIDTH); 
	    eprintf("trans of  last va=%p gives pte = %lx\n", last_va, pte);
	    ntsc_dbgstop();
	}

	if(last_pa != (start_pa + buflen - 1)) {
	    eprintf("BlockDev$Write: blkno=%d, nblks=%d buf at VA [%p, %p)\n", 
		    blkno, nblks, start_va, last_va);
	    eprintf("                                 buf at PA [%p, %p)\n", 
		    start_pa, last_pa);
	    eprintf("=> not contiguous. Death.\n");
	    ntsc_dbgstop();
	}
    }
#endif

    cmd = &st->data;
    SEQ_LEN(&st->command) = sizeof(struct scsi_rw_big);
    
    bzero(cmd, sizeof(*cmd));
    cmd->generic.opcode  =  WRITE_BIG;
    cmd->rw_big.addr_3   =  (blkno & 0xff000000) >> 24;
    cmd->rw_big.addr_2   =  (blkno & 0xff0000) >> 16;
    cmd->rw_big.addr_1   =  (blkno & 0xff00) >> 8;
    cmd->rw_big.addr_0   =  blkno & 0xff;
    cmd->rw_big.length2  =  (nblks & 0xff00) >> 8;
    cmd->rw_big.length1  =  (nblks & 0xff);
    
    /* XXX Synchronous */
    val = EC_READ(st->ec);
    rc  = SCSIController$Queue(st->controller, st->target, st->lun, 
	 		      &st->command, buf, nblks * st->params.blksz, 
		 	      &st->scb, &st->handle);
    EC_AWAIT(st->ec, val+1);

    return True;
}

static void 
Disk_GetParams_m (
	Disk_cl	        *self,
	Disk_Params	*p	/* OUT */ )
{
    Disk_st	*st = (Disk_st *) self->st;
    
    bcopy(&st->params, p, sizeof(Disk_Params));
}

static void 
Disk_Translate_m (
	Disk_cl	        *self,
	BlockDev_Block	 b	/* IN */,
	Disk_Position	*p	/* OUT */ )
{
/*    Disk_st	*st = (Disk_st *) self->st; */

}

static void 
Disk_Where_m (
	Disk_cl	        *self,
	Time_ns	         t	/* IN */,
	Disk_Position	*p	/* OUT */ )
{
/*   Disk_st	*st = (Disk_st *) self->st; */

}

static Time_ns 
Disk_ReadTime_m (
	Disk_cl	       *self,
	BlockDev_Block	b	/* IN */ )
{
    Disk_st	*st = (Disk_st *) self->st; 
    /* XXX SMH: hack up a random guess to stop gcc moaning */
    return st->params.seek + (st->params.rotate / 2) + 
	(st->params.xfr * st->params.blksz); 
}

static Time_ns 
Disk_WriteTime_m (
	Disk_cl	       *self,
	BlockDev_Block	b	/* IN */ )
{
    Disk_st	*st = (Disk_st *) self->st; 
    /* XXX SMH: hack up a random guess to stop gcc moaning */
    return st->params.seek + (st->params.rotate / 2) + 
	(st->params.xfr * st->params.blksz); 
}

/*
 * End 
 */
