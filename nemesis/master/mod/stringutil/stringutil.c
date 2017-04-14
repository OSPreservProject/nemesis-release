/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	stringutil.c
**
** FUNCTIONAL DESCRIPTION:
**
**	Helper functions for strings
**
** ENVIRONMENT: 
**
**	Shared library, usable anywhere.
**
** ID : $Id: stringutil.c 1.2 Thu, 08 Apr 1999 15:43:05 +0100 dr10009 $
**
**
*/

#include <nemesis.h>
#include <string.h>
#include <stdio.h>

#include <StringUtil.ih>
#include <Heap.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);


static	StringUtil_Cat_fn 		StringUtil_Cat_m;
static	StringUtil_Len_fn 		StringUtil_Len_m;
static  StringUtil_LongCardToIP_fn      StringUtil_LongCardToIP_m;
static  StringUtil_StringToCardinal_fn  StringUtil_StringToCardinal_m;

static	StringUtil_op	ms = {
  StringUtil_Cat_m,
  StringUtil_Len_m,
  StringUtil_LongCardToIP_m,
  StringUtil_StringToCardinal_m
};

static	const StringUtil_cl	cl = { &ms, NULL };

CL_EXPORT (StringUtil, StringUtil, cl);


/*---------------------------------------------------- Entry Points ----*/

static string_t StringUtil_Cat_m (
	StringUtil_cl	*self,
	Heap_clp	heap	/* IN */,
	string_t	beginning	/* IN */,
	string_t	end	/* IN */ )
{
  string_t       out;

  out = Heap$Malloc(heap, strlen(beginning) + strlen(end) + 1);
  strcpy(out, beginning);
  strcat(out, end);
  return(out);
}

static uint32_t StringUtil_Len_m (
	StringUtil_cl	*self,
	string_t	str	/* IN */ )
{
  return(strlen(str));
}

static string_t StringUtil_LongCardToIP_m (
	StringUtil_cl	*self,
	Heap_clp	heap	/* IN */,
	uint64_t        addr    /* IN */ )
{
  string_t       out;

  out = Heap$Malloc(heap, 17);
  sprintf(out, 
	  "%d.%d.%d.%d", 
	  (int)((addr >>  0) & 0xff),
	  (int)((addr >>  8) & 0xff),
	  (int)((addr >> 16) & 0xff),
	  (int)((addr >> 24) & 0xff));

  return(out);
}

static uint32_t StringUtil_StringToCardinal_m (
    StringUtil_cl   *self,
    string_t string )
{
    return atoi(string);
}

/*
 * End 
 */
