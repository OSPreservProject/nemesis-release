/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.		                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      dev/pci/s3
**
** FUNCTIONAL DESCRIPTION:
** 
**      Header file for s3 stuff...
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
**
*/


int blat_YUV_tile( FB_st *st, pixel16_t *sptr, 
		   word_t sw, word_t sh, 
		   word_t sp, tag_t *clip,
		   word_t dx, word_t dy, 
		   word_t dw, word_t dh,
		   word_t tagword);

