/*
*****************************************************************************
**                                                                          *
**  XXX Copyright 1997, University of Cambridge Computer Laboratory         *
**                                                                          *
**  XXX All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/nemesis/ugdb
** 
** FUNCTIONAL DESCRIPTION:
** 
**      User-space stub for remote debugging for domains.
** 
** ENVIRONMENT: 
**
**      User space.
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <exceptions.h>
#include <string.h>
#include <time.h>
#include <ecs.h>

#include <VPMacros.h>
#include <IDCMacros.h>

#include "GDB_st.h"



/* Define DEBUG (manually or in config) to turn on tracing. */
#define DEBUG
#ifdef DEBUG
#warning UGDB Debugging on: this currently uses eprintf
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

/****************************************************************************
  
  THIS SOFTWARE IS NOT COPYRIGHTED  
  
  HP offers the following for use in the public domain.  HP makes no
  warranty with regard to the software or it's performance and the 
   user accepts the software "AS IS" with all faults.

   HP DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD
   TO THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

****************************************************************************/

/****************************************************************************
 *  Header: remcom.c,v 1.34 91/03/09 12:29:49 glenne Exp $                   
 *
 *  Module name: remcom.c $  
 *  Revision: 1.34 $
 *  Date: 91/03/09 12:29:49 $
 *  Contributor:     Lake Stevens Instrument Division$
 *  
 *  Description:     low level support for gdb debugger. $
 *
 *  Considerations:  only works on target hardware $
 *
 *  Written by:      Glenn Engel $
 *  ModuleState:     Experimental $ 
 *
 *  NOTES:           See Below $
 * 
 *************
 *
 *    The following gdb commands are supported:
 * 
 * command          function                               Return value
 * 
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 * 
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 * 
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 * 
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 * 
 *    k             kill
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 * 
 * All commands and responses are sent with a packet which includes a 
 * checksum.  A packet consists of 
 * 
 * $<packet info>#<checksum>.
 * 
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: <two hex digits computed as modulo 256 sum of <packetinfo>>
 * 
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 * 
 * Example:
 * 
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 * 
 ****************************************************************************/


static GDB_Debug_fn Debug_m;
static GDB_op gdb_op = { Debug_m };

static GDBMod_New_fn New_m;
static GDBMod_op gdbmod_op = { New_m };
static const GDBMod_cl gdbmod_cl = { &gdbmod_op, NULL };
CL_EXPORT(GDBMod, GDBMod, gdbmod_cl);


/* Internal function prototypes */
static void    putpacket(GDB_st *);
static void    getpacket(GDB_st *);
static int     hex(char);
static char   *mem2hex(char *, char *, int);
static char   *hex2mem(char *, char *, int);
static void    debug_error(GDB_st *, char *, char *);
static int     hex2word(char **, word_t *);




/* read-only data */
static const char hexchars[]="0123456789abcdef";


static GDB_cl *New_m(GDBMod_cl *self, Rd_cl *rd, Wr_cl *wr)
{
    GDB_st *st; 
    NOCLOBBER Rd_cl *NOCLOBBER saferd;

    TRC(eprintf("GDBMod (%qx): reader is at %p, writer at %p\n", 
		RO(Pvs(vp))->id, rd, wr));

    /* Malloc a bit of space for state */
    if(!(st = Heap$Malloc(Pvs(heap), sizeof(*st)))) {
	eprintf("UGDB: out of cheese error.\n");
	return ((GDB_cl *)NULL);
    }


    /* Intialise the state */
    bzero(st, sizeof(*st));

/* XXX nasty hack to find the serial line */
#ifdef __ix86__
#define SERIAL_LINE "dev>serial0>rd"
#else
#define SERIAL_LINE "dev>serial1>rd"
#endif

    if(!rd) { 
	/* Don't have a sensible stdin: try the serial line */
	saferd = rd;
	TRC(eprintf("GDBMod: trying to get a reader...\n"));
	TRY {
	    saferd  = IDC_OPEN (SERIAL_LINE, Rd_clp);
	} CATCH_Binder$Error(problem) {
	    if(problem == Binder_Problem_ServerRefused) {
		TRC(eprintf("GDBMod: serial reader is in use. exiting.\n"));
		PAUSE(FOREVER);
	    } else {
		TRC(eprintf("GDBMod: some kind of binder error.\n"));
		RERAISE;
	    }
	} ENDTRY;
	rd = saferd;
	TRC(eprintf("Ok, got reader at %p\n", rd));
    }
    st->rd                   = rd;
    st->wr                   = wr;
    st->remote_debug         = False; 

    CL_INIT(st->cl, &gdb_op, st);
    TRC(eprintf("GDBMod: setup state (at %p)\n", st));

    return &st->cl;
}


/*
 * This function does all command procesing for interfacing to gdb.
 */
static bool_t Debug_m(GDB_cl *self, addr_t cx, GDB_Reason why)
{
    word_t addr, length;
    GDB_st *st  = self->st; 
    char *ptr;


    if(!arch_init_state(st, cx)) {
	eprintf("GDB$Debug: architecture specific init failed.\n");
	PAUSE(FOREVER);
    }

    why = 3;
    if (why != GDB_Reason_BPT) {
	/* reply to host that an exception has occurred */
	st->outbuf[0] = 'S';
	st->outbuf[1] =  hexchars[why >> 4];
	st->outbuf[2] =  hexchars[why % 16];
	st->outbuf[3] =  0;
	
	putpacket(st);
    }

    TRC(eprintf("UGDB: Stub entered ($k#6b to halt)\n"));

    while(True) { 
	st->error = 0;
	st->outbuf[0] = 0;
	getpacket(st);
	if (st->remote_debug) {
	    eprintf("UGDB: got packet %s\n", st->inbuf);
	}
	
	switch (st->inbuf[0]) {
	case '?' :   st->outbuf[0] = 'S';
	    st->outbuf[1] =  hexchars[why >> 4];
	    st->outbuf[2] =  hexchars[why % 16];
	    st->outbuf[3] = 0;
	    break; 

	case 'd' : st->remote_debug = 
		       !(st->remote_debug);  /* toggle debug flag */
	    break; 

	case 'g' : /* return the value of the CPU registers */
	    mem2hex((char *)st->registers, st->outbuf, st->numregbytes);
	    break;

	case 'G' : /* set the value of the CPU registers - return OK */
	    hex2mem(&st->inbuf[1], (char *)st->registers, st->numregbytes);
	    strcpy(st->outbuf,"OK");
	    break;
      
	    /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
	case 'm' : 
	    /* TRY TO READ %x,%x.  IF SUCCEED, SET PTR = 0 */
	    ptr = &st->inbuf[1];
	    if (hex2word(&ptr,&addr))
		if (*(ptr++) == ',')
		    if (hex2word(&ptr,&length))  {
			ptr = 0;
			mem2hex((char*) addr, st->outbuf, length);
		    }
      
	    if (ptr) {
		strcpy(st->outbuf, "E01");
		debug_error(st, "malformed read memory command: %s",
			    st->inbuf);
	    }     
	    break;
      
	    /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
	case 'M' : 
	    /* TRY TO READ '%x,%x:'.  IF SUCCEED, SET PTR = 0 */
	    ptr = &st->inbuf[1];
	    if (hex2word(&ptr,&addr))
		if (*(ptr++) == ',')
		    if (hex2word(&ptr,&length))
			if (*(ptr++) == ':') {
			    hex2mem(ptr, (char*) addr, length);
			    ptr = 0;
			    strcpy(st->outbuf, "OK");
			}
	    if (ptr) {
		strcpy(st->outbuf, "E02");
		debug_error(st, "malformed write memory command: %s",
			    st->inbuf);
	    }     
	    break;

	    /*
	      query sect offs qOffsets        Get section offsets.  Reply is
	      Text=xxx;Data=yyy;Bss=zzz
	      */
	case 'q':
	{
#if 0
	    extern word_t _ftext;
	    extern word_t _fdata;
	    extern word_t _fbss;

	    word_t val;

	    ptr = st->outbuf;
	    strcpy(ptr, "Text="); ptr += 5;
	    val = &_ftext;
	    ptr = mem2hex(&val, ptr, sizeof(val));
	    strcpy(ptr, ";Data="); ptr += 6;
	    val = &_fdata;
	    ptr = mem2hex(&val, ptr, sizeof(val));
	    strcpy(ptr, ";Bss="); ptr += 5;
	    val = &_fbss;
	    ptr = mem2hex(&val, ptr, sizeof(val));
	
	    sprintf(st->outbuf, "Text=%lx;Data=%lx;Bss=%lx",
		    &_ftext, &_fdata, &_fbss); 
#endif /* 0 */
	    strcpy(st->outbuf, "Text=0;Data=0;Bss=0");
	}
	break;

	/*
	  set thread      Hct...          Set thread for subsequent operations.
	  c = 'c' for thread used in step and
	  continue; t... can be -1 for all
	  threads.
	  c = 'g' for thread used in other
	  operations.  If zero, pick a thread,
	  any thread.
	  reply           OK              for success
	  ENN             for an error.
	  */
	case 'H':
	    strcpy(st->outbuf, "OK");
	    break;

	    /* kill the program == halt the machine */
	case 'k' :  
	    ntsc_dbgstop();  /* yet another debugger... */
	    break;

	    /* Don't understand */
	default:
	    strcpy(st->outbuf, "E01");
	    break;

	} /* switch */ 
    
	/* reply to the request */
	putpacket(st);
	if (st->remote_debug) 
	    eprintf("UGDB: put packet %s\n", st->outbuf);
    }
}



/* Internal routines */

static int hex(char ch)
{
    if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
    if ((ch >= '0') && (ch <= '9')) return (ch-'0');
    if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
    return (-1);
}



/* scan for the sequence $<data>#<checksum> from st->inbuf   */
static void getpacket(GDB_st *st)
{
    char *buffer = st->inbuf;
    uchar_t checksum, xmitcsum;
    int i, count;
    char ch;
  
    do {
	/* wait around for the start character, ignore all other characters */
	while ((ch = (Rd$GetC(st->rd) & 0x7f)) != '$') ;

	checksum = 0;
	xmitcsum = -1;

	count = 0;
    
	/* now, read until a # or end of buffer is found */
	while (count < st->bufmax) {
	    ch = Rd$GetC(st->rd) & 0x7f;
	    if (ch == '#') break;
	    checksum = (checksum + ch);
	    buffer[count] = ch;
	    count = count + 1;
	}
	buffer[count] = 0;

	if (ch == '#') {
	    xmitcsum = hex(Rd$GetC(st->rd) & 0x7f) << 4;
	    xmitcsum += hex(Rd$GetC(st->rd) & 0x7f);

	    if ((st->remote_debug ) && (checksum != xmitcsum)) {
		eprintf("GDB: bad checksum '%s'\n", &buffer[0]);
		eprintf("computed=%x sent=%x\n",
			(uint32_t)checksum, (uint32_t)xmitcsum);
	    }
      
	    if (checksum != xmitcsum) /* failed checksum */ 
		Wr$PutC(st->wr, '-'); 
	    else {
		Wr$PutC(st->wr, '+');  /* successful transfer */
		/* if a sequence char is present, reply the sequence ID */
		if (buffer[2] == ':') {
		    Wr$PutC(st->wr,  buffer[0]);
		    Wr$PutC(st->wr,  buffer[1]);
		    /* remove sequence chars from buffer */
		    count = strlen(buffer);
		    for (i=3; i <= count; i++) buffer[i-3] = buffer[i];
		} 
	    } 
	} 
    } while (checksum != xmitcsum);
}


/* send the packet in buffer.  The host get's one chance to read it.  
   This routine does not wait for a positive acknowledge.  */
static void putpacket(GDB_st *st)
{
    char *buffer = st->outbuf;
    unsigned char checksum;
    int  count;
    char ch;

    /*  $<packet info>#<checksum>. */
    do {
	Wr$PutC(st->wr, '$');
	checksum = 0;
	count    = 0;
  
	while ( (ch=buffer[count]) ) {
	    Wr$PutC(st->wr, ch);
	    checksum += ch;
	    count += 1;
	}

	Wr$PutC(st->wr, '#');
	Wr$PutC(st->wr, hexchars[checksum >> 4]);
	Wr$PutC(st->wr, hexchars[checksum % 16]);

    } while (1 == 0);  /* (Rd$GetC(st->rd) != '+'); */
    
    Wr$Flush(st->wr);
}


static void debug_error(GDB_st *st, char *format, char *parm)
{
    if(st->remote_debug) 
	eprintf(format, parm);
}

/* convert the memory pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
static char *mem2hex(char *mem, char *buf, int count)
{
    int i;
    unsigned char ch;

    for (i=0; i < count; i++) {
	ch = *mem++;
	*buf++ = hexchars[ch >> 4];
	*buf++ = hexchars[ch % 16];
    }
    *buf = 0; 
    return(buf);
}

/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
static char *hex2mem(char *buf, char *mem, int count)
{
    int i;
    unsigned char ch;

    for (i=0; i < count; i++) {
	ch = hex(*buf++) << 4;
	ch = ch + hex(*buf++);
	*mem++ = ch;
    }
    return(mem);
}

/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD AN INT */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/
static int hex2word(char **ptr, word_t *intValue)
{
    int numChars = 0;
    int hexValue;
    
    *intValue = 0;

    while (**ptr) {
        hexValue = hex(**ptr);
        if (hexValue >=0) {
            *intValue = (*intValue <<4) | hexValue;
            numChars ++;
        }
        else
            break;
        
        (*ptr)++;
    }
    
    return (numChars);
}



