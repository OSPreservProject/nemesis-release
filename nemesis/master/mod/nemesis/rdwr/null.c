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
**      null.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Null implementations of readers and writers
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: null.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <Wr.ih>
#include <Rd.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

static  Wr_PutC_fn              Wr_PutC_m;
static  Wr_PutStr_fn            Wr_PutStr_m;
static  Wr_PutChars_fn          Wr_PutChars_m;
static  Wr_Seek_fn              Wr_Seek_m;
static  Wr_Flush_fn             Wr_Flush_m;
static  Wr_Close_fn             Wr_Close_m;
static  Wr_Length_fn            Wr_Length_m;
static  Wr_Index_fn             Wr_Index_m;
static  Wr_Seekable_fn          Wr_Seekable_m;
static  Wr_Buffered_fn          Wr_Buffered_m;
static  Wr_Lock_fn              Wr_Lock_m;
static  Wr_Unlock_fn            Wr_Unlock_m;
static  Wr_LPutC_fn             Wr_LPutC_m;
static  Wr_LPutStr_fn           Wr_LPutStr_m;
static  Wr_LPutChars_fn         Wr_LPutChars_m;
static  Wr_LFlush_fn            Wr_LFlush_m;

static  Rd_GetC_fn              Rd_GetC_m;
static  Rd_EOF_fn               Rd_EOF_m;
static  Rd_UnGetC_fn            Rd_UnGetC_m;
static  Rd_CharsReady_fn        Rd_CharsReady_m;
static  Rd_GetChars_fn          Rd_GetChars_m;
static  Rd_GetLine_fn           Rd_GetLine_m;
static  Rd_Seek_fn              Rd_Seek_m;
static  Rd_Close_fn             Rd_Close_m;
static  Rd_Length_fn            Rd_Length_m;
static  Rd_Index_fn             Rd_Index_m;
static  Rd_Intermittent_fn      Rd_Intermittent_m;
static  Rd_Seekable_fn          Rd_Seekable_m;
static  Rd_Lock_fn              Rd_Lock_m;
static  Rd_Unlock_fn            Rd_Unlock_m;
static  Rd_LGetC_fn             Rd_LGetC_m;
static  Rd_LEOF_fn              Rd_LEOF_m;

static  Wr_op   wr_ms = {
    Wr_PutC_m,
    Wr_PutStr_m,
    Wr_PutChars_m,
    Wr_Seek_m,
    Wr_Flush_m,
    Wr_Close_m,
    Wr_Length_m,
    Wr_Index_m,
    Wr_Seekable_m,
    Wr_Buffered_m,
    Wr_Lock_m,
    Wr_Unlock_m,
    Wr_LPutC_m,
    Wr_LPutStr_m,
    Wr_LPutChars_m,
    Wr_LFlush_m
};

static  Rd_op   rd_ms = {
    Rd_GetC_m,
    Rd_EOF_m,
    Rd_UnGetC_m,
    Rd_CharsReady_m,
    Rd_GetChars_m,
    Rd_GetLine_m,
    Rd_Seek_m,
    Rd_Close_m,
    Rd_Length_m,
    Rd_Index_m,
    Rd_Intermittent_m,
    Rd_Seekable_m,
    Rd_Lock_m,
    Rd_Unlock_m,
    Rd_LGetC_m,
    Rd_LEOF_m
};

static const Wr_cl   wr_cl = { &wr_ms, NULL };
static const Rd_cl   rd_cl = { &rd_ms, NULL };

CL_EXPORT (Rd, NullRd, rd_cl);
CL_EXPORT (Wr, NullWr, wr_cl);


/*---------------------------------------------------- Entry Points ----*/

static void Wr_PutC_m (
    Wr_cl   *self,
    int8_t  ch      /* IN */ )
{
    return;
}

static void Wr_PutStr_m (
    Wr_cl   *self,
    string_t        s       /* IN */ )
{
    return;
}

static void Wr_PutChars_m (
    Wr_cl   *self,
    Wr_Buffer       s       /* IN */,
    uint64_t        nb      /* IN */ )
{
    return;
}

static void Wr_Seek_m (
    Wr_cl   *self,
    uint64_t        n       /* IN */ )
{
    return;
}

static void Wr_Flush_m (
    Wr_cl   *self )
{
    return;
}

static void Wr_Close_m (
    Wr_cl   *self )
{
    return;
}

static uint64_t Wr_Length_m (
    Wr_cl   *self )
{
    return 0;
}

static uint64_t Wr_Index_m (
    Wr_cl   *self )
{
    return 0;
}

static bool_t Wr_Seekable_m (
    Wr_cl   *self )
{
    return False;
}

static bool_t Wr_Buffered_m (
    Wr_cl   *self )
{
    return False;
}

static void Wr_Lock_m (
    Wr_cl   *self )
{
    return;
}

static void Wr_Unlock_m (
    Wr_cl   *self )
{
    return;
}

static void Wr_LPutC_m (
    Wr_cl   *self,
    int8_t  ch      /* IN */ )
{
    return;
}

static void Wr_LPutStr_m (
    Wr_cl   *self,
    string_t        s       /* IN */ )
{
    return;
}

static void Wr_LPutChars_m (
    Wr_cl   *self,
    Wr_Buffer       s       /* IN */,
    uint64_t        nb      /* IN */ )
{
    return;
}

static void Wr_LFlush_m (
    Wr_cl   *self )
{
    return;
}

static int8_t Rd_GetC_m (
    Rd_cl   *self )
{
    RAISE_Rd$EndOfFile();

    return 0;
}

static bool_t Rd_EOF_m (
    Rd_cl   *self )
{
    return True;
}

static void Rd_UnGetC_m (
    Rd_cl   *self )
{
    return;
}

static uint64_t Rd_CharsReady_m (
    Rd_cl   *self )
{
    return 0;
}

static uint64_t Rd_GetChars_m (
    Rd_cl   *self,
    Rd_Buffer       buf     /* IN */,
    uint64_t        nb      /* IN */ )
{
    return 0;
}

static uint64_t Rd_GetLine_m (
    Rd_cl   *self,
    Rd_Buffer       buf     /* IN */,
    uint64_t        nb      /* IN */ )
{
    return 0;
}

static void Rd_Seek_m (
    Rd_cl   *self,
    uint64_t        nb      /* IN */ )
{
    return;
}

static void Rd_Close_m (
    Rd_cl   *self )
{
    return;
}

static uint64_t Rd_Length_m (
    Rd_cl   *self )
{
    return 0;
}

static uint64_t Rd_Index_m (
    Rd_cl   *self )
{
    return 0;
}

static bool_t Rd_Intermittent_m (
    Rd_cl   *self )
{
    return False;
}

static bool_t Rd_Seekable_m (
    Rd_cl   *self )
{
    return False;
}

static void Rd_Lock_m (
    Rd_cl   *self )
{
    return;
}

static void Rd_Unlock_m (
    Rd_cl   *self )
{
    return;
}

static int8_t Rd_LGetC_m (
    Rd_cl   *self )
{
    RAISE_Rd$EndOfFile();
    return 0;
}

static bool_t Rd_LEOF_m (
    Rd_cl   *self )
{
    return True;
}

/* End */
