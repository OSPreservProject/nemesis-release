/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      login.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Login system service
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: login.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
/* XXX! */
#undef EOF
#include <contexts.h>
#include <links.h>
#include <mutex.h>
#include <IDCMacros.h>

#include <Security.ih>
#include <Login.ih>
#include <UnixLoginMod.ih>
#include <FSClient.ih>
#include <FSDir.ih>
#include <FileIO.ih>
#include <FSUtil.ih>
#include <StringTblMod.ih>
#include <StringTbl.ih>
#include <IDCCallback.ih>
#include <IDCTransport.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);


static  UnixLoginMod_New_fn         UnixLoginMod_New_m;

static  UnixLoginMod_op     mod_ms = {
    UnixLoginMod_New_m
};

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

static  IDCCallback_Request_fn  IDCCallback_Request_m;
static  IDCCallback_Bound_fn    IDCCallback_Bound_m;
static  IDCCallback_Closed_fn   IDCCallback_Closed_m;

static  IDCCallback_op  callback_ms = {
    IDCCallback_Request_m,
    IDCCallback_Bound_m,
    IDCCallback_Closed_m
};

static  const UnixLoginMod_cl     cl = { &mod_ms, NULL };

CL_EXPORT (UnixLoginMod, UnixLoginMod, cl);


/*----------------------------------------------------------- State ----*/

typedef struct _Login_st Login_st;
typedef struct _UnixLoginMod_st UnixLoginMod_st;
typedef struct _Certificate Certificate;
typedef struct _User User;
typedef struct _Group Group;

struct _Certificate {
    struct _Certificate *next, *prev;
    Security_Tag tag;   /* What tag is it bound to? */
    User *u;            /* Who is it? */
    string_t cert;      /* Certificate string returned to clients */
};

struct _UnixLoginMod_st {
    IDCCallback_cl callback;
    Heap_clp heap;

    Security_clp sec;     /* Our security closure */

    StringTbl_clp users;  /* We read in the password file */
    StringTbl_clp groups; /* And the group file */

    StringTbl_clp certificate_tbl; /* Lookup table for valid certificates */

    Certificate certificate_list; /* List of all valid certificates */
    uint32_t certificate_id; /* Ensure we issue unique certificates */

    SRCThread_Mutex mu; /* Protect access to data */
};

struct _Login_st {
    Login_cl cl;
    Domain_ID id;      /* Who are they? */
    UnixLoginMod_st *lst;
};

struct _User {
    string_t username;
    string_t password; /* Encrypted */
    uint32_t unix_uid;
    uint32_t unix_gid;
    string_t name;
    string_t homedir;
    string_t shell;
};

struct _Group {
    string_t groupname;
    string_t password;
    uint32_t unix_gid;
    /* Plus some group members... */
};

static bool_t add_passwd_line(UnixLoginMod_st *lst, char *line)
{
    char *username;
    char *password;
    char *uid;
    char *gid;
    char *name;
    char *homedir;
    char *shell;
    User *u;

    username=line;
    line=strchr(line,':');
    if (!line) return False;
    *line++=0;

    password=line;
    line=strchr(line,':');
    if (!line) return False;
    *line++=0;

    uid=line;
    line=strchr(line,':');
    if (!line) return False;
    *line++=0;

    gid=line;
    line=strchr(line,':');
    if (!line) return False;
    *line++=0;

    name=line;
    line=strchr(line,':');
    if (!line) return False;
    *line++=0;

    homedir=line;
    line=strchr(line,':');
    if (!line) return False;
    *line++=0;

    shell=line;
    line=strchr(line,'\n');
    if (!line) return False;
    *line++=0;

    /* Ok, the line is valid; put it in the table */
    u=Heap$Malloc(lst->heap, sizeof(*u));
    u->username=strduph(username, lst->heap);
    u->password=strduph(password, lst->heap);
    u->unix_uid=atoi(uid);
    u->unix_gid=atoi(gid);
    u->name=strduph(name, lst->heap);
    u->homedir=strduph(homedir, lst->heap);
    u->shell=strduph(shell, lst->heap);

    StringTbl$Put(lst->users, username, u);

    return True;
}

static bool_t add_group_line(UnixLoginMod_st *lst, char *line)
{
    return False;
}

/*---------------------------------------------------- Entry Points ----*/

static IDCOffer_clp UnixLoginMod_New_m (
    UnixLoginMod_cl     *self,
    IDCOffer_clp    fs      /* IN */,
    string_t        passwd  /* IN */,
    string_t        group   /* IN */ )
{
    UnixLoginMod_st     *lst;
    Login_st        *st;
    FSClient_clp     fsclient;
    Type_Any         any, *dirany;
    FSTypes_RC       rc;
    FSDir_clp        dir;
    FileIO_clp       fileio;
    Rd_clp           rd;
    FSUtil_clp       fsutil;
    StringTblMod_clp strtbl;
    IDCTransport_clp shmt;
    IDCOffer_clp     offer;
    IDCService_clp   service;
    char buff[124];

    TRC(printf("UnixLoginMod_New_m(%p, %s, %s)\n",fs,passwd,group));

    fsutil=NAME_FIND("modules>FSUtil", FSUtil_clp);
    strtbl=NAME_FIND("modules>StringTblMod", StringTblMod_clp);
    shmt=NAME_FIND("modules>ShmTransport", IDCTransport_clp);

    lst=Heap$Malloc(Pvs(heap), sizeof(*lst));
    lst->heap=Pvs(heap);

    /* Bind to security service */
    TRC(printf(" + binding to security service\n"));
    lst->sec=IDC_OPEN("sys>SecurityOffer", Security_clp);

    /* Bind to FS */
    TRC(printf(" + binding to filesystem\n"));
    fsclient=IDC_BIND(fs, FSClient_clp);
    
    rc=FSClient$GetDir(fsclient, "", True, &dirany);
    if (rc!=FSTypes_RC_OK) {
	printf("UnixLoginMod: couldn't get directory\n");
	FREE(lst);
	return NULL;
    }

    dir=NARROW(dirany, FSDir_clp);
    FREE(dirany);

    /* Create tables */
    TRC(printf(" + creating tables\n"));
    lst->users=StringTblMod$New(strtbl, lst->heap);
    lst->groups=StringTblMod$New(strtbl, lst->heap);
    lst->certificate_tbl=StringTblMod$New(strtbl, lst->heap);
    lst->certificate_id = 1;

    /* Read in passwd file */
    TRC(printf(" + reading password file\n"));
    rc=FSDir$Lookup(dir, passwd, True);
    if (rc!=FSTypes_RC_OK) {
	printf("UnixLoginMod: couldn't lookup passwd file\n");
	FREE(lst);
	return NULL;
    }

    rc=FSDir$Open(dir, 0, FSTypes_Mode_Read, 0, &fileio);
    if (rc!=FSTypes_RC_OK) {
	printf("UnixLoginMod: couldn't open passwd file\n");
	FREE(lst);
	return NULL;
    }

    rd=FSUtil$GetRd(fsutil, fileio, lst->heap, True);
    
    while (!Rd$EOF(rd))
    {
	uint32_t l;
	l=Rd$GetLine(rd, buff, 120);
	buff[l]=0;
	/* TRC(printf("->%s\n",buff)); */
	add_passwd_line(lst, buff);
    }

    Rd$Close(rd);

    /* Read in group file */
    TRC(printf(" + reading group file\n"));
    rc=FSDir$Lookup(dir, group, True);
    if (rc!=FSTypes_RC_OK) {
	printf("UnixLoginMod: couldn't lookup group file\n");
	FREE(lst);
	return NULL;
    }

    rc=FSDir$Open(dir, 0, FSTypes_Mode_Read, 0, &fileio);
    if (rc!=FSTypes_RC_OK) {
	printf("UnixLoginMod: couldn't open group file\n");
	FREE(lst);
	return NULL;
    }

    rd=FSUtil$GetRd(fsutil, fileio, lst->heap, True);

    while (!Rd$EOF(rd))
    {
	uint32_t l;
	l=Rd$GetLine(rd, buff, 120);
	buff[l]=0;
	/* TRC(printf("=>%s\n",buff)); */
	add_group_line(lst, buff);
    }

    Rd$Close(rd);

    /* We don't need the filesystem any more */
    FSDir$Close(dir);

    LINK_INITIALISE(&lst->certificate_list);

    MU_INIT(&lst->mu);

    /* Closure for local access to the server */

    st=Heap$Malloc(lst->heap, sizeof(*st));
    CL_INIT(st->cl, &login_ms, st);
    st->id=VP$DomainID(Pvs(vp));
    st->lst=lst;

    /* Export */
    TRC(printf(" + exporting Login service\n"));
    ANY_INIT(&any, Login_clp, &st->cl);
    CL_INIT(lst->callback, &callback_ms, lst);
    offer=IDCTransport$Offer(shmt, &any, &lst->callback, lst->heap,
			     Pvs(gkpr), Pvs(entry), &service);

    return offer;
}

/*---------------------------------------------------- Entry Points ----*/

static bool_t Login_Login_m (
    Login_cl        *self,
    Security_Tag   tag    /* IN */,
    string_t        username        /* IN */,
    string_t        password        /* IN */
    /* RETURNS */,
    string_t        *certificate )
{
    Login_st      *st = self->st;
    UnixLoginMod_st   *lst = st->lst;

    User *u;
    Certificate *c;

    /* Check that caller owns the tag */
    if (!Security$CheckTag(lst->sec, tag, st->id)) {
	TRC(printf("Login: domain %qx doesn't have tag %qx\n",st->id,tag));
	*certificate=strduph("invalid", lst->heap);
	return False; /* Bad tag */
    }

    /* Lookup username */
    if (!StringTbl$Get(lst->users, username, (void *)&u)) {
	*certificate=strduph("invalid", lst->heap);
	TRC(printf("Login: domain %qx tried to login with unknown "
		   "username %s\n",st->id,username));
	return False; /* Bad username */
    }

    /* XXX Check password; skip for now */

    /* Proceed: generate a new certificate */
    c=Heap$Malloc(lst->heap, sizeof(*c));
    if (!c) {
	TRC(printf("Login: out of memory\n"));
	return False; /* Out of memory */
    }

    MU_LOCK(&lst->mu);
    c->tag=tag;
    c->u=u;
    c->cert=Heap$Malloc(lst->heap, 18+strlen(username));
    sprintf(c->cert, "%s:%x", username, lst->certificate_id++);

    LINK_ADD_TO_HEAD(&lst->certificate_list, c);
    StringTbl$Put(lst->certificate_tbl, c->cert, c);

    MU_RELEASE(&lst->mu);

    *certificate=strduph(c->cert, lst->heap);

    TRC(printf("Login: user %s logged in; issued certificate %s\n"
	       "       bound to tag %qx\n",c->u->username,c->cert,tag));

    return True; /* User logged on */
}

static bool_t Login_Logout_m (
    Login_cl        *self,
    string_t        certificate     /* IN */ )
{
    Login_st      *st = self->st;
    UnixLoginMod_st   *lst = st->lst;

    Certificate *c;

    MU_LOCK(&lst->mu);

    /* Find the certificate */
    if (!StringTbl$Get(lst->certificate_tbl, certificate, (void *)&c)) {
	MU_RELEASE(&lst->mu);
	TRC(printf("Logout: unknown certificate %s\n",certificate));
	return False; /* Not a valid certificate */
    }

    /* Check that the caller is allowed to use the certificate */
    if (!Security$CheckTag(lst->sec, c->tag, st->id)) {
	MU_RELEASE(&lst->mu);
	TRC(printf("Logout: domain %qx attempted to delete certificate "
		   "%s (%s),\n"
		   "        but does not possess tag %qx\n",st->id,
		   certificate, c->u->username, c->tag));
	return False; /* Invalid logout attempt */
    }

    /* Go ahead and do it */
    TRC(printf("Logout: domain %qx deleted certificate %s (%s)\n",
	       st->id, certificate, c->u->username));

    StringTbl$Delete(lst->certificate_tbl, certificate, (void *)&c);
    LINK_REMOVE(c);
    FREE(c->cert);
    FREE(c);

    MU_RELEASE(&lst->mu);

    return True; /* User logged off */
}

static bool_t Login_Validate_m (
    Login_cl        *self,
    string_t        certificate,
    Domain_ID       domain )
{
    Login_st      *st = self->st;
    UnixLoginMod_st   *lst = st->lst;

    Certificate *c;

    MU_LOCK(&lst->mu);

    /* Find the certificate */
    if (!StringTbl$Get(lst->certificate_tbl, certificate, (void *)&c)) {
	MU_RELEASE(&lst->mu);
	TRC(printf("Login_Validate: unknown certificate %s\n",certificate));
	return False; /* Not a valid certificate */
    }

    /* Check the certificate */
    if (!Security$CheckTag(lst->sec, c->tag, domain)) {
	MU_RELEASE(&lst->mu);
	TRC(printf("Login_Validate: certificate %s (%s) may not be used by"
		   "domain %qx\n",certificate, c->u->username, domain));
	return False; /* Domain does not own certificate */
    }

    MU_RELEASE(&lst->mu);

    return True; /* Certificate is valid for domain to use */
}

static bool_t Login_GetProperty_m (
    Login_cl        *self,
    string_t        certificate     /* IN */,
    string_t        property        /* IN */
    /* RETURNS */,
    string_t        *val )
{
    Login_st      *st = self->st;
    UnixLoginMod_st   *lst = st->lst;
    char buff[20]={'i','n','v','a','l','i','d',0};
    bool_t rval = True;
    Certificate *c;

    MU_LOCK(&lst->mu);

    if (!StringTbl$Get(lst->certificate_tbl, certificate, (void *)&c)) {
	MU_RELEASE(&lst->mu);
	TRC(printf("Login: domain %qx tried to fetch property %s of "
		   "nonexistant certificate %s\n",
		   st->id,property,certificate));
	*val=strduph("invalid",lst->heap);
	return False;
    }

    /* We have the certificate. For now we only recognise some specific
       properties. */
    if (strcmp(property, "username")==0) {
	*val=strduph(c->u->username,lst->heap);
    } else if (strcmp(property, "name")==0) {
	*val=strduph(c->u->name,lst->heap);
    } else if (strcmp(property, "homedir")==0) {
	*val=strduph(c->u->homedir,lst->heap);
    } else if (strcmp(property, "shell")==0) {
	*val=strduph(c->u->shell,lst->heap);
    } else if (strcmp(property, "unix_uid")==0) {
	sprintf(buff,"%d",c->u->unix_uid);
	*val=strduph(buff,lst->heap);
    } else if (strcmp(property, "unix_gid")==0) {
	sprintf(buff,"%d",c->u->unix_gid);
	*val=strduph(buff,lst->heap);
    } else {
	rval=False;
	*val=strduph("invalid",lst->heap);
    }

    MU_RELEASE(&lst->mu);

    return rval;
}

static bool_t Login_SetProperty_m (
    Login_cl        *self,
    string_t        certificate     /* IN */,
    string_t        property        /* IN */,
    string_t        value   /* IN */ )
{
    return False; /* XXX not implemented */
}

/*---------------------------------------------------- Entry Points ----*/

static bool_t IDCCallback_Request_m (
    IDCCallback_cl  *self,
    IDCOffer_clp    offer   /* IN */,
    Domain_ID       dom     /* IN */,
    ProtectionDomain_ID    pdom    /* IN */,
    const Binder_Cookies    *clt_cks        /* IN */,
    Binder_Cookies  *svr_cks        /* OUT */ )
{
    return True;
}

static bool_t IDCCallback_Bound_m (
    IDCCallback_cl  *self,
    IDCOffer_clp    offer   /* IN */,
    IDCServerBinding_clp    binding /* IN */,
    Domain_ID       dom     /* IN */,
    ProtectionDomain_ID    pdom    /* IN */,
    const Binder_Cookies    *clt_cks        /* IN */,
    Type_Any        *server /* INOUT */ )
{
    UnixLoginMod_st *lst = self->st;
    Login_st *st;

    st=Heap$Malloc(lst->heap, sizeof(*st));

    CL_INIT(st->cl, &login_ms, st);
    st->id = dom;
    st->lst = lst;

    server->val=(uint64_t)(word_t)&st->cl;

    return True;
}

static void IDCCallback_Closed_m (
    IDCCallback_cl  *self,
    IDCOffer_clp    offer   /* IN */,
    IDCServerBinding_clp    binding /* IN */,
    const Type_Any  *server /* IN */ )
{
    Login_clp clp;
    
    clp=(void *)(word_t)server->val;

    FREE(clp->st);
}
