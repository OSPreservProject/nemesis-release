/**************************************************************************\
*          Copyright (c) 1995 INRIA Sophia Antipolis, FRANCE.              *
*                                                                          *
* Permission to use, copy, modify, and distribute this material for any    *
* purpose and without fee is hereby granted, provided that the above       *
* copyright notice and this permission notice appear in all copies.        *
* WE MAKE NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY OF THIS     *
* MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS", WITHOUT ANY EXPRESS   *
* OR IMPLIED WARRANTIES.                                                   *
\**************************************************************************/
/**************************************************************************\
*                                                                          *
*  File              : tcpu_xsum.c                                         *
*  Author            : Torsten Braun                                       *
*  Last modification : July 27, 1995                                       *
\**************************************************************************/


#include <nemesis.h>
#include <stdio.h>

#include <FlowMan.ih>
#include "inet.h"

/********************************* TCP checksum *******************************/

#define PSEUDO_HDR_SIZE 12
#define TCPHDRSIZE sizeof(struct tcphdr)

u_int TCP_xsum_init (struct tcpiphdr *tcpip, u_int xsum)
{
   unsigned char hdr[PSEUDO_HDR_SIZE+TCPHDRSIZE];
   unsigned char *ptr;
   unsigned int pxsum;
   unsigned char off,i,j;

   j=0;
   ptr=(unsigned char *)(&(tcpip->ti_src.s_addr));
   for (i=0;i<sizeof(tcpip->ti_src.s_addr);i+=2) {
        xsum += (ptr[i] << 8) + ptr[i+1];
   }

   ptr=(unsigned char *)(&(tcpip->ti_dst.s_addr));
   for (i=0;i<sizeof(tcpip->ti_src.s_addr);i+=2) {
        xsum += (ptr[i] << 8) + ptr[i+1];
   }

   xsum+=IP_PROTO_TCP; /* this is already a low part of 16 bit value, high is 0 */
   xsum+=tcpip->ti_len; /* this is OK because it's stored in host order. Once 
                           the checksum is caluclated it has to be stored in
                           network order. Horrible, fix it!!! */

   ptr=(unsigned char *)(&(tcpip->ti_t));
   off=(ptr[12]>>4) * 4;
   for (i=0;i<off;i+=2) {
        xsum += (ptr[i] << 8) + ptr[i+1];
   }
   
  return (xsum);
}
  

u_int TCP_xsum_final (u_int xsum)
{
   xsum  = (xsum >> 16) + (xsum & 0xFFFF);    /*  Add high-16 to low-16 */
   xsum += (xsum >>  16);                     /*  Add carry             */
   xsum  = (~xsum) & 0xffff;                  /*  One's complement      */
   return(xsum);
}

u_int in_cksum(mp, len)
	unsigned char   *mp;
	int             len;
{
    	register unsigned char *w;
	register u_int  sum;		/* Assume u_int > 16 bits */
	register int    nwords;
	
	sum = 0;
	
	w = mp;
        
	nwords = len >> 1;

	while (nwords--) {
            /****
	      sum += *(ushort *) w;
	      sum+= (*w)<<8 + *(w+1);
	      sum += ntohs (*(ushort *) w);
	      ****/
//	    printf("IN CKSUM adding %02x%02x \n",*w, *(w+1) );

            sum+= ((*w)<<8) | *(w+1);
	    w += 2;
	}
        if (len &1)
	    sum+= ((*w)<<8)| 0;
	
        return (sum);

}

