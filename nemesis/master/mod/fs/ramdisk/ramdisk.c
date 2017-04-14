/* ramdisk.c - Implementation of Ram Disk device */
/* Matt Holgate - mjh44 */

/* TODO:
 * Sanity checks on parameters to new.
 */

#include <nemesis.h>
#include <stdio.h>
#include <exceptions.h>
#include <RamDiskMod.ih>
#include <BlockDev.ih>
#include <Disk.ih>
#include <RamDisk.ih>
#include <Heap.ih>
#include <Rd.ih>

#undef EOF            /* needed to correct problem using Rd$EOF */


/* debugging macro */

#undef DEBUG

#ifdef DEBUG
#define TRC(x) x
#define FTRC(format, args...)			\
printf(__FUNCTION__": " format			\
	, ## args 				\
      )
#else
#define TRC(x)
#define FTRC(format, args...)
#endif



/* definitions for RamDiskMod interface (module stuff) */

static RamDiskMod_New_fn      New_m;
static RamDiskMod_NewInit_fn  NewInit_m;
static RamDiskMod_op          rammod_op = { New_m, NewInit_m };
static const RamDiskMod_cl    rammod_cl = { &rammod_op, NULL };
CL_EXPORT (RamDiskMod, RamDiskMod, rammod_cl);


/* per Ram Disk state */
typedef struct _ramdisk_st {
    uint32_t    bs;    /* block size, bytes */
    uint32_t    size;  /* size of disk in blocks */
    addr_t      data;  /* pointer to memory allocated for disk */
    Heap_clp    h;     /* heap on which disk was allocated - used by Destroy */

} ramdisk_st;

/* BlockDev, Disk & RamDisk implementation */

static BlockDev_BlockSize_fn        RamDisk_BlockSize_m;
static BlockDev_Read_fn             RamDisk_Read_m;
static BlockDev_Write_fn            RamDisk_Write_m;

static Disk_GetParams_fn            RamDisk_GetParams_m;
static Disk_Translate_fn            RamDisk_Translate_m;
static Disk_Where_fn                RamDisk_Where_m;
static Disk_ReadTime_fn             RamDisk_ReadTime_m;
static Disk_WriteTime_fn            RamDisk_WriteTime_m;

static RamDisk_Destroy_fn           RamDisk_Destroy_m;

static RamDisk_op ram_op = {
    RamDisk_BlockSize_m,
    RamDisk_Read_m,
    RamDisk_Write_m,
    RamDisk_GetParams_m,
    RamDisk_Translate_m,
    RamDisk_Where_m,
    RamDisk_ReadTime_m,
    RamDisk_WriteTime_m,
    RamDisk_Destroy_m
};



/* create a new RamDisk interface for a ram disk of the specified parameters */
/* block nos start at 0 */

static RamDisk_clp  New_m (
       RamDiskMod_clp self,
       Heap_clp       h,
       uint32_t       bs,
       uint32_t       size )
{

    ramdisk_st         *ram_st   = NULL;
    RamDisk_clp         ram       = NULL;

    TRC(printf("RamDiskMod$New called. (version "
	       __DATE__ " " __TIME__ ")\n"));

    FTRC("RCS id: $Id: ramdisk.c 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $.\n");
    FTRC("entered: self=%p, h=%p, bs=%d, size=%d.\n", self, h, bs, size);


    /* create the ram disk state record */
    ram_st = Heap$Malloc ( h, sizeof(ramdisk_st) );
    if (!ram_st) {
	TRC(printf("RamDiskMod$New: failed to create state.\n"));
	return (RamDisk_clp) NULL;
    }

    /* initialise it */
    ram_st -> bs   = bs;
    ram_st -> size = size;
    ram_st -> data = NULL;
    ram_st -> h    = h;

    /* allocate the memory for the disk image */
    ram_st -> data = Heap$Malloc ( h, bs*size );
    if (! ram_st -> data) {
	TRC(printf("RamDiskMod$New: failed to create disk image.\n"));
	Heap$Free ( h, ram_st );
	return (RamDisk_clp) NULL;
    }
    
    /* now create and initialise the Disk Closure */
    ram = Heap$Malloc ( h, sizeof(RamDisk_cl) );
    if (!ram) {
	TRC(printf("RamDiskMod$New: failed to create RamDisk closure.\n"));
	Heap$Free ( h, ram_st->data );
	Heap$Free ( h, ram_st );
	return (RamDisk_clp) NULL;
    }
    ram -> op = &ram_op;
    ram -> st = ram_st;

    TRC(printf("RamDiskMod$New: Disk created. bs=%d size=%d bytes=%d\n", 
	       ram_st->bs, ram_st->size, ram_st->bs * ram_st->size ));

    return ram;

}

/* create a new RamDisk interface for a ram disk initialised with the
   contents of the specified reader */
static RamDisk_clp  NewInit_m (
        RamDiskMod_clp  self,
	Heap_clp        h,
	uint32_t        bs,
	Rd_clp          rd )
{
    uint64_t    NOCLOBBER len;
    uint32_t              size; /* size in blocks */
    RamDisk_clp           ram;
    char                  buf[bs];  /* temp buffer */
    uint64_t              nread;
    uint32_t              blk;

    TRC(printf("RamDiskMod$NewInit called.\n"));
    
    /* calculate size of disk to create from length of file */
    TRY {
	len = Rd$Length(rd);
    }
    CATCH_Rd$Failure(tmp) {
	TRC(printf("RamDiskMod$NewInit: unable to calculate "
		   "length of input: %d.\n", tmp));
	(void)tmp;
	return (RamDisk_clp) NULL;
    } ENDTRY;
    TRC(printf("Input stream is %d bytes\n",len));

    /* calculate no. of blocks and create the disk */
    size = 1+(len-1)/bs;
    TRC(printf("%d blocks of size %d\n",size,bs));
    ram = RamDiskMod$New(self, h, bs, size);
    if (!ram) {
	TRC(printf("RamDiskMod$NewInit: call to New failed.\n"));
	return ram;
    }

    /* read data into the disk use BlockDev$Write etc. */
    blk = 0;
    while (!Rd$EOF(rd)) {
	nread = Rd$GetChars(rd, buf, bs);
	if (nread) {
	    BlockDev$Write((BlockDev_clp) ram, blk, 1, buf);
	    blk++;
	}
    }
    
    return ram;
}


/* deallocates the space taken by a ram disk */

static void RamDisk_Destroy_m (RamDisk_clp self)
{

    ramdisk_st   *ram_st = (ramdisk_st *) self->st;
    Heap_clp      h;

    TRC(printf("RamDisk$Destroy called.\n"));

    /* get the heap */
    h = ram_st->h;
    
    /* deallocate the ram disk data */
    TRC(printf("RamDisk$Destroy: Trashing ramdisk at address %x\n",
	       (uint32_t) ram_st->data));
    Heap$Free ( h, ram_st->data );

    /* deallocate the state record */
    Heap$Free ( h, ram_st );

    /* now deallocate the disk closure */
    Heap$Free ( h, self );
}


static uint32_t RamDisk_BlockSize_m (BlockDev_clp self)
{
    ramdisk_st     *ram_st = (ramdisk_st *) self->st;

    return ram_st->bs;
}

static bool_t RamDisk_Read_m (BlockDev_clp     self,
			   BlockDev_Block   b,
			   uint32_t         n,
			   addr_t           a)
{
    ramdisk_st     *ram_st = (ramdisk_st *) self->st;
    void           *src;
    
    TRC(printf("BlockDev$Read: blk=%d\t\tnum=%d\n",b,n));

    if ((b >= ram_st->size) || (b<0)) {
	TRC(printf("BlockDev$Read: ERROR! Out of bounds read on "
		   "block %d.\n", b));
	return False;
    }

    if (b+n > ram_st->size) {
	TRC(printf("BlockDev$Read: ERROR! Out of bounds read of "
		   "%d blocks %d starting from %d.\n", n, b));
	return False;
    }

    src = (((int8_t *) ram_st->data) + b*ram_st->bs);
    TRC(printf("BlockDev$Read: Disk starts at address %x. We're "
	       "reading from %x.\n", 
	       (uint32_t) ram_st->data, (uint32_t) src));
    TRC(printf("BlockDev$Read: Reading %d bytes\n", n*ram_st->bs));
    memcpy(a, src, n*ram_st->bs);
    return True;

}


static bool_t RamDisk_Write_m (BlockDev_clp    self,
			       BlockDev_Block  b,
			       uint32_t        n,
			       addr_t          a)
{
    ramdisk_st     *ram_st = (ramdisk_st *) self->st;
    void           *dest;
    
    TRC(printf("BlockDev$Write: blk=%d\t\tnum=%d\n",b,n));
 
    if ((b >= ram_st->size) || (b<0)) {
	TRC(printf("BlockDev$Write: ERROR! Out of bounds write "
		   "on block %d.\n", b));
	return False;
    }

    if (b+n > ram_st->size) {
	TRC(printf("BlockDev$Write: ERROR! Out of bounds write "
		   "of %d blocks from block %d.\n", n, b));
	return False;
    }

    dest = (((int8_t *) ram_st->data) + b*ram_st->bs);
    TRC(printf("BlockDev$Write: Disk starts at address %x. We're "
	       "writing from %x.\n", 
	       (uint32_t) ram_st->data, (uint32_t) dest));
    TRC(printf("BlockDev$Write: Writing %d bytes\n", n*ram_st->bs));
    

    memcpy(dest, a, n*ram_st->bs);
    return True;
}


/* the following functions supply details of disk geometry etc.
   This doesn't really apply to a ramdisk, but these functions have
   been implemented so sensible values are returned. In the future,
   a ramdisk which simulates a real drive more correctly may be written,
   by inserting PAUSEs and returning suitable values from the following calls
   (and keeping track of the current position of the heads etc.)
   This may be useful to an intelligent USD disk scheduler 
*/

static void RamDisk_GetParams_m (
    Disk_clp self,
    Disk_Params   *p)
{
    ramdisk_st    *ram_st = (ramdisk_st *) self->st;
    
    p->blksz  = ram_st->bs;
    p->blocks = ram_st->size;
    p->cyls   = 1;
    p->heads  = 1;
    p->seek   = 0;
    p->rotate = 0;
    p->headsw = 0;
    p->xfr    = 0;
}

/* we only have one head and cylinder. Rotational time is 0 */
static void RamDisk_Translate_m (
    Disk_clp       self,
    uint32_t       b,
    Disk_Position *p)
{
    p->c = 0;
    p->h = 0;
    p->r = 0;   
}

/* doesn't really matter where we are! */
static void RamDisk_Where_m (
    Disk_clp       self,
    int64_t        t,
    Disk_Position *p)
{
    p->c = 0;
    p->h = 0;
    p->r = 0;
}

/* Time to read any block is (for our purposes) nothing from RAM */
static int64_t RamDisk_ReadTime_m (
    Disk_clp  self,
    uint32_t  blk)
{
    return 0;
}

/* Time to write any block is (for our purposes) nothing from RAM */
static int64_t RamDisk_WriteTime_m (
    Disk_clp  self,
    uint32_t  blk)
{
    return 0;
}

