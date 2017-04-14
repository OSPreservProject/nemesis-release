/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      User safe disk device
** 
** FUNCTIONAL DESCRIPTION:
** 
**      State record
** 
** ENVIRONMENT: 
**
**      Internal to module mod/fs/usd
** 
** ID : $Id: usd.h 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#ifndef _USD_H_
#define _USD_H_

#include <mutex.h>

#include <Disk.ih>

#include <IDCCallback.ih>

#include <USD.ih>
#include <USDCtl.ih>
#include <USDCallback.ih>
#include <USDDrive.ih>

#include <IO.ih>

#include <QoSEntry.ih>
#include <QoSEntryMod.ih>


typedef struct _disk_t disk_t;

#include "disklabel.h"

#define UNIMPLEMENTED \
    printf("%s: UNIMPLEMENTED\n", __FUNCTION__);


/*******************************************************/
typedef struct _usd_st         usd_st;
typedef struct _transaction_t  transaction_t;
typedef struct _stream_t       stream_t;

#define USD_MAX_STREAMS    16	/* XXX Temp */
#define EXTENT_CACHE_SIZE   4

/* XXX SMH: We don't use the credit based scheme (RSCAN) anymore */
#define MAX_CREDITS_PER_SECOND (1L<<20) /* 2000 */
#define MAX_CREDIT 0                    /* (MAX_CREDITS_PER_SECOND >> 1) */

typedef struct {
    /* Leaky Bucket Scheduling Parameters */
    mutex_t        mu;		/* Lock for this state */
    int64_t        credits;	/* Credits in the bucket */
    uint64_t       creditspersec; /* Handy constants */
    Time_ns        nspercredit;	/*   "       "     */
    Time_ns        last;		/* Time of last credit */
    Time_ns        lastdbg;	/* Time of last debug printf */
} sched_params_t;

typedef enum { Free, Pending, Complete } transaction_state_t;


struct _stream_t {
    USD_StreamID   sid;		/* ID of this stream */
    bool_t         free;

    usd_st        *usd;

    IDCOffer_clp   offer;	/* Client offer */
    IDCService_clp service;	/* For revocation of offer */

    IDCOffer_clp   io_offer;    /* Offer of plain IO channel */
    IDCService_clp io_service;  /* For revocation of plain IO offer */

    IO_clp         io;		/* Client IO connection */
    IDCCallback_cl scb;         /* For notification of bind */
    USD_ClientID   cid;		/* Opaque client ID */

    /* Cache of exents this connection can access */
    struct {
	USD_Extent      extent;
	USD_Permissions permissions;
    } extentCache[EXTENT_CACHE_SIZE];
    uint32_t       extentLast;

    /* Leaky Bucket Scheduling */
    USD_QoS        qos;		/* Current QoS */

    /* QoSEntry type qos stuff. */
    Time_ns        s, p;
    bool_t         x;

    sched_params_t *sp;		/* Points to contracted or besteffort */
    sched_params_t params;	/* This may be handy */
};


/*
 * State for this disk 
 */
#define USD_MAX_PARTITIONS 8
#define USD_MAX_STREAMS    16	/* XXX Temp */

struct _disk_t {
    mutex_t               mu;
    Disk_clp              device;
    Disk_Params           params; 

    char                  name[32];

    /* OSF1 Partition table */
    struct disklabel      label;

    /* IO Streams */
    mutex_t               smu;
    stream_t 		  stream[USD_MAX_STREAMS];
    QoSEntry_clp          qosentry;

    /* Leaky bucket scheduling */
    uint64_t      	  contracted; /* percentage (!!!) contracted */
    sched_params_t        besteffort;
    uint64_t              extra;

    uint64_t              accounted;

    /* General stuff we need to know */
    USDDrive_cl		  drive;
    bool_t                msdos_ptab;
    usd_st 		 *partition[USD_MAX_PARTITIONS];
    usd_st               *whole_disk;
};


/*
 * State for the USD - one per partition
 */
struct _usd_st {
    USDCtl_cl             control;
    USDCallback_clp 	  handler;

    disk_t               *disk;
    struct d_partition   *partition;
    uint32_t		  number;

    IDCOffer_clp	  offer;
    IDCService_clp	  service;
    IDCCallback_cl	  callback;

    IDCServerBinding_clp  binding; /* Current binding to this partition */

    USDCallback_clp       callbacks;

    /* XXX We should record the streams associated with this partition. */
};

extern void IOThread(disk_t *disk);

/* Utility functions to create/free FileIO offers */
extern IDCOffer_clp create_fileio_offer(IDCOffer_cl *, uint64_t, uint32_t);
extern uint64_t get_fileio_length(IDCOffer_cl *);
extern void set_fileio_length(IDCOffer_cl *, uint64_t);
extern void free_fileio_offer(IDCOffer_cl *);

#endif /* _USD_H_ */

