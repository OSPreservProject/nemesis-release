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
**      direct.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**	Raw interface to OPPO
**
** ENVIRONMENT: 
**
**      OPPO device driver
** 
** ID : $Id: direct.c 1.1 Thu, 18 Feb 1999 15:09:21 +0000 dr10009 $
**
*/

#include <nemesis.h>
#include <stdio.h>

#include "otto_platform.h"
#include "otto_hw.h"
#include "state.h"
#include <DirectOppo.ih>

static DirectOppo_W13Open_fn W13Open_m;
static DirectOppo_GetCellBufs_fn GetCellBufs_m;
static DirectOppo_PartitionCellBufs_fn PartitionCellBufs_m;

static  DirectOppo_op   ms = {
  W13Open_m,
  GetCellBufs_m,
  PartitionCellBufs_m
};

static  DirectOppo_cl   cl = { &ms, NULL };

static AALPod_clp aalpod;

DirectOppo_cl *otto_direct_init(Otto_st *otto_st, AALPod_clp aal)
{
    cl.st = otto_st;
    aalpod = aal;  /* XXX nasty global! */
    return &cl;
}


/* ------------------------------------------------------------ */
/* Methods */


static IDCOffer_clp W13Open_m(
        DirectOppo_cl   *self,
        AALPod_Dir      dir     /* IN */,
        AALPod_Master   master  /* IN */,
        AALPod_Mode     mode    /* IN */,
        uint32_t        slots   /* IN */ )
{
    Otto_st *st = self->st;
    IDCOffer_clp offer;

    if (st->w13mode || st->normalmode)
	RAISE_ATM$VCIInUse(0, 0);

    offer = AALPod$Open(aalpod, dir, master, mode, 0, 109, slots);
    st->w13mode = True;
    st->normalmode = False;

    return offer;
}


static uint32_t GetCellBufs_m(DirectOppo_cl   *self)
{
    return OTTO_MAXCELLS;
}


static void PartitionCellBufs_m(
        DirectOppo_cl   *self,
        uint32_t        TX      /* IN */,
        uint32_t        NoAck   /* IN */ )
{
    Otto_st *st = self->st;

    otto_softreset(st->otto_hw, TX, NoAck);
}


/* End of direct.c */
