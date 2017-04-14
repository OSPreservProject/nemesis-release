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
 *  Modified for 386 by Jim Kingdon, Cygnus Support.
 *  Modified for Nemesis/ELF by Stephen Early.
 *    Assembly code split out into gdb-asm.S
 *  $Id: i386-stub.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 *
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a trap #1.
 *
 *  The external function exceptionHandler() is
 *  used to attach a specific handler to a specific 386 vector number.
 *  It should use the same privilege level it runs at.  It should
 *  install it as an interrupt gate so that interrupts are masked
 *  while the handler runs.
 *  Also, need to assign exceptionHook and oldExceptionHook.
 *
 *  Because gdb will sometimes write to the stack area to execute function
 *  calls, this program cannot rely on using the supervisor stack so it
 *  uses it's own stack area reserved in the int array remcomStack.
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
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
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

#include <kernel.h>
#include "i386-stub.h"
#include "asm.h"
#include "string.h"

/************************************************************************
 *
 * external low-level support routines
 */
extern putDebugChar();   /* write a single character      */
extern getDebugChar();   /* read and return a single char */

extern void exceptionHandler();  /* assign an exception handler */
int stub_debug_flag=0;

/************************************************************************
 *
 * external assembly support routines
 */
extern Function catchException0;
extern Function catchException1;
extern Function catchException3;
extern Function catchException4;
extern Function catchException5;
extern Function catchException6;
extern Function catchException7;
extern Function catchException8;
extern Function catchException9;
extern Function catchException10;
extern Function catchException11;
extern Function catchException12;
extern Function catchException13;
extern Function catchException14;
extern Function catchException16;


/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 400

static char initialized;  /* boolean flag. != 0 means we've been initialized */

static const char hexchars[]="0123456789abcdef";

/* Number of bytes of registers.  */
int registers[NUMREGBYTES/4];

#define STACKSIZE 10000
static int remcomStack[STACKSIZE/sizeof(int)];
int *stackPtr = &remcomStack[STACKSIZE/sizeof(int) - 1];

extern void
return_to_prog();

#define BREAKPOINT() __asm__("int $3");

/* Put the error code here just in case the user cares.  */
int gdb_i386errcode;
/* Likewise, the vector number here (since GDB only gets the signal
   number through the usual means, and that's not very specific).  */
int gdb_i386vector = -1;

static int hex(ch)
char ch;
{
  if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
  if ((ch >= '0') && (ch <= '9')) return (ch-'0');
  if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
  return (-1);
}


/* scan for the sequence $<data>#<checksum>     */
static void getpacket(buffer)
char * buffer;
{
  unsigned char checksum;
  unsigned char xmitcsum;
  int  i;
  int  count;
  char ch;

  do {
    /* wait around for the start character, ignore all other characters */
    while ((ch = (getDebugChar() & 0x7f)) != '$');
    checksum = 0;
    xmitcsum = -1;

    count = 0;

    /* now, read until a # or end of buffer is found */
    while (count < BUFMAX) {
      ch = getDebugChar() & 0x7f;
      if (ch == '#') break;
      checksum = checksum + ch;
      buffer[count] = ch;
      count = count + 1;
      }
    buffer[count] = 0;

    if (ch == '#') {
      xmitcsum = hex(getDebugChar() & 0x7f) << 4;
      xmitcsum += hex(getDebugChar() & 0x7f);
#ifdef REMOTEDEBUG
      if ((remote_debug ) && (checksum != xmitcsum)) {
        fprintf (gdb ,"bad checksum.  My count = 0x%x, sent=0x%x. buf=%s\n",
		 checksum,xmitcsum,buffer);
      }
#endif

      if (checksum != xmitcsum) putDebugChar('-');  /* failed checksum */
      else {
	 putDebugChar('+');  /* successful transfer */
	 /* if a sequence char is present, reply the sequence ID */
	 if (buffer[2] == ':') {
	    putDebugChar( buffer[0] );
	    putDebugChar( buffer[1] );
	    /* remove sequence chars from buffer */
	    count = strlen(buffer);
	    for (i=3; i <= count; i++) buffer[i-3] = buffer[i];
	 }
      }
    }
  } while (checksum != xmitcsum);

}

/* send the packet in buffer - exported for use in stub-support.c  */
void putpacket(buffer)
char * buffer;
{
  unsigned char checksum;
  int  count;
  char ch;

  /*  $<packet info>#<checksum>. */
  do {
  putDebugChar('$');
  checksum = 0;
  count    = 0;

  while ((ch=buffer[count])) {
    if (! putDebugChar(ch)) return;
    checksum += ch;
    count += 1;
  }

  putDebugChar('#');
  putDebugChar(hexchars[checksum >> 4]);
  putDebugChar(hexchars[checksum % 16]);

  } while ((getDebugChar() & 0x7f) != '+');
}

static char  remcomInBuffer[BUFMAX];
static char  remcomOutBuffer[BUFMAX];
static short error;



/* convert the memory pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
char* mem2hex(mem, buf, count)
char* mem;
char* buf;
int   count;
{
      int i;
      unsigned char ch;

      /* Check we have access rights. */
      /* Doesn't handle mem+count that straddles a page
       * boundary, but GDB seems to align reads to 20 byte boundaries. */
      if (k_st.mmu_ok)
      {
	  word_t pte;

	  pte = ntsc_trans((word_t)mem >> PAGE_WIDTH);
	  if (pte == MEMRC_BADVA || !IS_VALID_PTE(pte))
	  {
	      strcpy(buf, "E03");
	      return buf+3;
	  }
      }

      for (i=0; i < count; i++)
      {
          ch = *mem++;
          *buf++ = hexchars[ch >> 4];
          *buf++ = hexchars[ch % 16];
      }

      *buf = 0;
      return(buf);
}

/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
char* hex2mem(buf, mem, count)
char* buf;
char* mem;
int   count;
{
      int i;
      unsigned char ch;

      /* Check we have access rights. */
      /* Doesn't handle mem+count that straddles a page
       * boundary, but GDB seems to align writes to 20 byte boundaries. */
      if (k_st.mmu_ok)
      {
	  word_t pte;

	  pte = ntsc_trans((word_t)mem >> PAGE_WIDTH);
	  if (pte == MEMRC_BADVA || !IS_VALID_PTE(pte))
	      return mem;
      }

      for (i=0; i < count; i++)
      {
          ch = hex(*buf++) << 4;
          ch = ch + hex(*buf++);
          *mem++ = ch;
      }

      return mem;
}

/* this function takes the 386 exception vector and attempts to
   translate this number into a unix compatible signal value */
int computeSignal( exceptionVector )
int exceptionVector;
{
  int sigval;
  switch (exceptionVector) {
    case 0 : sigval = 8; break; /* divide by zero */
    case 1 : sigval = 5; break; /* debug exception */
    case 3 : sigval = 5; break; /* breakpoint */
    case 4 : sigval = 16; break; /* into instruction (overflow) */
    case 5 : sigval = 16; break; /* bound instruction */
    case 6 : sigval = 4; break; /* Invalid opcode */
    case 7 : sigval = 8; break; /* coprocessor not available */
    case 8 : sigval = 7; break; /* double fault */
    case 9 : sigval = 11; break; /* coprocessor segment overrun */
    case 10 : sigval = 11; break; /* Invalid TSS */
    case 11 : sigval = 11; break; /* Segment not present */
    case 12 : sigval = 11; break; /* stack exception */
    case 13 : sigval = 11; break; /* general protection */
    case 14 : sigval = 11; break; /* page fault */
    case 16 : sigval = 7; break; /* coprocessor error */
    default:
      sigval = 7;         /* "software generated" */
  }
  return (sigval);
}

/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD AN INT */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/
int hexToInt(char **ptr, int *intValue)
{
    int numChars = 0;
    int hexValue;

    *intValue = 0;

    while (**ptr)
    {
        hexValue = hex(**ptr);
        if (hexValue >=0)
        {
            *intValue = (*intValue <<4) | hexValue;
            numChars ++;
        }
        else
            break;

        (*ptr)++;
    }

    return (numChars);
}

/*
 * This function does all command procesing for interfacing to gdb.
 */
void handle_exception(int exceptionVector)
{
  int    sigval=0;
  int    addr, length;
  char * ptr;
  int    newPC;

  if (stub_debug_flag) {
      /* We're already in the debugger, so we've just hit a bad
         address. Send an error reply */
      strcpy(remcomOutBuffer, "EBARFBARF");
      putpacket(remcomOutBuffer);

  } else {
      gdb_i386vector = exceptionVector;


      stub_debug_flag=1; /* Redirect console output through gdb */
#ifdef REMOTE_DEBUG
      if (remote_debug) printf("vector=%d, sr=0x%x, pc=0x%x\n",
			       exceptionVector,
			       registers[ PS ],
			       registers[ PC ]);
#endif
      
      /* reply to host that an exception has occurred */
      sigval = computeSignal( exceptionVector );
      remcomOutBuffer[0] = 'S';
      remcomOutBuffer[1] =  hexchars[sigval >> 4];
      remcomOutBuffer[2] =  hexchars[sigval % 16];
      remcomOutBuffer[3] = 0;
      
      putpacket(remcomOutBuffer);
  }
  
  while (1) {
    error = 0;
    remcomOutBuffer[0] = 0;
    getpacket(remcomInBuffer);
    switch (remcomInBuffer[0]) {
      case '?' :   remcomOutBuffer[0] = 'S';
                   remcomOutBuffer[1] =  hexchars[sigval >> 4];
                   remcomOutBuffer[2] =  hexchars[sigval % 16];
                   remcomOutBuffer[3] = 0;
                 break;
      case 'd' : 
#ifdef REMOTE_DEBUG
	  remote_debug = !(remote_debug);  /* toggle debug flag */
#endif
                 break;
      case 'g' : /* return the value of the CPU registers */
                mem2hex((char*) registers, remcomOutBuffer, NUMREGBYTES);
                break;
      case 'G' : /* set the value of the CPU registers - return OK */
                hex2mem(&remcomInBuffer[1], (char*) registers, NUMREGBYTES);
                strcpy(remcomOutBuffer,"OK");
                break;

      /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
      case 'm' :
		    /* TRY TO READ %x,%x.  IF SUCCEED, SET PTR = 0 */
                    ptr = &remcomInBuffer[1];
                    if (hexToInt(&ptr,&addr))
                        if (*(ptr++) == ',')
                            if (hexToInt(&ptr,&length))
                            {
                                ptr = 0;
                                mem2hex((char*) addr, remcomOutBuffer, length);
                            }

                    if (ptr)
                    {
		      strcpy(remcomOutBuffer,"E01");
		    }
	          break;

      /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
      case 'M' :
		    /* TRY TO READ '%x,%x:'.  IF SUCCEED, SET PTR = 0 */
                    ptr = &remcomInBuffer[1];
                    if (hexToInt(&ptr,&addr))
                        if (*(ptr++) == ',')
                            if (hexToInt(&ptr,&length))
                                if (*(ptr++) == ':')
                                {
                                    if (hex2mem(ptr, (char*) addr, length) ==
					((char*)addr) + length)
				        strcpy(remcomOutBuffer,"OK");
				    else
					strcpy (remcomOutBuffer, "E03");
                                    ptr = 0;
                                }
                    if (ptr)
                    {
		      strcpy(remcomOutBuffer,"E02");
		    }
                break;

     /* cAA..AA    Continue at address AA..AA(optional) */
     /* sAA..AA   Step one instruction from AA..AA(optional) */
     case 'c' :
     case 's' :
          /* try to read optional parameter, pc unchanged if no parm */
         ptr = &remcomInBuffer[1];
         if (hexToInt(&ptr,&addr))
             registers[ PC ] = addr;

          newPC = registers[ PC];

          /* clear the trace bit */
          registers[ PS ] &= 0xfffffeff;

          /* set the trace bit if we're stepping */
          if (remcomInBuffer[0] == 's') registers[ PS ] |= 0x100;

          /*
           * If we found a match for the PC AND we are not returning
           * as a result of a breakpoint (33),
           * trace exception (9), nmi (31), jmp to
           * the old exception handler as if this code never ran.
           */
#if 0
	  /* Don't really think we need this, except maybe for protection
	     exceptions.  */
                  /*
                   * invoke the previous handler.
                   */
                  if (oldExceptionHook)
                      (*oldExceptionHook) (frame->exceptionVector);
                  newPC = registers[ PC ];    /* pc may have changed  */
#endif /* 0 */

	  return_to_prog(); /* this is a jump */

          break;

      /* kill the program - reboot */
      case 'k' :  /* do nothing */
	  /* We must ack now, 'cos re_enter_rom will never return */
	  putDebugChar('+');
	  putDebugChar('+');
	  putDebugChar('+');
	  putDebugChar('+');
	  k_reboot();
	  break;
      } /* switch */

    /* reply to the request */
    putpacket(remcomOutBuffer);
    }
}

/* this function is used to set up exception handlers for tracing and
   breakpoints */
void set_debug_traps()
{
#if 0
    extern void remcomHandler();

  stackPtr  = &remcomStack[STACKSIZE/sizeof(int) - 1];

  exceptionHandler (0, catchException0);
  exceptionHandler (1, catchException1);
  exceptionHandler (3, catchException3);
  exceptionHandler (4, catchException4);
  exceptionHandler (5, catchException5);
  exceptionHandler (6, catchException6);
  exceptionHandler (7, catchException7);
  exceptionHandler (8, catchException8);
  exceptionHandler (9, catchException9);
  exceptionHandler (10, catchException10);
  exceptionHandler (11, catchException11);
  exceptionHandler (12, catchException12);
  exceptionHandler (13, catchException13);
/* We don't catch exception 14 (page fault) any more. */
/*  exceptionHandler (14, catchException14); */
  exceptionHandler (16, catchException16);

#if 0
  if (exceptionHook != remcomHandler)
  {
      oldExceptionHook = exceptionHook;
      exceptionHook    = remcomHandler;
  }
#endif /* 0 */

#endif /* 0 */
  initialized = 1;

}

/* This function will generate a breakpoint exception.  It is used at the
   beginning of a program to sync up with a debugger and can be used
   otherwise as a quick means to stop program execution and "break" into
   the debugger. */

void breakpoint()
{
  if (initialized)
#if 0
    handle_exception(3);
#else
    BREAKPOINT();
#endif
}
