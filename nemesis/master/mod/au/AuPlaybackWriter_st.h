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
**	
**
** FUNCTIONAL DESCRIPTION:
**
**	
**
** ENVIRONMENT: 
**
**
**
** ID : $Id: AuPlaybackWriter_st.h 1.1 Thu, 18 Feb 1999 14:19:27 +0000 dr10009 $
**
**
*/

typedef struct _AuPlaybackWriter {
    Au_Rec *au;
    AuPlaybackWriter_cl closure;

    int32_t *ptr;
    uint32_t ptrloc;
    uint32_t absize;
} AuPlaybackWriter_st;
