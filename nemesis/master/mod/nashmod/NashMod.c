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
** ID : $Id: NashMod.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
**
*/

#include <nemesis.h>

#include <NashMod.ih>
#include <stdio.h>
#include <IDCMacros.h>
#include <time.h>
#include <ecs.h>
#include <CLine.ih>
#include "nashst.h"

#include <autoconf/nashlogin.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif


Nash_clp NewNash(bool_t runprofile);

static	NashMod_New_fn 		NashMod_New_m;

static	NashMod_op	ms = {
  NashMod_New_m
};

const static	NashMod_cl	cl = { &ms, NULL };

CL_EXPORT (NashMod, NashMod, cl);

static Closure_Apply_fn   PromptInterpreter;

static Closure_op ni_ms = { & PromptInterpreter };

const static Closure_cl ni_cl = { & ni_ms, NULL };

CL_EXPORT (Closure, NashInterpreter, ni_cl);

/*---------------------------------------------------- Entry Points ----*/

static Nash_clp NashMod_New_m (
	NashMod_cl	*self)

{
    return NewNash(0);
}

/*
 * End 
 */

void PromptInterpreter(Closure_clp self) {
    Nash_clp nash;
    char *buf;
    int nread;
    int leave;
    Rd_clp raw_in, cooked_rd;
    Wr_clp raw_out, cooked_wr;
    CLine_clp cline;
    CLineCtl_clp clinectl;
    /* Cook the command line */
    raw_in = Pvs(in);
    raw_out = Pvs(out);
    leave = 0;
    cline = NAME_FIND("modules>CLine", CLine_clp);
    cooked_rd = CLine$New(cline, Pvs(in) , Pvs(out), &cooked_wr, &clinectl);  
    Pvs(in) = cooked_rd;
    Pvs(out) = cooked_wr;
    buf = malloc(sizeof(char)*256);
    nash = NewNash(0);

#ifdef CONFIG_NASHLOGIN
    while (!leave) {
	
	printf("\n\nWelcome to %s's Nemesis\n\n%s login: ", NAME_FIND("conf>userid", string_t), gethostname());
	
	nread = Rd$GetLine(Pvs(in), buf, 255);
	if (nread) buf[nread-1] = 0;
	if (!strcmp(buf, "reboot")) {
	    printf("Rebooting\n");
	    Nash$ExecuteString(nash, "reboot");
	}
	
	if (!strcmp(buf, "b") || !strcmp(buf, "boot")) {
	    printf("Rebooting\n");
	    Nash$ExecuteString(nash, "b");
	}
	
	if (!strcmp(buf, "root")) {
	    leave = 1;
	} else
	if (!strcmp(buf, "user")) {
	    leave = 1;
	} else
	if (strlen(buf) > 0) {

	  {
	    /* figure out homedirectory */
	    char *newhomedir;
	    char *ptr;
	    char *lastslash;
	    nash_st *st = (nash_st*) (nash->st);
	    newhomedir = malloc(strlen(st->homedir)+strlen(buf));
	    strcpy(newhomedir, st->homedir);
	    ptr = newhomedir;
	    lastslash = ptr;
	    while (*ptr) {
		if (*ptr == '/') lastslash = ptr;
		ptr++;
	    }
	    lastslash++;
	    strcpy(lastslash, buf);
	    free(st->homedir);
	    st->homedir = newhomedir;
	    printf("New home directory will be %s\n", newhomedir);
	    Nash$ExecuteString(nash, "cd ~");
	    leave = 1;
	  }
	}
    }

#endif /* CONFIG_NASHLOGIN */

    Nash$ExecuteString(nash, "source profile.nash");

    while (1) {
	printf("%s ", Nash$GetPrompt(nash));
	nread = Rd$GetLine(Pvs(in), buf, 255);
	TRC(printf("\n"));
	if (nread) {
	    buf[nread-1] = 0;

	    TRC(printf("nread %d buf %s\n", nread, buf+1));

	    Nash$ExecuteString(nash, buf);
	}
	
    }
}

/***********************************************************************/
/* UDPNash */


static void UDP_Closure_Apply_m (
        Closure_cl      *self )
{
    Closure_clp termclosure;

    printf("Netterm Nash starting\n");
    termclosure = NAME_FIND("modules>netterm", Closure_clp);
    Closure$Apply(termclosure);


    PromptInterpreter(NULL);
}

static  Closure_op      udpms = { UDP_Closure_Apply_m };
const static  Closure_cl      udpcl = { &udpms, NULL };
CL_EXPORT (Closure, UDPnash, udpcl);

/**********************************************************************/
/* SerialNash */
#define SERIAL_READER "dev>serial0>rd"
#define SERIAL_WRITER "dev>serial0>wr"

static void Serial_Closure_Apply_m (
        Closure_cl      *self )
{
    printf("Serial Nash starting\n");
    TRY {
	Pvs(in)  = IDC_OPEN (SERIAL_READER, Rd_clp);
	Pvs(out) = IDC_OPEN (SERIAL_WRITER, Wr_clp);
	Pvs(err) = Pvs(out);
	TRC(eprintf("Opened serial line\n"));
    } CATCH_Binder$Error(problem) {
	TRC(eprintf("Caught exception when opening serial line\n"));
	if(problem == Binder_Problem_ServerRefused) {
	    printf("nash: serial reader is in use. exiting.\n");
	    return;
	} else RERAISE;
    } ENDTRY;

    PromptInterpreter(NULL);
}

static  Closure_op      Serialms = { Serial_Closure_Apply_m };
const static  Closure_cl      Serialcl = { &Serialms, NULL };
CL_EXPORT (Closure, Serialnash, Serialcl);


/**********************************************************************/
/* WSNash */

static void WS_Closure_Apply_m (
        Closure_cl      *self )
{
    Closure_clp termclosure;
    Type_Any any;
    bool_t gotws = False;

    printf("WS Nash starting\n");
    while (!gotws) {
	gotws = Context$Get(Pvs(root), "svc>WS", &any);
	if (!gotws) PAUSE(SECONDS(1));
    }

    termclosure = NAME_FIND("modules>WSterm", Closure_clp);
    Closure$Apply(termclosure);


    PromptInterpreter(NULL);
}

static  Closure_op      WSms = { WS_Closure_Apply_m };
const static  Closure_cl      WScl = { &WSms, NULL };
CL_EXPORT (Closure, WSnash, WScl);

/**********************************************************************/
/* TTYNash */

static void TTY_Closure_Apply_m (
        Closure_cl      *self )
{
    Closure_clp termclosure;

    termclosure = NAME_FIND("modules>ttyterm", Closure_clp);
    Closure$Apply(termclosure);

    printf("tty Nash starting\n");

    PromptInterpreter(NULL);
}

static  Closure_op      TTYms = { TTY_Closure_Apply_m };
const static  Closure_cl      TTYcl = { &TTYms, NULL };
CL_EXPORT (Closure, TTYnash, TTYcl);



/*
 * End 
 */
