/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.		                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/nemesis/ugdb
**
** FUNCTIONAL DESCRIPTION:
** 
**      Definitions for the UGDB state.
** 
** ENVIRONMENT: 
**
**      User land.
** 
** FILE :	GDB_st.h
** CREATED :	Wed Jul 16 1997
** AUTHOR :	Steven Hand (Steven.Hand@cl.cam.ac.uk)
** ID : 	$Id: GDB_st.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _GDB_ST_H_
#define _GDB_ST_H_

#include <Rd.ih>
#include <Wr.ih>
#include <GDB.ih>
#include <GDBMod.ih>


typedef struct _GDB_st GDB_st;
struct _GDB_st {
    GDB_cl  cl;

    Rd_clp  rd;
    Wr_clp  wr;
    word_t *registers;
    word_t  numregbytes;

    char   *inbuf;
    char   *outbuf;
    word_t  bufmax;

    short   error;
    bool_t  remote_debug;   /* debug > 0 prints ill-formed commands, etc */ 

};



#endif /* _GDB_ST_H_ */
