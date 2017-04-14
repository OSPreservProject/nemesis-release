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
**      Nemesis client rendering window system.
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Pixel blasting code
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: blit_16.c 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <IDCMacros.h>
#include <IOMacros.h>
#include <stdio.h>
#include <time.h>
#include <ntsc.h>
#include "FB_st.h"

#include "s3.h"
#define SECURE 0

#define PROF(_x)

static INLINE void clip8(row16_t *src, 
			 pixel16_t *dst, 
			 tag_t *mask,
			 word_t  tag);

static INLINE void blat8(row16_t *src,
			 pixel16_t *dst);

static INLINE void clip8_8(row8_t *src,
			   pixel16_t *dst, 
			   tag_t *mask,
			   word_t  tag);

static INLINE void blat8_8(row8_t *src,
			   pixel16_t *dst);

#ifdef __ALPHA__ 
#define EXPAND_4(src, dst) "
        insbl " #src ", 0,"    #dst "
        extwl " #src ", 0,     $1          # 0,1
        sll     $1,     8,     $1          #
        bis     $1, "   #dst "," #dst " 

        extwl " #src ", 1,     $1          # 1,2
        sll     $1,     24,    $1          #
        bis     $1, "   #dst "," #dst "    #


        extwl " #src ", 2,     $1          # 2,3
        sll     $1,     40,    $1          #
        bis     $1, "   #dst "," #dst "    #

        extbl " #src ", 3,     $1          # 3
        sll     $1,     56,    $1          #
        bis     $1, "   #dst "," #dst "    # 
       " 
        

#define EXPAND(src, dst1, dst2) \
        EXPAND_4(src, dst1) \
        " srl " #src ", 32, " #src \
        EXPAND_4(src, dst2)


static INLINE void clip8(row16_t *src, 
			 pixel16_t *dst, 
			 tag_t *mask,
			 word_t  tag)
{ 
  __asm__ ("
        ldq	$2, 0(%2)     #load mask into r2

        eqv     $2, %3, $2    #cmp with tag, into r2
	cmpbge  $2, %4, $2    #cmp with 0xff.. into r2
 	beq     $2, 2f        #if all zeros quit
        ldq    	$4, 0(%0)     #load source into r4 and r5
        ldq     $5, 8(%0)

        # r2 = blit bitmask
        # r4 = source pixels 1
        # r5 = source pixels 2

        cmpeq   $2, 0xff, $3  # check if all ones

        blbs    $3, 1f        # if so, blit them all

                
        zapnot  %4, $2, $2

        ldq     $6, 0(%1)   
        " EXPAND_4($2, $3) "

        and     $4, $3, $4
        bic     $6, $3, $6
        bis     $6, $4, $4

        ldq     $6, 8(%1)

        srl     $2, 32, $2
        " EXPAND_4($2, $3) "

        and     $5, $3, $5
        bic     $6, $3, $6
        bis     $6, $5, $5
        
1:      stq     $4, 0(%1)
        stq     $5, 8(%1)
2:
        " 
	     :			         /* Outputs */       
	     : "r" (src), "r" (dst),     /* Inputs */
	       "r" (mask), "r" (tag),  
	       "r" (0xFFFFFFFFFFFFFFFFUL)
	     : "$1", "$2", "$3", "$4", "$5", "$6"   /* Clobbers */      
	     );
}

static INLINE void blat8(row16_t *src,
			 pixel16_t *dst) {

    __asm__ ("
        ldq    $2, 0(%0)
        stq    $2, 0(%1)
        ldq    $3, 8(%0)
        stq    $3, 8(%1)
           "
	     :
	     : "r" (src), "r" (dst)
	     : "$2", "$3"
	);
}

static INLINE void clip8_8(row8_t *src, 
			 pixel16_t *dst, 
			 tag_t *mask,
			 word_t  tag)
{ 
  __asm__ ("
        ldq	$2, 0(%2)     #load mask into r2

        eqv     $2, %3, $2    #cmp with tag, into r2
	cmpbge  $2, %4, $2    #cmp with 0xff.. into r2
 	beq     $2, 2f        #if all zeros quit
        ldq    	$3, 0(%0)     #load source into r3
        " EXPAND($3, $4, $5)  "  #expand source into r4 and r5 

        # r2 = blit bitmask
        # r4 = source pixels 1
        # r5 = source pixels 2

        cmpeq   $2, 0xff, $3  # check if all ones

        blbs    $3, 1f        # if so, blit them all

        
        zapnot  %4, $2, $2

        ldq     $6, 0(%1)   
        " EXPAND_4($2, $3) "

        and     $4, $3, $4
        bic     $6, $3, $6
        bis     $6, $4, $4

        ldq     $6, 8(%1)
        srl     $2, 32, $2
        " EXPAND_4($2, $3) "

        and     $5, $3, $5
        bic     $6, $3, $6
        bis     $6, $5, $5
        
1:      stq     $4, 0(%1)
        stq     $5, 8(%1)
2:
        " 
	     :			         /* Outputs */       
	     : "r" (src), "r" (dst),     /* Inputs */
	       "r" (mask), "r" (tag),  
	       "r" (0xFFFFFFFFFFFFFFFFUL)
	     : "$1", "$2", "$3", "$4", "$5", "$6"   /* Clobbers */      
	     );
}


static INLINE void blat8_8(row8_t *src,
			   pixel16_t *dst) {

    __asm__ ("
         ldq   $2, 0(%0)
         " EXPAND_4($2, $3) "
         stq   $3, 0(%1)
         srl   $2, 32, $2
         " EXPAND_4($2, $4) "
         stq   $4, 8(%1)
        "
	     :
	     : "r" (src), "r" (dst)
	     : "$2", "$3", "$4"
	);
}

#else

static INLINE void clip8(row16_t *src, 
			 pixel16_t *dst, 
			 tag_t *mask,
			 word_t tag4)
{ 
    uint32_t *srcptr = (uint32_t *)src;
    uint32_t *dstptr = (uint32_t *)dst;

    uint32_t   mask4;
    uint16_t *mask2ptr = (uint16_t *)mask;

    mask4 = (*mask2ptr++);
    mask4 |= (*mask2ptr++) <<16;

    if(mask4 == (uint32_t)tag4) {
	
	*(dstptr++) = *(srcptr++);
	*(dstptr++) = *(srcptr++);

    } else {
	int p1 = (mask4 & 0xff) == (tag4 & 0xff);
	int p2 = (mask4 & 0xff00) == (tag4 & 0xff00);
	int p3 = (mask4 & 0xff0000) == (tag4 & 0xff0000);
	int p4 = (mask4 & 0xff000000) == (tag4 & 0xff000000);

	if (p1 | p2 | p3 | p4) {
	    
	    pixel16_t *dstptr16 = (pixel16_t *) dstptr;
	    pixel16_t *srcptr16 = (pixel16_t *) srcptr;

	    if(p1) dstptr16[0] = srcptr16[0];
	    
	    if(p2) dstptr16[1] = srcptr16[1];

	    if(p3) dstptr16[2] = srcptr16[2];

	    if(p4) dstptr16[3] = srcptr16[3];

	}

	dstptr +=2;
	srcptr +=2;
	
    }


    mask4 = (*mask2ptr++);
    mask4 |= (*mask2ptr++)<<16;

    if(mask4 == (uint32_t) tag4) {
	*(dstptr++) = *(srcptr++);
	*(dstptr++) = *(srcptr++);
    } else {

	int p1 = (mask4 & 0xff) == (tag4 & 0xff);
	int p2 = (mask4 & 0xff00) == (tag4 & 0xff00);
	int p3 = (mask4 & 0xff0000) == (tag4 & 0xff0000);
	int p4 = (mask4 & 0xff000000) == (tag4 & 0xff000000);

	if (p1 | p2 | p3 | p4) {
	    
	    pixel16_t *dstptr16 = (pixel16_t *) dstptr;
	    pixel16_t *srcptr16 = (pixel16_t *) srcptr;

	    if(p1) dstptr16[0] = srcptr16[0];
	    
	    if(p2) dstptr16[1] = srcptr16[1];

	    if(p3) dstptr16[2] = srcptr16[2];

	    if(p4) dstptr16[3] = srcptr16[3];

	}

	dstptr +=2;
	srcptr +=2;
    }
}

static INLINE void blat8(row16_t *src,
			 pixel16_t *dst) {

    uint32_t *srcptr = (uint32_t *)src;
    uint32_t *dstptr = (uint32_t *)dst;

    *(dstptr++) = *(srcptr++);
    *(dstptr++) = *(srcptr++);
    *(dstptr++) = *(srcptr++);
    *(dstptr++) = *(srcptr++);
    
}

static INLINE void clip8_8(row8_t *src, 
			   pixel16_t *dst, 
			   tag_t *mask,
			   word_t  tag4)
{ 

    
    uint32_t *srcptr= (uint32_t *)src;
    uint32_t *dstptr= (uint32_t *)dst;
    uint32_t tmp;
    uint32_t mask4;
    uint16_t *mask2ptr = (uint16_t *)mask;

    mask4 = (*mask2ptr++);
    mask4 |= (*mask2ptr++)<<16;

    if (mask4 == (uint32_t)tag4) {

	/* space is all owned by this window */
	
	tmp = *srcptr++;

	*dstptr++ = ((tmp &0xff00) << 16) | ((tmp & 0xffff)<<8) | (tmp &0xff);

	*dstptr++ = (tmp &0xff000000) | ((tmp & 0xffff0000)>>8) | ((tmp &0xff0000)>>16);

    } else {
	
	int p1 = (mask4 & 0xff) == (tag4 & 0xff);
	int p2 = (mask4 & 0xff00) == (tag4 & 0xff00);
	int p3 = (mask4 & 0xff0000) == (tag4 & 0xff0000);
	int p4 = (mask4 & 0xff000000) == (tag4 & 0xff000000);

	
	if (p1 | p2 | p3 | p4) {
	    
	    pixel16_t *dstptr16 = (pixel16_t *) dstptr;
	    tmp = *(uint32_t *)srcptr;
	    
	    if(p1) {
		dstptr16[0] = ((tmp & 0xff) << 8) | (tmp & 0xff);
	    }
	    
	    if(p2) {
		dstptr16[1] = 
		    ((tmp &0xff00) >> 8) | (tmp & 0xff00);
	    }
	    
	    if(p3) {
		
		dstptr16[2] = ((tmp & 0xff0000) >> 16) 
		    | ((tmp & 0xff0000) >> 8);
	    }
	    
	    if(p4) {
		    
		dstptr16[3] = 
		    ((tmp & 0xff000000) >>16) 
		    | ((tmp & 0xff000000) >> 24);
	    }

	}
	
	srcptr++;
	dstptr += 2;

    }
	    
    mask4 = (*mask2ptr++);
    mask4 |= (*mask2ptr++)<<16;
    
    if (mask4 == (uint32_t)tag4) {

	/* space is all owned by this window */
	
	tmp = *srcptr++;

	*dstptr++ = 
	    ((tmp &0xff00) << 16) 
	    | ((tmp & 0xffff)<<8) 
	    | (tmp &0xff);

	*dstptr++ = 
	    (tmp &0xff000000) 
	    | ((tmp & 0xffff0000)>>8) 
	    | ((tmp &0xff0000)>>16);

    } else {
	
	int p1 = (mask4 & 0xff) == (tag4 & 0xff);
	int p2 = (mask4 & 0xff00) == (tag4 & 0xff00);
	int p3 = (mask4 & 0xff0000) == (tag4 & 0xff0000);
	int p4 = (mask4 & 0xff000000) == (tag4 & 0xff000000);

	
	if (p1 | p2 | p3 | p4) {
	    
	    pixel16_t *dstptr16 = (pixel16_t *)dstptr;
	    tmp = *(uint32_t *)srcptr;
	    
	    if(p1) {
		dstptr16[0] = ((tmp & 0xff) << 8) | (tmp & 0xff);
	    }
	    
	    if(p2) {
		dstptr16[1] = 
		    ((tmp &0xff00) >> 8) | (tmp & 0xff00);
	    }
	    
	    if(p3) {
		
		dstptr16[2] = ((tmp & 0xff0000) >> 16) 
		    | ((tmp & 0xff0000) >> 8);
	    }
	    
	    if(p4) {
		    
		dstptr16[3] = 
		    ((tmp & 0xff000000) >>16) 
		    | ((tmp & 0xff000000) >> 24);
	    }
	    
	}

	srcptr++;
	dstptr += 2;
    }
    
}

static INLINE void blat8_8(row8_t *src,
			   pixel16_t *dst) {

    uint32_t *srcptr = (uint32_t *)src;
    uint32_t *dstptr = (uint32_t *)dst;
    uint32_t tmp;

    tmp = *srcptr++;
    
    *dstptr++ = 
	((tmp &0xff00) << 16) | 
	((tmp & 0xffff)<<8) | 
	(tmp &0xff);
    
    *dstptr++ = 
	(tmp &0xff000000) | 
	((tmp & 0xffff0000)>>8) | 
	((tmp &0xff0000)>>16);
    
    tmp = *srcptr++;
    
    *dstptr++ = 
	((tmp &0xff00) << 16) | 
	((tmp & 0xffff)<<8) | 
	(tmp &0xff);
    
    *dstptr++ = 
	(tmp &0xff000000) | 
	((tmp & 0xffff0000)>>8) | 
	((tmp &0xff0000)>>16);
    
}
    
#endif

static INLINE void blit_tile_to_raster(tile16_t   *src, 
					pixel16_t *dst, 
					tag_t   *mask,
					word_t  tagword,
				        word_t ntiles)
{
    int i;
    for (i = 0; i < ntiles; i++)
    {
	clip8(&src->row[0], dst + (FB_WIDTH*0), mask+(FB_WIDTH*0), tagword);
	clip8(&src->row[1], dst + (FB_WIDTH*1), mask+(FB_WIDTH*1), tagword);
	clip8(&src->row[2], dst + (FB_WIDTH*2), mask+(FB_WIDTH*2), tagword);
	clip8(&src->row[3], dst + (FB_WIDTH*3), mask+(FB_WIDTH*3), tagword);
	clip8(&src->row[4], dst + (FB_WIDTH*4), mask+(FB_WIDTH*4), tagword);
	clip8(&src->row[5], dst + (FB_WIDTH*5), mask+(FB_WIDTH*5), tagword);
	clip8(&src->row[6], dst + (FB_WIDTH*6), mask+(FB_WIDTH*6), tagword);
	clip8(&src->row[7], dst + (FB_WIDTH*7), mask+(FB_WIDTH*7), tagword);

	src++;dst+=8;mask+=8;
    }
}

static INLINE void blat_tile_to_raster(tile16_t *src, 
				       pixel16_t *dst,
				       word_t ntiles)
{
    int i;
    for(i = 0; i<ntiles; i++) {
	blat8(&src->row[0], dst + (FB_WIDTH*0));
	blat8(&src->row[1], dst + (FB_WIDTH*1));
	blat8(&src->row[2], dst + (FB_WIDTH*2));
	blat8(&src->row[3], dst + (FB_WIDTH*3));
	blat8(&src->row[4], dst + (FB_WIDTH*4));
	blat8(&src->row[5], dst + (FB_WIDTH*5));
	blat8(&src->row[6], dst + (FB_WIDTH*6));
	blat8(&src->row[7], dst + (FB_WIDTH*7));
	
	src++;dst+=8;
    }
}


static INLINE void blit_8_tile_to_raster(tile8_t   *src, 
					pixel16_t *dst, 
					tag_t   *mask,
					word_t  tagword,
				        word_t ntiles)
{
    int i;
    for (i = 0; i < ntiles; i++)
    {
	clip8_8(&src->row[0], dst + (FB_WIDTH*0), mask+(FB_WIDTH*0), tagword);
	clip8_8(&src->row[1], dst + (FB_WIDTH*1), mask+(FB_WIDTH*1), tagword);
	clip8_8(&src->row[2], dst + (FB_WIDTH*2), mask+(FB_WIDTH*2), tagword);
	clip8_8(&src->row[3], dst + (FB_WIDTH*3), mask+(FB_WIDTH*3), tagword);
	clip8_8(&src->row[4], dst + (FB_WIDTH*4), mask+(FB_WIDTH*4), tagword);
	clip8_8(&src->row[5], dst + (FB_WIDTH*5), mask+(FB_WIDTH*5), tagword);
	clip8_8(&src->row[6], dst + (FB_WIDTH*6), mask+(FB_WIDTH*6), tagword);
	clip8_8(&src->row[7], dst + (FB_WIDTH*7), mask+(FB_WIDTH*7), tagword);

	src++;dst+=8;mask+=8;
    }
}

static INLINE void blat_8_tile_to_raster(tile8_t   *src, 
					pixel16_t *dst, 
				        word_t ntiles)
{
    int i;
    for (i = 0; i < ntiles; i++)
    {
	blat8_8(&src->row[0], dst + (FB_WIDTH*0));
	blat8_8(&src->row[1], dst + (FB_WIDTH*1));
	blat8_8(&src->row[2], dst + (FB_WIDTH*2));
	blat8_8(&src->row[3], dst + (FB_WIDTH*3));
	blat8_8(&src->row[4], dst + (FB_WIDTH*4));
	blat8_8(&src->row[5], dst + (FB_WIDTH*5));
	blat8_8(&src->row[6], dst + (FB_WIDTH*6));
	blat8_8(&src->row[7], dst + (FB_WIDTH*7));

	src++;dst+=8;
    }
}

uint32_t blit_raster(FB_st *st,
		     FB_StreamID sid, 
		     FBBlit_Rec *rec, 
		     word_t padding1, 
		     word_t padding2, 
		     dcb_ro_t *dcb) 
{
  FB_Stream *s = &st->stream[sid];
  FB_Window *w = s->w;
      

  pixel16_t  *pmagptr;
  row16_t  *dataptr;
  tag_t    *clipptr;

  int32_t x, y, i, j;
  word_t tagword;
  word_t destskip, dataskip; 
  
  word_t xtiles, ylines;
  word_t lines_left, maxlines;

  PROF(Time_T start = NOW();)

  if (SECURE && w->owner != dcb) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) -1;
  }

  if(!w->mapped) {
      PROF(rec->rsvd += NOW() - start);
      return 0;
  }

  x = rec->x;

  if(x & (PIXEL_X_GRID -1) || (((word_t)rec->data) & (sizeof(word_t) - 1))) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) -2;
  }

  y = rec->y;

  if((x >= w->width) || (y >= w->height) || 
     (x + w->x >= FB_WIDTH) || (y + w->y >= FB_HEIGHT)) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) 0;
  }

  xtiles = (MIN(rec->sourcewidth, MIN(w->width, FB_WIDTH - w->x) - x) + 7)>>3;
  ylines = MIN(rec->sourceheight, MIN(w->height, FB_HEIGHT - w->y) - y);

  if((xtiles == 0) || (ylines == 0)) {
      PROF(rec->rsvd += NOW() - start);
      return 0;
  }

  destskip = FB_WIDTH - (xtiles * 8);
  dataskip = (rec->stride >> 4) - xtiles;

  tagword = w->tagword;

  pmagptr = w->pmag + (y * FB_WIDTH) + x;
  clipptr = w->clip + (y * FB_WIDTH) + x;
  dataptr = (row16_t *) rec->data;

  maxlines = MAX((MAX_PIXELS >> 3) / xtiles, 1);

  if(maxlines < ylines) {
      /* we can't do it all in one go */

      lines_left = ylines - maxlines;
      rec->sourceheight -= maxlines;
      rec->y += maxlines;
      ylines = maxlines;
      rec->data += rec->stride * maxlines;
  } else {
      lines_left = 0;
  }
  
  for (i = 0; i < ylines; i++)
  {
      if(clipptr >= st->clipbase) {

	  for (j = 0; j < xtiles; j++)
	  {
	      
	      clip8(dataptr, pmagptr, clipptr, tagword);
	      
	      dataptr ++;
	      pmagptr += 8;
	      clipptr += 8;
	      
	  }
	  
	  dataptr += dataskip;
	  pmagptr += destskip;
	  clipptr += destskip;
      }
  }

  PROF(rec->rsvd += NOW() - start);
  return lines_left;

}

uint32_t blit_raster_8(FB_st *st,
		       FB_StreamID sid, 
		       FBBlit_Rec *rec,
		       word_t padding1,
		       word_t padding2,
		       dcb_ro_t *dcb) 
{
  FB_Stream *s = &st->stream[sid];
  FB_Window *w = s->w;
      
  pixel16_t  *pmagptr;
  row8_t  *dataptr;
  tag_t    *clipptr;

  word_t x, y, i, j;
  word_t tagword;
  word_t destskip, dataskip;

  word_t xtiles, ylines;
  word_t lines_left, maxlines;
  
  PROF(Time_T start = NOW();)

  if (SECURE && w->owner != dcb){
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) -1;
  }

  if(!w->mapped) {
      PROF(rec->rsvd += NOW() - start);
      return 0;
  }

  x = rec->x;

  if(x & (PIXEL_X_GRID -1) || (((word_t)rec->data) & (sizeof(word_t) - 1))) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) -2;
  }

  y = rec->y;

  if((x >= (w->width>>3)) || (y >= w->height) ||
      (x + w->x >= FB_WIDTH) || (y + w->y >= FB_HEIGHT)) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) 0;
  }

  xtiles = (MIN(rec->sourcewidth, w->width - rec->x)+7) >> 3;
  ylines = MIN(rec->sourceheight, (w->height - y));

  if((xtiles == 0) || (ylines == 0)) {
      PROF(rec->rsvd += NOW() - start);
      return 0;
  }


  destskip = FB_WIDTH - (xtiles * 8);
  dataskip = (rec->stride >> 3) - xtiles; 
  tagword = w->tagword;

  pmagptr = w->pmag + (y * FB_WIDTH) + x;
  clipptr = w->clip + (y * FB_WIDTH) + x;
  dataptr = (row8_t *) rec->data;

  maxlines = MAX((MAX_PIXELS >> 3) / xtiles, 1);

  if(maxlines < ylines) {
      /* we can't do it all in one go */

      lines_left = ylines - maxlines;
      rec->sourceheight -= maxlines;
      rec->y += maxlines;
      ylines = maxlines;
      rec->data += rec->stride * maxlines;
  } else {
      lines_left = 0;
  }

  for (i = 0; i < ylines; i++)
  {
      for (j = 0; j < xtiles; j++)
      {
	  clip8_8(dataptr, pmagptr, clipptr, tagword);
	  
	  dataptr ++;
	  pmagptr += 8;
	  clipptr += 8;

      }

      dataptr += dataskip;
      pmagptr += destskip;
      clipptr += destskip;
      
  }

  PROF(rec->rsvd += NOW() - start);
  return lines_left;
}


uint32_t blit_dfs(FB_st *st, 
		  FB_StreamID sid, 
		  FBBlit_Rec *rec,
		  word_t padding1,
		  word_t padding2,
		  dcb_ro_t *dcb)
{
  FB_Stream *s = &st->stream[sid];
  FB_Window *w = s->w;
      
  tile16_t  *dataptr;
  pixel16_t  *pmagptr;
  tag_t    *clipptr;

  word_t   x, y;
  word_t   ntiles, xtiles, ytiles;
  word_t tagword;
  bool_t more_work = False;

  PROF(Time_T start = NOW();)

  if (SECURE && w->owner != dcb) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) -1;
  }

  if(!w->mapped) {
      PROF(rec->rsvd += NOW() - start);
      return 0;
  }

  x = rec->x;
  y = rec->y;

  
  if(rec->destwidth < rec->sourcewidth) {
      x += rec->sourcewidth - rec->destwidth;
  }

  if(x & (PIXEL_X_GRID - 1)) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) - 2;
  }

  if (x >= w->width) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) 0;
  }

  if (y >= w->height) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) 0;
  }

  xtiles = (rec->destwidth + 7) >> 3;
  ytiles = (rec->sourceheight + 7) >> 3;

  dataptr  = (tile16_t *)rec->data;

  if(xtiles > MAX_PIXELS/64) {
      more_work = True;

      rec->destwidth -= MAX_PIXELS/8;
      rec->data += MAX_PIXELS * 2;
      xtiles = MAX_PIXELS/64;
  } else if(ytiles > 1) {
      more_work = True;

      rec->y += 8;
      rec->sourceheight -= 8;
      rec->destwidth = rec->sourcewidth;
      rec->data += xtiles * 64 * 2;
      
  }

  tagword = w->tagword;

  pmagptr = w->pmag + (y * FB_WIDTH) + x;
  clipptr = w->clip + (y * FB_WIDTH) + x;

  if(clipptr >= st->clipbase) {
      ntiles = MIN(((MIN(FB_WIDTH - w->x, w->width) - x) +7) >> 3, xtiles);
      blit_tile_to_raster (dataptr, pmagptr, clipptr, tagword, ntiles); 
  }

  PROF(rec->rsvd += NOW() - start);      
  return more_work;
}

/*
 * Blit DFS 8 takes an 8bpp tiled image & paints it on a 16 bpp display...
 */

uint32_t blit_dfs_8(FB_st *st,
		    FB_StreamID sid, 
		    FBBlit_Rec *rec,
		    word_t padding1,
		    word_t padding2,
		    dcb_ro_t *dcb)
{
  FB_Stream *s = &st->stream[sid];
  FB_Window *w = s->w;
      
  tile8_t  *dataptr;
  pixel16_t  *pmagptr;
  tag_t    *clipptr;

  uint32_t   x, y;
  word_t   ntiles, xtiles, ytiles;
  word_t tagword;
  bool_t more_work = False;
  word_t count;

  PROF(Time_T start = NOW();)

  if (SECURE && w->owner != dcb) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) -1;
  }

  if(!w->mapped) {
      PROF(rec->rsvd += NOW() - start);
      return 0;
  }

  x = rec->x;
  y = rec->y;

  if(rec->destwidth < rec->sourcewidth) {
      x += rec->sourcewidth - rec->destwidth;
  }

  if(x & (PIXEL_X_GRID - 1)) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) -2;
  }

  if (x >= w->width) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) 0;
  }

  if (y >= w->height) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) 0;
  }

  xtiles = (rec->destwidth + 7) >> 3;
  ytiles = (rec->sourceheight + 7) >> 3;

  dataptr  = (tile8_t *)rec->data;

  if(xtiles > MAX_PIXELS/64) {
      more_work = True;

      rec->destwidth -= MAX_PIXELS/8;
      rec->data += MAX_PIXELS;
      xtiles = MAX_PIXELS/64;
  } else if(ytiles > 1) {
      more_work = True;

      rec->y += 8;
      rec->sourceheight -= 8;
      rec->destwidth = rec->sourcewidth;
      rec->data += xtiles * 64;
  }

  pmagptr = w->pmag + (y * FB_WIDTH) + x;
  clipptr = w->clip + (y * FB_WIDTH) + x;

  if(clipptr >= st->clipbase) {

      ntiles = MIN(((MIN(FB_WIDTH - w->x, w->width) - x) + 7) >> 3, xtiles);
      
      blit_8_tile_to_raster (dataptr, pmagptr, clipptr, 
			     w->tagword, ntiles); 
      
  }


  PROF(rec->rsvd += NOW() - start);
  
  return more_work;
}

/* Video Engine blitting code for the S3 Vision 968.
 * This was developed under an NDA so the file S3 should be shipped as
 * binary only.
 */

uint32_t blit_video(FB_st *st,
		    FB_StreamID sid, 
		    FBBlit_Rec *rec,
		    word_t padding1,
		    word_t padding2,
		    dcb_ro_t *dcb) 
{
    FB_Stream *s = &st->stream[sid];
    FB_Window *w = s->w;
    
    word_t x, y, i;
    word_t ntiles;
    uint64_t tag;
    
    PROF(Time_T start = NOW();)

    if (SECURE && w->owner != dcb) {
      PROF(rec->rsvd += NOW() - start);
      return (uint32_t) -1;
    }

    if ((w->width <= rec->x) ||(w->height < rec->y)) {
	PROF(rec->rsvd += NOW() - start);      
	return 0;  
    }
    
    /* XXX; video engine code removed to make the source non confidential */
#if 0
     /* Call Video Engine code */
    if (rec->destheight==2*(rec->sourceheight))
    {
	sleazy_blat_YUV_tile(st, 
			     (uint32_t*)rec->data, 
			     rec->sourcewidth, rec->sourceheight, 
			     rec->stride, 
			     (rec->x+w->x), rec->y + w->y, 
			     rec->destwidth, rec->destheight); 
    }
    else
    {
	tag_t *clipbase;

	clipbase = w->clip + rec->x + (rec->y * FB_WIDTH);
	blat_YUV_tile(st, (uint32_t *)rec->data, 
		      rec->sourcewidth, rec->sourceheight, 
		      rec->stride, clipbase, 
		      rec->x+w->x, rec->y+w->y, 
		      rec->destwidth, rec->destheight,
		      w->tagword); 
    }
#endif
    PROF(rec->rsvd += NOW() - start);
    return 0;

}

void blit_loop(FB_st *st)
{
    IO_clp	io;
    IO_Rec	rec[3];
    word_t	value;
    uint32_t	nrecs;

    while(1)
    {
	FB_Stream *s;

	io = IOEntry$Rendezvous(st->fbentry, FOREVER);
	s  = (FB_Stream *)IO_HANDLE(io);
#if 0
	nrecs = IO$GetPkt(io, 2, rec, &valid);
	ENTER_KERNEL_CRITICAL_SECTION();
	s->blit_m(st, s->w->owner, s->sid, rec, nrecs);
	LEAVE_KERNEL_CRITICAL_SECTION();
	IO$PutPkt(io, nrecs, rec, valid);
#else
	while(IO$GetPkt(io, 2, rec, 0, &nrecs, &value))
	{
	    /* Go forth and blit my good man */
	    ENTER_KERNEL_CRITICAL_SECTION();
	    s->blit_m(st, s->w->owner, s->sid, rec->base);
	    LEAVE_KERNEL_CRITICAL_SECTION();
	    /* Ack the packet */
	    IO$PutPkt(io, nrecs, rec, value, 0);
	}
#endif /* 0 */
    }
}

uint32_t blit_ava(FB_st *st, 
		  FB_StreamID sid, 
		  FBBlit_Rec *rec, 
		  word_t padding1,
		  word_t padding2,
		  dcb_ro_t *dcb) {

    return (uint32_t) -1;

}

static void old_blit_ava(FB_st *st, dcb_ro_t *dcb,
		     FB_StreamID sid, 
		     IO_Rec *rec, uint32_t nrecs)
{

  FB_Stream *s = &st->stream[sid];
  FB_Window *w = s->w;
      
  uchar_t  *buf;
  word_t  len;

  pixel16_t  *pmagptr;
  tile16_t    *bptr;
  tag_t    *clipptr;
  
  word_t  datalen;

  word_t  ntiles;
  word_t x, y;
  word_t tagword;

  if (SECURE && w->owner != dcb) return;
  if (nrecs != 1) return;

  buf = rec->base;
  len = rec->len;
  
  datalen = buf[len-5] | (buf[len-6]<<8);
  ntiles  = (datalen/128);
  
  x = buf[len-16];
  y = buf[len-15];
  
  if( x>= (w->width >> 3)) return;

  if (y < (w->height >> 3)) {
    
    pmagptr = w->pmag + (y * FB_WIDTH * 8) + (x * 8);
    clipptr = w->clip + (y * FB_WIDTH * 8) + (x * 8);

    bptr = (tile16_t *)buf;
    tagword  = w->tagword;

    ntiles = MIN(ntiles, (w->width>>3) - x);
    blit_tile_to_raster (bptr, pmagptr, clipptr, tagword, ntiles); 

  }

  /* Do the accounts */
  s->credits -= ntiles;
}

void blit_init(FB_st *st)
{
    extern void blit_raster_stub();
    extern void blit_ava_stub();
    extern void blit_dfs_stub();
    extern void blit_video_stub();
    extern void blit_dfs_8_stub();
    extern void blit_raster_8_stub();

    /* Init the blit methods */
    st->blit_fns[FB_Protocol_Bitmap16] = blit_raster;
    st->blit_fns[FB_Protocol_AVA16]    = blit_ava;
    st->blit_fns[FB_Protocol_DFS16]    = blit_dfs;
    st->blit_fns[FB_Protocol_Video16]  = blit_video;
    st->blit_fns[FB_Protocol_DFS8]     = blit_dfs_8;
    st->blit_fns[FB_Protocol_Bitmap8]  = blit_raster_8;

#ifdef EB164
    st->blit_stub_fns[FB_Protocol_Bitmap16] = blit_raster_stub;
    st->blit_stub_fns[FB_Protocol_AVA16]    = blit_ava_stub;
    st->blit_stub_fns[FB_Protocol_DFS16]    = blit_dfs_stub;
    st->blit_stub_fns[FB_Protocol_Video16]  = blit_video_stub;
    st->blit_stub_fns[FB_Protocol_DFS8]     = blit_dfs_8_stub;
    st->blit_stub_fns[FB_Protocol_Bitmap8]  = blit_raster_8_stub;
#endif
    
    /* Set up privileged sections */
    
    {
	CallPriv_AllocCode rc;
	FB_Protocol p;
	CallPriv_clp cp = IDC_OPEN("sys>CallPrivOffer", CallPriv_clp);
	
	for(p = FB_Protocol_Bitmap16; p <= FB_Protocol_Bitmap8; p++) {
	    
#ifdef EB164
	    rc = CallPriv$Allocate(cp, st->blit_stub_fns[p], st, &st->blit_cps[p]);
#else
	    rc = CallPriv$Allocate(cp, st->blit_fns[p], st, &st->blit_cps[p]);
#endif
	    if(rc != CallPriv_AllocCode_Ok) {
		printf("Error initialising callpriv %d!\n");	      
	    }
	}
	
    }
    
}

