/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      net/TFTP/tftp.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implements TFTP.if
** 
** ENVIRONMENT: 
**
**      User-land
** 
** ID : $Id: tftp.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

/* This file is loosely based on BSD code containing the following copyright
   notice: */

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <nemesis.h>
#include <stdio.h>
#include <string.h>
#include <netorder.h>
#include <socket.h>

#include <kernel.h>

#include <TFTPMod.ih>
#include <TFTP.ih>

#ifdef TFTP_DOTS
#define DOTS(_x) _x
#else
#define DOTS(_x)
#endif

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

/*
 * Data types.
 */

typedef struct tftphdr {
  uint16_t	opcode;		/* packet type */
  union {
    uint16_t	block;		/* block # */
    int16_t	code;		/* error code */
    char	file[1];	/* request packet filename and mode */
  } u;
  uint8_t data[1];		/* data or error string */
} tftphdr_t;

#define th_block        u.block
#define th_code         u.code
#define th_file         u.file
#define th_msg		data

#define RRQ     	01      /* read request */
#define WRQ     	02      /* write request */
#define DATA    	03      /* data packet */
#define ACK     	04	/* acknowledgement */
#define ERROR   	05	/* error code */

#define SEGSIZE         512	/* data segment size */
#define PKTSIZE		(SEGSIZE+4)

#define TFTP_PORT	69

#define TIMEOUT_USECS	 1000000
#define MAX_TIMEOUT	32000000

#if 0
/* Useful UDP Ports */
#define PORT_TFTP		69
#define PORT_SUNRPC		111
#endif /* 0 */

/*
 * Module stuff
 */
static	TFTP_Get_fn Get_m;
static	TFTP_Put_fn Put_m;
static	TFTP_op	tftp_ms = {
  Get_m,
  Put_m
};
typedef struct {
  TFTP_cl cl;
  int fd;
} TFTP_st;


static	TFTPMod_New_fn New_m;
static	TFTPMod_op	ms = {
  New_m
};
static	const TFTPMod_cl	cl = { &ms, NULL };

CL_EXPORT (TFTPMod, TFTPMod, cl);

/* 
 * Prototypes
 */

static void	inet_gethostaddr (string_t host, struct in_addr *sin_addr);
  /* RAISES Context_NotFound, TypeSystem_Incompatible, Net_BadAF */

static word_t	build_request (word_t type, tftphdr_t *pkt, string_t file);
  /* always uses mode "octet" */

static void	nak (int fd, struct sockaddr_in *peer_sa, uint32_t error);

static void	synchnet (int fd);

/*---------------------------------------------------- Entry Points ----*/

int tftp_socket(void)
{
    struct sockaddr_in  my_sa;
    int fd, rc;

    TRC(printf("tftp_socket called\n");)

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
	{
	    printf ("TFTP: socket: %d\n", fd);
	    RAISE_TFTP$Failed(TFTP_Error_Other, 0);
	}

	TRC(printf("tftp_socket got summat\n"));

	bzero (&my_sa, sizeof (my_sa));
	my_sa.sin_family = AF_INET;
	rc = bind(fd, (struct sockaddr *) &my_sa, sizeof (my_sa));
	if (rc < 0)
	{
	    printf ("TFTP: bind: %d\n", rc);
	    RAISE_TFTP$Failed(TFTP_Error_Other, 0);
	}
	TRC(printf("TFTP: bound to socket, rc=%d\n", rc));

#if 0
	if (ioctl(fd, FIONBIO, &on) < 0)
	{
	    eprintf("ioctl(FIONBIO) failed\n");
	}
#endif /* 0 */

	return fd;
}


static TFTP_clp New_m(TFTPMod_clp self)
{
  TFTP_clp tftp;
  TFTP_st *st = Heap$Malloc(Pvs(heap), sizeof(*st));

  tftp = (TFTP_clp) st;

  CLP_INIT(tftp, &tftp_ms, st);

  st->fd = tftp_socket();
  
  return tftp;
}

static uint32_t 
Get_m (	TFTP_cl	       *self,
	string_t	host	/* IN */,
	string_t	file	/* IN */,
	Wr_clp		wr	/* IN */ )
{
  TFTP_st              *st = (TFTP_st *)self->st;

  uint8_t		txbuf [PKTSIZE];
  uint8_t		rxbuf [PKTSIZE];
  tftphdr_t    	       *pkt   = (tftphdr_t *) txbuf;
  tftphdr_t    	       *dp    = (tftphdr_t *) rxbuf; /* XXX do double buffer */

  uint32_t		NOCLOBBER bytes = 0;
  word_t		NOCLOBBER block = 1;

  uint32_t		size;
  struct sockaddr_in	from, peer_sa;
  int			from_len;

  int			fd = st->fd;

  int			NOCLOBBER rc;
  int			NOCLOBBER rcvd;
  bool_t		NOCLOBBER done_rrq = False;

  uint32_t		timeout;

  /* Pvs(out) = Pvs(err);*/

  /* XXX already done, no?? */
  /* fd = tftp_socket(host); */

  bzero (&peer_sa, sizeof (peer_sa));
  peer_sa.sin_family = AF_INET;
  peer_sa.sin_port   = hton16 ((uint16_t) TFTP_PORT);

  inet_gethostaddr (host, &(peer_sa.sin_addr));

  TRC (printf ("TFTP: got peer %s (%x)\n",
		host, ntoh32 (peer_sa.sin_addr.s_addr)));

  TRY
  {
    TRY
    {
      do
      {
	/* prepare the next packet in buf */
	if (!done_rrq)
	{
	  size = build_request (RRQ, pkt, file);
	  done_rrq = True;
	}
	else
	{
	  pkt->opcode   = hton16 ((uint16_t) ACK);
	  pkt->th_block = hton16 ((uint16_t) block);
	  size = 4;
	  block++;
	}
	
	timeout = TIMEOUT_USECS; 

      send_ack:
	/* send the rrq/ack packet */
	rc = sendto (fd, (char *)pkt, size, 0,
		     (struct sockaddr *)&peer_sa, sizeof (peer_sa));
	if (rc != size)
	{
	  printf("TFTP: sendto: %d\n", rc);
	  RAISE_TFTP$Failed(TFTP_Error_Other, bytes);
	}
	
	while (True) /* await the next data packet in sequence */
	{
	  struct timeval tv; fd_set fds; int nfds;
	  
	  tv.tv_usec = timeout; tv.tv_sec = 0;
	  FD_ZERO (&fds);
	  FD_SET  (fd, &fds);

	  /* select is being used here purely to time out.  The
	     traditional tftp implementation used SIGALARM, but
	     there's no way I'm implementing U**X signals over
	     Nemesis. */
	  
	  nfds = select (fd + 1, &fds, NULL, NULL, &tv);
	  if (nfds < 0)
	  {
	    printf ("TFTP: select: %d\n", nfds);
	    RAISE_TFTP$Failed (TFTP_Error_Other, bytes);
	  }
	  if (!nfds)
	  {
	    TRC (printf ("TFTP: get: rx timeout (%d secs)\n", timeout));
	    if (timeout >= MAX_TIMEOUT)
	    {
	      printf ("TFTP: transfer timed out\n");
	      RAISE_TFTP$Failed (TFTP_Error_Other, bytes);
	    }
	    else
	    {
	      timeout <<= 1;
	      goto send_ack;
	    }
	  }

	  /* we have data to read */
	  from_len = sizeof(from);
	  rcvd = recvfrom(fd, (char *)dp, PKTSIZE, 0,
			   (struct sockaddr *)&from, &from_len);
	  if (rcvd < 0)
	  {
	      printf ("TFTP: recvfrom: %d\n", rcvd);
	      RAISE_TFTP$Failed(TFTP_Error_Other, bytes);
	  }
	  
	  peer_sa.sin_port = from.sin_port;   /* now we've got peer's tfr id */
	  
	  dp->opcode   = ntoh16 (dp->opcode);
	  dp->th_block = ntoh16 (dp->th_block);
	  
	  TRC (printf ("TFTP: recv %d bytes from %x:%d; op=%d\n",
			rcvd, ntoh32 (from.sin_addr.s_addr),
			ntoh16 (from.sin_port), dp->opcode));
	  DOTS(Wr$PutC(Pvs(out), '.')); 
	  
	  if (dp->opcode == ERROR)
	  {
	    printf("TFTP: error code %d: %s\n", dp->th_code, dp->th_msg);
	    RAISE_TFTP$Failed(dp->th_code, bytes);
	  }
	  
	  if (dp->opcode == DATA)
	  {
	    if (dp->th_block == block) break; /* have next packet */
	    synchnet (fd);                    /* error: discard queued pkts */
	    if (dp->th_block == (block-1)) goto send_ack;  /* resend ack */
	  }
	  /* discard this packet */
	}

	/* we have the next data packet in sequence */
	Wr$PutChars(wr, dp->data, rcvd - 4);	/* XXX - do write-behind */
	size = rcvd - 4;
	bytes += size;
	
      } while (size == SEGSIZE);
    }
    CATCH_Wr$Failure (why)
    {
      nak(fd, &peer_sa, why + 100);
      RERAISE;
    }
    ENDTRY;
  }
  FINALLY
  {
      if (dp->opcode != ERROR)
      {
	  /* send the final ack */
	  pkt->opcode   = hton16 ((uint16_t) ACK);
	  pkt->th_block = hton16 ((uint16_t) block);
	  (void) sendto(fd, (char *)pkt, 4, 0,
			(struct sockaddr *)&peer_sa, sizeof (peer_sa));
      }
  }
  ENDTRY;
  
  DOTS(Wr$PutC(Pvs(out), '\n'));
  TRC (printf ("TFTP: received %d bytes from %s\n", bytes, host));
  
  return bytes;
}


static uint32_t 
Put_m (	TFTP_cl	       *self,
	string_t	host	/* IN */,
	string_t	file	/* IN */,
	Rd_clp		rd	/* IN */ )
{
  TFTP_st              *st = (TFTP_st *)self->st;
  uint8_t		txbuf [PKTSIZE];
  uint8_t		rxbuf [PKTSIZE];
  tftphdr_t    	       *pkt = txbuf;
  tftphdr_t    	       *ack = rxbuf;
  uint32_t		NOCLOBBER bytes = 0;
  word_t		NOCLOBBER block = 0;
  int32_t		size;
  struct sockaddr_in	from, peer_sa;
  int			from_len;
  int			fd = st->fd;
  int			NOCLOBBER rc;
  int			NOCLOBBER rcvd;
  uint32_t		timeout;

  TRC(printf ("TFTP: Put \"%s\" to \"%s\"\n", file, host));

  bzero (&peer_sa, sizeof (peer_sa));
  peer_sa.sin_family = AF_INET;
  peer_sa.sin_port   = hton16 ((uint16_t) TFTP_PORT);

  inet_gethostaddr (host, &(peer_sa.sin_addr));

  TRC (printf ("TFTP: got peer %s (%x)\n",
		host, ntoh32 (peer_sa.sin_addr.s_addr)));

  TRY
  {
    do
      {
	if (block == 0)
	  {
	    size = build_request (WRQ, pkt, file) - 4;
	  }
	else
	  {

	    /* Construct the next data packet in sequence */
	    size = Rd$GetChars (rd, pkt->data, SEGSIZE);
	    pkt->opcode = hton16((uint16_t)DATA);
	    pkt->th_block = hton16((uint16_t)block);
	    bytes += size;
	  }
	
	timeout = TIMEOUT_USECS; 

      send_data:
	/* send the WRQ/DATA packet */
	TRC (printf ("TFTP: send %d bytes to %x:%d; op=%d\n",
		     size+4, ntoh32 (peer_sa.sin_addr.s_addr),
		     ntoh16 (peer_sa.sin_port), 
		     pkt->opcode));
	rc = sendto (fd, (char *)pkt, size + 4, 0,
		     (struct sockaddr *)&peer_sa, sizeof (peer_sa));
	DOTS(Wr$PutC(Pvs(out), '.'));
	if (rc != size + 4)
	  {
	    printf ("TFTP: sendto: %d\n", rc);
	    RAISE_TFTP$Failed(TFTP_Error_Other, bytes);
	  }
	
	while (True) /* await the next ACK */
	  {
	    struct timeval tv;
	    fd_set         fds;
	    int	       	 nfds;
	  
	    tv.tv_usec = timeout; tv.tv_sec = 0;
	    FD_ZERO (&fds);
	    FD_SET  (fd, &fds);
	  
	    /* select is being used here purely to time out.  The
	       traditional tftp implementation used SIGALARM, but
	       there's no way I'm implementing U**X signals over
	       Nemesis. */
	  
	    nfds = select (fd + 1, &fds, NULL, NULL, &tv);
	    if (nfds < 0)
	    {
		printf("TFTP: select: %d\n", nfds);
		RAISE_TFTP$Failed(TFTP_Error_Other, bytes);
	    }
	    if (!nfds)
	    {
		TRC(printf("TFTP: get: rx timeout (%d secs)\n", timeout));
		if (timeout >= MAX_TIMEOUT)
		{
		    printf("TFTP: transfer timed out\n");
		    RAISE_TFTP$Failed(TFTP_Error_Other, bytes);
		}
		else
		{
		    timeout <<= 1;
		    goto send_data;
		}
	    }	  

	    /* we have data to read */
	    from_len = sizeof (from);
	    rcvd = recvfrom(fd, (char *)ack, PKTSIZE, 0,
			    (struct sockaddr *)&from, &from_len);
	    if (rcvd < 0)
	    {
		printf ("TFTP: recvfrom: %d\n", rcvd);
		RAISE_TFTP$Failed (TFTP_Error_Other, bytes);
	    }
	  
	    peer_sa.sin_port = from.sin_port;   /* now we've got peer's tfr id */
	  
	    ack->opcode   = ntoh16 (ack->opcode);
	    ack->th_block = ntoh16 (ack->th_block);
	  
	    TRC (printf ("TFTP: recv %d bytes from %x:%d; op=%d\n",
			 rcvd, ntoh32 (from.sin_addr.s_addr),
			 ntoh16 (from.sin_port), ack->opcode));
	  
	    if (ack->opcode == ERROR)
	      {
		printf("TFTP: error code %d: %s\n", 
		       ack->th_code, 
		       ack->th_msg);
		RAISE_TFTP$Failed (ack->th_code, bytes);
	      }
	  
	    if (ack->opcode == ACK)
	      {
		if (ack->th_block == block) break; /* have next ACK */

		synchnet (fd);	/* error: discard queued pkts */
		if (ack->th_block == (block-1)) 
		  goto send_data;	/* resend data */
	      }
	    /* discard this packet */
	  }
	block++;

      } while (size == SEGSIZE || block == 1);
  }
  FINALLY
  {
    /* Nothing for now */
  }
  ENDTRY;
  DOTS(Wr$PutC(Pvs(out), '\n'));
  TRC (printf ("TFTP: wrote %d bytes to %s\n", bytes, host));
  
  return bytes;
}

/*----------------------------------------------------------------------*/

/*
 *  Send a nak packet (error message).
 *  "error" is one of the standard TFTP codes, or a Wr$Failure reason
 *  offset by 100.
 */
static void
nak (int fd, struct sockaddr_in *peer_sa, uint32_t error)
{
  uint8_t	  buf [PKTSIZE];
  tftphdr_t	*tp = (tftphdr_t *) buf;
  char		 *msg;
  int		  length;
  int		  rc;
  
  tp->opcode  = hton16 ((uint16_t) ERROR);
  tp->th_code = hton16 ((uint16_t) error);

  switch (error)
  {
  case TFTP_Error_Other:           msg = "Undefined error code";   break;
  case TFTP_Error_FileNotFound:    msg = "File not found";         break;
  case TFTP_Error_AccessViolation: msg = "Access violation";       break;
  case TFTP_Error_NoSpace:         msg = "Disk full or allocation exceeded";
                                                                   break;
  case TFTP_Error_IllegalOp:       msg = "Illegal TFTP operation"; break;
  case TFTP_Error_UnknownTfrID:    msg = "Unknown transfer ID";    break;
  case TFTP_Error_FileExists:      msg = "File already exists";    break;
  case TFTP_Error_NoSuchUser:      msg = "No such user";           break;
  default:
    msg = "Rd/Wr failure";
    tp->th_code = hton16 ((uint16_t) TFTP_Error_Other);
  }

  strcpy (tp->th_msg, msg);
  length = strlen (msg) + 4;

  rc = sendto (fd, (char *)tp, length, 0,
	       (struct sockaddr *) peer_sa, sizeof (*peer_sa));

  if (rc != length)
    printf ("TFTP: nak: sendto: %d\n", rc);
}

void inet_gethostaddr (string_t host, struct in_addr *sin_addr)
  /* RAISES Context_NotFound, TypeSystem_Incompatible, Net_BadAF */
{
  unsigned int a,b,c,d;
  TRC(printf ("inet_gethostaddr: '%s' %p\n", host, sin_addr));

  if (sscanf(host, "%d.%d.%d.%d", &a,&b,&c,&d) != 4) 
    RAISE_TFTP$Failed (TFTP_Error_Other, 0);

  sin_addr->s_addr = a | b<<8| c<<16 | d<<24;
  TRC(printf("TFTP: addr is %x\n", sin_addr->s_addr));
}

#if 0
static void
inet_gethostaddr (string_t host, struct in_addr *sin_addr)
  /* RAISES Context_NotFound, TypeSystem_Incompatible, Net_BadAF */
{
  Context_clp	hosts = NAME_FIND ("hosts", Context_clp);
  Host_Info	info  = NAME_LOOKUP (host, hosts, Host_Info);

  if (info->addr.tag != Net_AF_IP)
  {
    printf ("inet_gethostaddr: unknown host %s\n", host);
    RAISE_Net_BadAF (info->addr.tag);
  }
  memcpy (sin_addr, &info->addr.u.AF_IP, sizeof (*sin_addr));
}
#endif /* 0 */

static word_t
build_request (word_t type, tftphdr_t *pkt, string_t file)
{
  word_t size = sizeof (pkt->opcode) + strlen (file) + 1 + strlen("octet");
  char  *cp   = pkt->th_file;

  if (size >= PKTSIZE)
  {
    printf ("TFTP: filename too long\n");
    RAISE_TFTP$Failed (TFTP_Error_Other, 0);
  }
  pkt->opcode = hton16 ((uint16_t) type);

  strcpy (cp, file);
  cp += strlen(file);
  *cp++ = 0;
  strcpy (cp, "octet");

  return size;
}

static void
synchnet (int fd)
{
  char			pkt [PKTSIZE];
  char                 *buf;
  struct sockaddr_in	from;
  int			fromlen;

  buf = pkt;

  while (True)
  {
    struct timeval tv;
    fd_set         fds;
    int	       	   nfds;
    
    tv.tv_sec = 0; tv.tv_usec = 0;
    FD_ZERO (&fds);
    FD_SET  (fd, &fds);
    
    /* select is being used here to replace ioctl (fd, FIONREAD, ..),
       which isn't implemented in Nemesis */
    
    nfds = select (fd + 1, &fds, NULL, NULL, &tv);
    if (nfds < 0)
    {
      printf ("TFTP: synchnet: select: %d\n", nfds);
      RAISE_TFTP$Failed (TFTP_Error_Other, 0);
    }
    if (!nfds)
      return;

    fromlen = sizeof (from);
    (void) recvfrom (fd, (char *)buf, sizeof (buf), 0,
		     (struct sockaddr *)&from, &fromlen);
  }
}

/* End of net/TFTP/tftp.c */
