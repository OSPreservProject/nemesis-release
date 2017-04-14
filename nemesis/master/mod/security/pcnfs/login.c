/*
 ****************************************************************************
 * (C) 1998                                                                 *
 * Rolf Neugebauer                                                          *
 * Department of Computing Science - University of Glasgow                  *
 ****************************************************************************
 *
 *        File: login.c
 *      Author: Rolf Neugebauer
 *     Changes: 
 *              
 *        Date: 1998
 * 
 * Environment: Security/Login
 * Description: Provides a pcnfs based login service
 *
 ****************************************************************************
 * $Id: login.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 ****************************************************************************
 */


#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <links.h>
#include <mutex.h>
#include <IDCMacros.h>

#include <PcnfsLoginMod.ih>
#include <Security.ih>
#include <Login.ih>

#include <StringTblMod.ih>
#include <StringTbl.ih>
#include <IDCCallback.ih>
#include <IDCTransport.ih>

#include <stdio.h>
#include <socket.h>
#include <netorder.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>

#include "common.h"
#include "pcnfsd.h"

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED\n", __FUNCTION__);

static void   scramble (char *s1, char *s2); /* primitive pcnfs encrypt */
static bool_t inet_gethostaddr (const char *host, struct in_addr *sin_addr);

/* --- PcnfsLoginMod -------------------------------------------------- */
static  PcnfsLoginMod_New_fn PcnfsLoginMod_New_m;
static  PcnfsLoginMod_op     PcnfsLoginMod_ms = {
  PcnfsLoginMod_New_m
};
static  const PcnfsLoginMod_cl     cl = { &PcnfsLoginMod_ms, NULL };
CL_EXPORT (PcnfsLoginMod, PcnfsLoginMod, cl);

/* --- Login ---------------------------------------------------------- */
static  Login_Login_fn          Login_Login_m;
static  Login_Logout_fn         Login_Logout_m;
static  Login_Validate_fn       Login_Validate_m;
static  Login_GetProperty_fn    Login_GetProperty_m;
static  Login_SetProperty_fn    Login_SetProperty_m;

static  Login_op        login_ms = {
    Login_Login_m,
    Login_Logout_m,
    Login_Validate_m,
    Login_GetProperty_m,
    Login_SetProperty_m
};

/* --- IDCCallback ---------------------------------------------------- */
static  IDCCallback_Request_fn  IDCCallback_Request_m;
static  IDCCallback_Bound_fn    IDCCallback_Bound_m;
static  IDCCallback_Closed_fn   IDCCallback_Closed_m;

static  IDCCallback_op  callback_ms = {
    IDCCallback_Request_m,
    IDCCallback_Bound_m,
    IDCCallback_Closed_m
};



/*----------------------------------------------------------- State ----*/

typedef struct _PcnfsLoginMod_st PcnfsLoginMod_st;
typedef struct _Login_st Login_st;
typedef struct _Certificate Certificate;
typedef struct _User User;


struct _User {
  string_t username;
  string_t password;   /* We don't ned to store this, do we? */
  uint32_t unix_uid;
  uint32_t unix_gid;
  string_t name;       /* We don't get this from pcnfs */
  string_t homedir;    /* We might get this from pcnfs */
  string_t shell;      /* We don't get this from pcnfs */
  /* gids */
};

struct _Certificate {
  struct _Certificate *next, *prev;
  Security_Tag tag;   /* What tag is it bound to? */
  User *u;            /* Who is it? */
  string_t cert;      /* Certificate string returned to clients */
};

struct _PcnfsLoginMod_st {
  CLIENT          *client;           /* PcnfsLoginModD client */
  char            *hostname;
  IDCCallback_cl   callback;
  Heap_clp         heap;
  Security_clp     sec;              /* Our security closure */
  uint32_t         certificate_id;   /* Ensure we issue unique certificates */
  Certificate      certificate_list; /* List of all valid certificates */
  StringTbl_cl    *certificate_tbl;  /* Lookup table for valid certificates */
  SRCThread_Mutex  mu;               /* Protect access to data */
};

struct _Login_st {
  Login_cl   cl;
  Domain_ID  id;      /* Who are they? */
  PcnfsLoginMod_st  *pst;
};

/*----------------------------------------------- PcnfsLoginMod Entry Points ----*/

#define TIMEOUT_MS 1000
static const struct timeval TIMEOUT = { 
    (TIMEOUT_MS / 1000), 
    ((TIMEOUT_MS % 1000) * 1000) 
};

static IDCOffer_clp PcnfsLoginMod_New_m (PcnfsLoginMod_cl *self,
					 string_t          server)
{
  char               *hostname;  /* Our hostname */
  /* RPC stuff */
  CLIENT             *client;  
  struct sockaddr_in  servaddr;
  int                 sock;
  struct pmap         parms;
  uint16_t            pcnfsport;
  int32_t             res;
  /* Login stuff */
  PcnfsLoginMod_st   *pst;
  Login_st           *st;
  IDCTransport_clp    shmt;
  StringTblMod_clp    strtbl;
  Type_Any            any;
  IDCOffer_clp        offer;
  IDCService_clp      service;
  


  TRC(printf("PcnfsLoginMod$New entered: server=%s\n", server));

  TRY {
    shmt     = NAME_FIND("modules>ShmTransport", IDCTransport_clp);
    strtbl   = NAME_FIND("modules>StringTblMod", StringTblMod_clp);
    hostname = NAME_FIND ("svc>net>eth0>myHostname", string_t);
  } CATCH_ALL {
    printf ("pcnfs: couldn't find necessary modules.\n");
    return NULL;
  } ENDTRY;

  /*
  ** Contact Portmapper
  */
  sock = RPC_ANYSOCK;
  servaddr.sin_family = AF_INET;
  servaddr.sin_port   = hton16(PMAPPORT);
  if (!inet_gethostaddr(server, &(servaddr.sin_addr))) 
    return NULL;

  TRC(printf(" + Contact Portmapper on host %I\n", servaddr.sin_addr.s_addr));
    client = clntudp_create(&servaddr, PMAPPROG, PMAPVERS, TIMEOUT, &sock);
  if(client == NULL) {
    printf ("pcnfs: failed to contact portmapper\n");
    return NULL;
  }
  clnt_control(client, CLSET_FD_NCLOSE, NULL); 
  
  parms.pm_prog = PCNFSDPROG;
  parms.pm_vers = PCNFSDV2;
  parms.pm_prot = IPPROTO_UDP;
  parms.pm_port = 0;  /* not needed or used */
  
  if ((res =  CLNT_CALL(client, PMAPPROC_GETPORT,
		       xdr_pmap, &parms,
		       xdr_u_short, &pcnfsport,
		       TIMEOUT)) != RPC_SUCCESS){
    printf("pcnfs: portmapper failure\n");
    CLNT_DESTROY(client);
    close(sock);
    return NULL;
  }
  if (pcnfsport == 0) {
    printf("pcnfs: portmapper didn't return pcnfsd port\n");
    CLNT_DESTROY(client);
    close(sock);
    return NULL;
  }
  TRC(printf(" + PCNFS port is %04x\n", pcnfsport));
  /* SUNRPC didn't close the socket so we can use it again */
  servaddr.sin_port   = hton16(pcnfsport);
  client = clntudp_create(&servaddr, PCNFSDPROG, PCNFSDV2, TIMEOUT, &sock);
  if(client == NULL) {
    printf("pcnfs: pcnfsd failure\n");
    CLNT_DESTROY(client);
    close(sock);
    return NULL;
  }
  /*
  ** Now we have everything we need to generate pcnfs calls
  ** We can now set up the remaining infrastructure for the login service 
  */
  TRC(printf(" + init state\n"));  
  pst=Heap$Malloc(Pvs(heap), sizeof(*pst));
  pst->heap=Pvs(heap);

  pst->client          = client;
  pst->hostname        = strduph (hostname, pst->heap);
  pst->certificate_id  = 1;
  pst->certificate_tbl = StringTblMod$New(strtbl, pst->heap);

  LINK_INITIALISE(&pst->certificate_list);
  MU_INIT(&pst->mu);

  TRC(printf(" + binding to security service\n"));
  pst->sec             = IDC_OPEN("sys>SecurityOffer", Security_clp); 

  /* Closure for local access to the server */
  st=Heap$Malloc(pst->heap, sizeof(*st));
  CL_INIT(st->cl, &login_ms, st);
  st->id=VP$DomainID(Pvs(vp));
  st->pst=pst;  

  TRC(printf(" + exporting Login service\n"));
  ANY_INIT(&any, Login_clp, &st->cl);
  CL_INIT(pst->callback, &callback_ms, pst);
  offer=IDCTransport$Offer(shmt, &any, &pst->callback, pst->heap,
			   Pvs(gkpr), Pvs(entry), &service);
  return offer;  
}

/*----------------------------------------------- Login Entry Points ----*/

static bool_t Login_Login_m (Login_cl     *self,
			     Security_Tag  tag         /* IN */,
			     string_t      username    /* IN */,
			     string_t      password    /* IN */,
			     string_t     *certificate /* RETURNS */)
{
  Login_st      *st = self->st;
  PcnfsLoginMod_st   *pst = st->pst;
  /* pcnfs stuff */
  v2_auth_args        a;
  v2_auth_results     rp;
  char                uname[32];
  char                pw[64];
  int                 res;

  Certificate *c;

  TRC(printf("Pcmfs: Login$Login called for username: %s.\n", username));

  /* Check that caller owns the tag */
  TRC(printf(" + check tag\n"));
  if (!Security$CheckTag(pst->sec, tag, st->id)) {
    printf("Login: domain %qx doesn't have tag %qx\n",st->id,tag);
    *certificate=strduph("invalid", pst->heap);
    return False; /* Bad tag */
  } 

  /* Lookup username and password */ 
  scramble(username, uname);
  scramble(password, pw);
  a.system = pst->hostname;
  a.id = uname;
  a.pw = pw;
  a.cm = "-";
  TRC(printf(" + invoking pr_auth_2\n"));
  /* zero the result structure */
  (void)memset((char *)&rp, 0, sizeof(rp));
  if ((res = CLNT_CALL(pst->client, PCNFSD2_AUTH,
		      xdr_v2_auth_args,	(caddr_t)&a,
		      xdr_v2_auth_results, (caddr_t)&rp,
		      TIMEOUT)) != RPC_SUCCESS) {
    printf ("pcnfs: client call for PCNFSD2_AUTH result=%d.\n", res);
    *certificate=strduph("invalid", pst->heap);
    return False;
  }
  if(rp.stat == AUTH_RES_FAIL) {
    printf("pcnfs: authentification failure\n");
    *certificate=strduph("invalid", pst->heap);
    return False;
  }
  
  TRC(printf (" + uid = %u, gid = %u\n", rp.uid, rp.gid));
  TRC(printf (" + home = '%s', cm = '%s'\n", rp.home, rp.cm));
  TRC(printf (" + gids_len = %d", rp.gids.gids_len));  


  TRC(printf (" + generate a new certificate\n"));  
  c=Heap$Malloc(pst->heap, sizeof(Certificate));
  if (!c) {
    printf("Login: out of memory\n");
    return False; /* Out of memory */
  }
  c->u=Heap$Malloc(pst->heap, sizeof(User));
  if (!c->u) {
    printf("Login: out of memory\n");
    return False; /* Out of memory */
  }
  TRC(printf (" + fill in pcnfs results\n"));  
  c->tag=tag;
  c->u->username = strduph(username, pst->heap);
  c->u->password = strduph("", pst->heap);       /* XXX don't store it */
  c->u->unix_uid = rp.uid;
  c->u->unix_gid = rp.gid;
  c->u->name     = strduph(username, pst->heap); /* XXX don't get this info */
  c->u->homedir  = strduph(rp.home, pst->heap);
  c->u->shell    = strduph("", pst->heap);       /* XXX don't get this info */

  c->cert=Heap$Malloc(pst->heap, 18+strlen(username));
  sprintf(c->cert, "%s:%x", username, pst->certificate_id++);

  MU_LOCK(&pst->mu);
  LINK_ADD_TO_HEAD(&pst->certificate_list, c);
  StringTbl$Put(pst->certificate_tbl, c->cert, c);
  MU_RELEASE(&pst->mu);

  *certificate=strduph(c->cert, pst->heap);
  
  TRC(printf(" + user %s logged in; issued certificate %s\n",
	     c->u->username,c->cert));
  TRC(printf(" + bound to tag %qx\n", tag));

  /* free up strings allocated by RPC */
  if(rp.cm)
    free(rp.cm);
  if(rp.gids.gids_val)
    free(rp.gids.gids_val);
  if(rp.home)
    free(rp.home);

  return True; /* User logged on */
}

static bool_t Login_Logout_m (Login_cl *self,
			      string_t  certificate /* IN */ )
{
  Login_st      *st = self->st;
  PcnfsLoginMod_st      *pst = st->pst;

  Certificate   *c;

  MU_LOCK(&pst->mu);

  /* Find the certificate */
  if (!StringTbl$Get(pst->certificate_tbl, certificate, (addr_t)&c)) {
    MU_RELEASE(&pst->mu);
    TRC(printf("Pcnfs: Logout: unknown certificate %s\n",certificate));
    return False; /* Not a valid certificate */
  }

  /* Check that the caller is allowed to use the certificate */
  if (!Security$CheckTag(pst->sec, c->tag, st->id)) {
    MU_RELEASE(&pst->mu);
    TRC(printf("Pcnfs: Logout: domain %qx attempted to delete certificate "
	       "%s (%s),\n"
	       "               but does not possess tag %qx\n",st->id,
	       certificate, c->u->username, c->tag));
    return False; /* Invalid logout attempt */
  }
  
  /* Go ahead and do it */
  TRC(printf("Logout: domain %qx deleted certificate %s (%s)\n",
	     st->id, certificate, c->u->username));

  StringTbl$Delete(pst->certificate_tbl, certificate, (addr_t)&c);
  LINK_REMOVE(c);
  FREE(c->cert); 
  FREE(c);
  
  MU_RELEASE(&pst->mu);

  return True; /* User logged off */
}

static bool_t Login_Validate_m (Login_cl  *self,
				string_t   certificate,
				Domain_ID  domain )
{
  Login_st         *st = self->st;
  PcnfsLoginMod_st *pst = st->pst;

  Certificate   *c;

  MU_LOCK(&pst->mu);

  /* Find the certificate */
  if (!StringTbl$Get(pst->certificate_tbl, certificate, (addr_t)&c)) {
    MU_RELEASE(&pst->mu);
    TRC(printf("Login_Validate: unknown certificate %s\n",certificate));
    return False; /* Not a valid certificate */
  }

  /* Check the certificate */
  if (!Security$CheckTag(pst->sec, c->tag, domain)) {
    MU_RELEASE(&pst->mu);
    TRC(printf("Login_Validate: certificate %s (%s) may not be used by"
	       "domain %qx\n",certificate, c->u->username, domain));
    return False; /* Domain does not own certificate */
  }
  
  MU_RELEASE(&pst->mu);

  return True; /* Certificate is valid for domain to use */ 
}

static bool_t Login_GetProperty_m (Login_cl *self,
				   string_t  certificate /* IN */,
				   string_t  property    /* IN */
				   /* RETURNS */,
				   string_t *val )
{
  Login_st         *st = self->st;
  PcnfsLoginMod_st *pst = st->pst;
  char              buff[20]={'i','n','v','a','l','i','d',0};
  bool_t            rval = True;
  Certificate      *c;
  
  MU_LOCK(&pst->mu);
  
  if (!StringTbl$Get(pst->certificate_tbl, certificate, (addr_t)&c)) {
    MU_RELEASE(&pst->mu);
    TRC(printf("Login: domain %qx tried to fetch property %s of "
	       "nonexistant certificate %s\n",
	       st->id,property,certificate));
    *val=strduph("invalid",pst->heap);
    return False;
  }
  
  /* We have the certificate. For now we only recognise some specific
     properties. */
  if (strcmp(property, "username")==0) {
    *val=strduph(c->u->username,pst->heap);
  } else if (strcmp(property, "name")==0) {
    *val=strduph(c->u->name,pst->heap);
  } else if (strcmp(property, "homedir")==0) {
    *val=strduph(c->u->homedir,pst->heap);
#if 0
  } else if (strcmp(property, "shell")==0) {
    *val=strduph(c->u->shell,pst->heap);
#endif
  } else if (strcmp(property, "unix_uid")==0) {
    sprintf(buff,"%d",c->u->unix_uid);
    *val=strduph(buff,pst->heap);
  } else if (strcmp(property, "unix_gid")==0) {
    sprintf(buff,"%d",c->u->unix_gid);
    *val=strduph(buff,pst->heap);
  } else {
    rval=False;
    *val=strduph("invalid",pst->heap);
  }
  
  MU_RELEASE(&pst->mu);
    
  return rval;      
}

static bool_t Login_SetProperty_m (Login_cl *self,
				   string_t  certificate /* IN */,
				   string_t  property    /* IN */,
				   string_t  value       /* IN */ )
{
  return False; /* XXX not implemented */
}


/*------------------------------------------------- IDC Entry Points ----*/

static bool_t IDCCallback_Request_m (IDCCallback_cl       *self,
				     IDCOffer_clp          offer   /* IN */,
				     Domain_ID             dom     /* IN */,
				     ProtectionDomain_ID   pdom    /* IN */,
				     const Binder_Cookies *clt_cks /* IN */,
				     Binder_Cookies       *svr_cks /* OUT */ )
{
    return True;
}

static bool_t IDCCallback_Bound_m (IDCCallback_cl       *self,
				   IDCOffer_clp          offer   /* IN */,
				   IDCServerBinding_clp  binding /* IN */,
				   Domain_ID             dom     /* IN */,
				   ProtectionDomain_ID   pdom    /* IN */,
				   const Binder_Cookies *clt_cks /* IN */,
				   Type_Any             *server  /* INOUT */)
{
  PcnfsLoginMod_st *pst = self->st;
  Login_st         *st;

  st=Heap$Malloc(pst->heap, sizeof(*st));
  CL_INIT(st->cl, &login_ms, st);
  st->id      = dom;
  st->pst     = pst;
  server->val = &st->cl;
  return True; 
}

static void IDCCallback_Closed_m (IDCCallback_cl       *self,
				  IDCOffer_clp          offer   /* IN */,
				  IDCServerBinding_clp  binding /* IN */,
				  const Type_Any       *server  /* IN */ )
{
  Login_clp clp;

  clp = server->val;
  FREE(clp->st); 
}

/* -------------------------------------------------------------- helper --- */

#define zchar           0x5b
void
scramble(s1, s2)
char           *s1;
char           *s2;
{
        while (*s1) 
              {
              *s2++ = (*s1 ^ zchar) & 0x7f;
              s1++;
              }
        *s2 = 0;
}

/* This is horrible. Use DNS code instead. */
static bool_t inet_gethostaddr (const char *host, struct in_addr *sin_addr)
    /* RAISES Context_NotFound, TypeSystem_Incompatible, Net_BadAF */
{
    unsigned int a,b,c,d;

    if (sscanf(host, "%d.%d.%d.%d", &a,&b,&c,&d) != 4)
    {
	printf("Pcnfs: inet_gethostaddr: bad hostname format `%s'\n", host);
	return False;
    }

    sin_addr->s_addr = a | b<<8 | c<<16 | d<<24;
    return True;
}
