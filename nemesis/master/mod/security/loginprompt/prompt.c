/*
 ****************************************************************************
 * (C) 1998                                                                 *
 * Rolf Neugebauer                                                          *
 * Department of Computing Science - University of Glasgow                  *
 ****************************************************************************
 *
 *        File: 
 *      Author: Rolf Neugebauer
 *     Changes: 
 *              
 *        Date: 1998
 * 
 * Environment: 
 * Description: 
 *
 ****************************************************************************
 * $Id: prompt.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 ****************************************************************************
 */


#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <links.h>
#include <mutex.h>
#include <time.h>
#include <ecs.h>
#include <IDCMacros.h>


#include <LoginPrompt.ih>

#include <Security.ih>
#include <Login.ih>

#include <CLine.ih>
#include <CLineCtl.ih>
#include <Rd.ih>
#include <Wr.ih>

#include <stdio.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define BUF_SIZE 256

/* --- LoginPrompt ------------------------------------------------------- */
static  LoginPrompt_Prompt_fn LoginPrompt_Prompt_m;
static  LoginPrompt_op     LoginPrompt_ms = {
  LoginPrompt_Prompt_m
};
static  const LoginPrompt_cl     cl = { &LoginPrompt_ms, NULL };
CL_EXPORT (LoginPrompt, LoginPrompt, cl);

/* --- Entry Points ------------------------------------------------------ */

static bool_t LoginPrompt_Prompt_m (LoginPrompt_cl *self,
				    Rd_clp          raw_in,
				    Wr_clp          raw_out)
{
  Login_clp       log;
  Security_clp    sec;
  Security_Tag    tag;
  string_t        cert;
  bool_t          ok;
  
  CLine_clp       cline;
  CLineCtl_clp    clinectl;
  Rd_clp          cooked_rd;
  Wr_clp          cooked_wr;

  string_t        buf;
  NOCLOBBER string_t        hostname;

  NOCLOBBER string_t        username = NULL;
  NOCLOBBER string_t        password = NULL;
  uint64_t        nread;

  /*
  ** Find our security stuff
  */
  TRY {
    log = IDC_OPEN ("sys>Login", Login_clp);
    sec = IDC_OPEN ("sys>SecurityOffer", Security_clp); 
  } CATCH_ALL {
    printf ("LoginPrompt_Prompt_m: Error finding security modules.\n");
    return False;
  } ENDTRY;

  /*
  ** cook our command line
  */
  TRY {
    cline = NAME_FIND("modules>CLine", CLine_clp);
    cooked_rd = CLine$New(cline, raw_in, raw_out, &cooked_wr, &clinectl);  
    buf = Heap$Malloc (Pvs(heap), BUF_SIZE * sizeof(char));
  } CATCH_ALL {
    printf ("LoginPrompt_Prompt_m: Error cooking commandline.\n");
    return False;
  } ENDTRY;

  /*
  ** get our hostname
  */
  TRY {
    Type_Any any;
    NOCLOBBER int i;
    Time_ns timeout; 
    
#define MAX_TRIES 8
    timeout = TENTHS(1);
    for(i = 0; i < MAX_TRIES; i++) {
      if(Context$Get(Pvs(root), "svc>net>eth0>myHostname", &any))
	break;
      PAUSE(timeout);
      timeout <<= 1;
    }
    
    if(i == MAX_TRIES) {
      hostname = strduph ("Hostname", Pvs(heap));
    } else {
      hostname = NARROW(&any, string_t);
    }
  } CATCH_ALL {
    printf ("LoginPrompt_Prompt_m: Error getting hostname.\n");
    return False;
  } ENDTRY; 


  /*
  ** Prompt
  */
  TRY {
    /* Username */
    sprintf (buf, "\nWelcome to %s's Nemesis\n\n%s login: ",
	     NAME_FIND("conf>userid", string_t), hostname);
    Wr$PutStr (cooked_wr, buf);
    
    nread = Rd$GetLine(cooked_rd, buf, BUF_SIZE);
    if (nread) {
      buf[nread-1] = '\0';
      username = strduph(buf, Pvs(heap));
    } else {
      username = strduph("", Pvs(heap));
    }
    /* Password */
    Wr$PutStr (cooked_wr, "Password: ");
    CLineCtl$Echo (clinectl, False);
    nread = Rd$GetLine(cooked_rd, buf, BUF_SIZE);
    if (nread) {
      buf[nread-1] = '\0';
      password = strduph(buf, Pvs(heap));
    } else {
      password = strduph("", Pvs(heap));
    }
  }CATCH_ALL {
    printf ("LoginPrompt_Prompt_m: Error while prompting.\n");
    Heap$Free (Pvs(heap), buf);
    if (username) Heap$Free (Pvs(heap), username);
    if (password) Heap$Free (Pvs(heap), password);
    return False;
  } ENDTRY; 

  /* Authenticate */
  TRY {
    ok = Security$CreateTag (sec, &tag);
    if (!ok) {
      printf ("LoginPrompt_Prompt_m: Can't created tag.\n");
      Heap$Free (Pvs(heap), buf);
      Heap$Free (Pvs(heap), password);
      Heap$Free (Pvs(heap), username);
      return False;
    }
    
    ok = Login$Login (log, tag, username, password, &cert);
    if (!ok) {
      printf ("LoginPrompt_Prompt_m: Authetication failed.\n");
      Heap$Free (Pvs(heap), buf);
      Heap$Free (Pvs(heap), password);
      Heap$Free (Pvs(heap), username);
      return False;
    }
  }CATCH_ALL {
    printf ("LoginPrompt_Prompt_m: Error while authenticate.\n");
    Heap$Free (Pvs(heap), buf);
    Heap$Free (Pvs(heap), password);
    Heap$Free (Pvs(heap), username);
    return False;
  } ENDTRY; 

  printf ("well done\n");

  return True;
}

