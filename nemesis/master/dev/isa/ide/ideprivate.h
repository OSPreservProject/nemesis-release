/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE: ideprivate.h
**
**	
**
** FUNCTIONAL DESCRIPTION: includes for IDE driver
**
**	
**
** ENVIRONMENT: 
**
**
**
** ID : $Id: ideprivate.h 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
**
**
*/

#include <BlockDev.ih>
#include <USDMod.ih>
#include <USDDrive.ih>

/* Maximum number of inb's to try while waiting for activity from drive */
#define MAX_TRIES 10

#define DRIVE_LET(x) ('a' + ((x)->ide->ifno*2) + (x)->num)

/* Forward declarations for this lot */
typedef struct _disc_t disc_t;
typedef struct _iface_t iface_t;

/* Per disc state */
struct _disc_t {
    id_info	id;	/* device identification details */
    iface_t	*ide;	/* parent pointer */
    int		num;	/* drive number (0 or 1) */
    uint32_t	maxsec;	/* largest linear sect num we will accept */
    int         mult_count; /* current setting for multiple sector mode */
   int using_dma;
};

typedef enum { Read, Write 
	       , MultRead, MultWrite 
	       , DMARead, DMAWrite
             } rq_type;

typedef struct {
    rq_type	type;
    disc_t	*d;	/* disc to operate on */
    uint32_t	block;	/* start block */
    uint32_t	count;	/* how many blocks */
    addr_t	buf;	/* from/to where */
    uint32_t	error;	/* any error happen? */
    bool_t	valid;	/* True if previous information is correct */
} req_t;

/* Per interface state */
struct _iface_t {
    mutex_t	mu;	/* lock held while accessing interface */
    disc_t	*hw[2];	/* 0 = master, 1 = slave */
    Disk_cl	disk[2];
    irq_stub_st		stub;
    Event_Count		event;
    Event_Val		ack;
    int		iobase;
    int		irq;
    unsigned long *dmatable;
    unsigned short    dma_base; 
    int		ifno;	/* 0 or 1 */
    req_t	rq;	/* work description */
    condition_t reqdone; /* signalled when rq has been processed */
    char	name[5];
};


