/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      app/nash
**
** FUNCTIONAL DESCRIPTION:
** 
**      NASH:  Nemesis Shell
**
**      All those bits of Clanger that are actually useful. :-)
**      Start domains and threads easily from a command line.
** 
** ENVIRONMENT: 
**
**      User Application
** 
** FILE :	nashst.h
** CREATED :	Fri Jan 31 1997
** AUTHOR :	Paul Barham (Paul.Barham@cl.cam.ac.uk)
** ID : 	$Id: nashst.h 1.2 Wed, 02 Jun 1999 13:55:50 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <Nash.ih>
#include <FSDir.ih>
#include <FSUtil.ih>
#include <Exec.ih>
#include <Clanger.ih>
#include <CLine.ih>
#include <ClangerMod.ih>
#include <StringTbl.ih>
#include <stdio.h>
#include <exceptions.h>
#include <time.h>
#include <ecs.h>
#include <IDCMacros.h>
#include <ctype.h>
#define INHERIT_IN  1
#define INHERIT_OUT 2
#define INHERIT_ERR 4
#define INHERIT_FS  8

typedef struct {
    ContextMod_clp cmod;
/*    Context_clp    env; */
    Exec_clp       exec;
    Rd_clp         raw_in;
    CLineCtl_clp   clinectl;
    Heap_clp       serialheap;
    char          *buf;
    bool_t         really;
    char          *clangerbuf;
    Clanger_clp    clanger;
    Context_clp    clangerroot;
    Wr_clp         raw_out;
    Wr_clp         originalout;
    char          *hostname;

/* Filesystems */
    Context_clp    fsctx;
    Context_clp    mounts;
    FSUtil_clp     fsutils;
    FSDir_clp      pwd;      /* FSDir belonging to current FS */
    StringTbl_clp  fs_cwds;  /* Table of pwd strings, per fs */
    string_t       current_fs;
    string_t       cwdname;  /* For convenience; also stored in fs_cwds */

    char          *homedir; /* Something to do with login... */
    bool_t         foreground;
    Rd_clp         gdbrd; /* XXX; exec puts these in to priv root if they */
    Wr_clp         gdbwr; /* are non null */
    int            inherit_flag; 
    DomainMgr_clp  dm;
    Nash_cl        closure;
    char           *prompt;
} nash_st;



typedef int builtin_fn(nash_st *st, int argc, char **argv);

typedef struct { 
     char 	*name;
     builtin_fn 	*function;
     char *description;
 } cmd_t;

extern builtin_fn builtin_cd, builtin_really, builtin_ls, builtin_mode,
     builtin_mountfs, builtin_jobs, builtin_kill, builtin_pwd,
     builtin_setenv, builtin_printenv, builtin_exec, builtin_clanger,
     builtin_whoami, builtin_debug, builtin_reboot, builtin_boot, 
     builtin_exit, builtin_sub, builtin_ps, builtin_dominfo,
     builtin_gdb, builtin_inherit,
     builtin_source, builtin_help, builtin_cdfs;

int parse(nash_st *st, char *cmdline);
void init(nash_st *st);
extern const cmd_t builtins[];

int main(int, char **); 

extern const char * const msg[];

#define PATHMAX 256

char *gethostname(void);

void canon_path(char *path);

#ifdef DEBUG
#define TRC(_x) _x
#define FTRC(format, args...)			\
fprintf(stderr,					\
	__FUNCTION__": " format			\
	, ## args				\
      ); PAUSE(SECONDS(0.25));
#else
#define TRC(_x)
#define FTRC(format, args...)
#endif
#define DB(_x) _x
#define UNIMPLEMENTED  printf("UNIMPLEMENTED: " __FUNCTION__ " with state record %p\n",st)
