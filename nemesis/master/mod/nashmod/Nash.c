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
** ID : $Id: Nash.c 1.4 Wed, 02 Jun 1999 13:55:50 +0100 dr10009 $
**
**
*/


#include <nemesis.h>
#include <stdlib.h>
#include <stdio.h>
#include <Nash.ih>
#include <nashst.h>
#include <autoconf/bootloader.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef CONFIG_BROKEN_BOOTLOADER
#define KINDNAME "bootloader"
#define DEFAULT_PROMPT \
({						\
char buf[80];					\
sprintf(buf, "%s boot$", gethostname());	\
strdup(buf);					\
})
#else
#define KINDNAME "nash"
#define DEFAULT_PROMPT strdup(KINDNAME "$")
#endif
    
    
static	Nash_ExecuteString_fn 		Nash_ExecuteString_m;
static	Nash_GetPrompt_fn 		Nash_GetPrompt_m;
static	Nash_Destroy_fn 		Nash_Destroy_m;

static	Nash_op	ms = {
    Nash_ExecuteString_m,
    Nash_GetPrompt_m,
    Nash_Destroy_m
};


const char * const msg[] = {
    "<no error>",
    "No such file or directory",
    "File exists",
    "Permission denied",
    "unhandled exception"
};

const cmd_t builtins[] = {
    { "help",           builtin_help, 
      "Nash; the NAsty SHell.\n\nUsed for both bootloaders and standard Nemesis use.\n"
      "A bootloader nash first executes, if it exists\nNFSROOT/autoboot/autoexec.machinename.nash. Bootloaders then execute\n~/autoexec.bootloader.nash. Standard nashes, however, execute ~/autoexec.nash.\nFor each shell thread, a standard nash executes profile.nash. A bootloader nash\nexecutes NFSROOT/autoboot/profile.machinename.nash.\nInvoke help with one of the following to get help on that command:\n"},
    { "cd",		builtin_cd,
      "Change directory. If no arguments are specified, goes to the original directory.\n If an argument is specified, it is taken as a path and that directory is entered.\nIf the path begins with ~, it is taken as relative.\n"
    },
    { "cdfs",		builtin_cdfs,
      "Change the current filesystem.\n"
    },
    { "pwd",		builtin_pwd,
      "Print what Nash things the current working directory is. \nSymlinks confuse nash's notion of the working directory.\n"
    },
    { "really",		builtin_really,
      "really with no arguments causes programs started later to be given kernel\nprivileges.\n",
    },
    { "ls", 		builtin_ls,
      "List the current directory, or a directory specified as an argument\n."
    },
    { "mount", 		builtin_mountfs,
      "Instructs nash to attempt a filesystem mount, or list mounted filesystems.\n"},
    { "ps",             builtin_ps,
      "Displays a list of the domains currently running on the machine.\n"},
    { "dominfo",        builtin_dominfo,
      "Displays debugging information about a domain.\n"},
    { "kill", 		builtin_kill,
      "If a number is given as an argument, that domain is killed.\nIf a percentage sign followed by a number is given, that job is killed.\n"},
    { "setenv", 	builtin_setenv,
      "Set an environment variable. Deprecated.\n"},
    { "printenv", 	builtin_printenv,
      "Print an environment variable. Deprecated.\n"},
    { "!",              builtin_clanger,
      "Parse everything following the ! to a clanger interpreter.\n"},
    { "whoami",         builtin_whoami,
      "Indicate value of the really flag.\n"},
    { "debug",          builtin_debug,
      "Invoke ntsc_dbgstop().\n"},
    { "reboot",         builtin_reboot,
      "Invoke ntsc_reboot().\n"},
    { "b",              builtin_boot,
      "Chainload a boot image (if chainloading is available).\nThe boot image is either named image or the value of the opitonal argument.\n"},
    { "boot",           builtin_boot,
      "Chainload a boot image (if chainloading is available).\nThe boot image is either named image or the value of the opitonal argument.\n"},
    { "gdb",            builtin_gdb,
      "Execute the argument but with UDP user space remote debugging stubs.\n"},
    { "inherit",        builtin_inherit,
      "Each argument should consist of a + or - followed by one of (in,out,err,fs).\nIf the argument starts with +, inheritance is turned on, or off for -.\nin, out and err control naive inheritance of characeter IO from nash.\nfs controls whether the current mount and working directory are passed on or\nwhether the child must invent it's own.\n"},
    { "source",         builtin_source,
      "Source Nash script specified as an argument.\n"},
    { 0, 0, 0}
    
};
/* Make "path" canonical.  That is, remove "//", "/./", "foo/.." */
void canon_path(char *path)
{
    char *src, *dest, *prev;
    
    src = path;
    dest = path;
    prev = NULL;  /* previous path component */
    
    /* skip leading "./" */
    if (src[0] == '.' && src[1] == '/')
	src+=2;
    
    /* forwards, doing "//" -> "/",  and "/./" -> "/"   */
    while(*src)
    {
	/* "//" -> "/" */
	if (src[0] == '/' && src[1] == '/')
	{
	    src++;
	    continue;
	}
	
	/* "/./" -> "/" */
	if (src[0] == '/' && src[1] == '.' &&
	    (src[2] == '/' || src[2] == '\000'))
	{
	    src+=2;
	    continue;
	}
	
	/* "/foo/../" -> "/" */
	if (src[0] == '/' && src[1] == '.' && src[2] == '.' && 
	    (src[3] == '/' || src[3] == '\000') &&
	    prev != NULL)
	{
	    /* doing "/foo/../" -> "/"   */
	    dest = prev;  /* set dest to overwrite the previous component */
	    src+=3;	  /* skip the /../ */
	    /* find the new prev */
	    do {
		if (prev == path)  /* start of string */
		    prev = NULL;
		else
		    prev--;
	    } while(prev != NULL && *prev != '/');
	    continue;
	}
	
	/* if we get this far with a slash, and haven't tripped any special
	 * cases, then we have a plausable previous component. */
	if (src[0] == '/')
	    prev = src;

	*dest++ = *src++;
    }
    *dest = '\000';
}

char *gethostname(void) {
    Type_Any any;
    NOCLOBBER int i;
    Time_ns timeout; 
     
     
    TRC(printf("Getting myHostName\n"));
     
#define MAX_TRIES 8
    timeout = TENTHS(1);
    for(i = 0; i < MAX_TRIES; i++) {
	if(Context$Get(Pvs(root), "svc>net>eth0>myHostname", &any))
	    break;
	PAUSE(timeout);
	timeout <<= 1;
    }
     
    if(i == MAX_TRIES) {
	fprintf(stderr, "nash: timed out waiting for eth0. ");
	fprintf(stderr, "not using autoboot.\n");
	return NULL;
    }
    return NARROW(&any, string_t);
}    

int parse(nash_st *st, char *cmdline)
{
    builtin_fn *command = builtin_exec;
    int j;

    char *p;
    int i;
    
    int argc = 0;
    char *argv[16];


    st->foreground = True;
    p = cmdline;

    argc = 0;
    TRC(printf("Parsing \"%s\"\n", cmdline));


    if (*p == '!') {
	sprintf(st->clangerbuf, "%s\n", p+1);
	Clanger$ExecuteString(st->clanger, st->clangerbuf, Pvs(out));
	return 0;
    }

    while (*p) {
	/* Skip leading space */
	while (*p && isspace(*p)) p++;

	/* Do we have an argument */
	if (!*p || *p=='#') break;
	argv[argc] = p;
	
	/* Find the end */
	
	while (*p && !isspace(*p)) p++;
	if (*p) { *p++ = 0; }
	TRC(printf("argv[%d] = \"%s\"\n", argc, argv[argc]));
	argc++;
    }
    if (!argc) return -1;

    if (!strcmp (argv[argc-1], "&")) {
	st->foreground = False;
	argc--;
    }

    for (i = 0; builtins[i].name != 0; i++) {
	if (!strcmp(builtins[i].name, argv[0])) {
	    command = builtins[i].function;
	    st->foreground = 0;
	    break;
	}
    }
    j = (*command)(st, argc, argv);

    if (j < 0) {
	printf("nash: %s: command not found\n", argv[0]);
	return -1;
    }
    return 0;
}



Nash_clp NewNash(bool_t runprofile) {
    int r;
    MergedContext_clp merged;
    nash_st *st = calloc(1,sizeof(*st));
    st->closure.op = &ms;
    st->closure.st = st;

    TRC(printf("nash: getting FSUtil.\n"));
    st->fsutils = NAME_FIND("modules>FSUtil", FSUtil_clp);
    st->cmod = NAME_FIND("modules>ContextMod", ContextMod_clp);
    st->dm = IDC_OPEN("sys>DomainMgrOffer", DomainMgr_clp);

    
    {
	Type_Any any;
	Context_clp localchanges;
	merged = ContextMod$NewMerged(st->cmod, Pvs(heap), Pvs(types));


	localchanges = ContextMod$NewContext(st->cmod, Pvs(heap), Pvs(types));
	MergedContext$AddContext(merged, localchanges, MergedContext_Position_Before, False, NULL);

	if (Context$Get(Pvs(root), "svc>program_environments", &any)) {
	    MergedContext$AddContext(merged, NAME_FIND("svc>program_environments", Context_clp), MergedContext_Position_After, False, NULL);
	}
    }
    r = builtin_mountfs(st, 0, NULL);

    if (r != -1) {
	st->homedir = strdup(st->cwdname);
#if 1
	fprintf(stderr,"Home directory %s\n", st->homedir);
#endif
    }
    /* Exec module */
    TRC(printf("nash: Finding the Exec module...\n"));
    st->exec = NAME_FIND("modules>Exec", Exec_clp);
    
    TRC(printf("Starting ClangerMod\n"));
    st->clanger = ClangerMod$New(NAME_FIND("modules>ClangerMod", 
					   ClangerMod_clp));
    Clanger$ExecuteString(st->clanger, "l=modules>CExec\n", Pvs(out));
    TRC(printf("Calling GetRoot\n"));
    st->clangerroot = Clanger$GetRoot(st->clanger);
    CX_ADD_IN_CX(st->clangerroot, "penv", merged, MergedContext_clp);

    st->prompt = DEFAULT_PROMPT;

    st->inherit_flag = INHERIT_FS;
    
    st->buf = Heap$Malloc(Pvs(heap), 256);
    st->clangerbuf = Heap$Malloc(Pvs(heap), 256);
    
    
    if(st->fsctx)
    {
	char fname[128];
	FILE *NOCLOBBER f;
	bool_t present;
	/* a bootloader runs boot/autoboot/profile.machinename.nash 
	   whereas a normal nash runs profile.nash */
	
#ifdef CONFIG_BROKEN_BOOTLOADER
	strcpy(fname, st->homedir);
	strcat(fname, "/../autoboot/profile.");
	canon_path(fname); 	/* textual strip out of the machine name */
	strcat(fname, gethostname());
	strcat(fname, ".nash");
#else
	sprintf(fname, "profile.%s", KINDNAME);
#endif
	f = fopen(fname, "r");
	if (f) {
	    present = 1; 
	    fclose(f);
	} else present = 0;
	
	if (present) {
	    char* argv[2];
	    
	    argv[0] = "source";
	    argv[1] = fname;
	    builtin_source(st, 2, argv);
	}
    } 
    st->gdbrd = NULL;
    st->gdbwr = NULL;
    TRC(printf("Done init\n"));

    return &st->closure;
}



/*---------------------------------------------------- Entry Points ----*/

static int32_t Nash_ExecuteString_m (
    Nash_cl	*self,
    string_t	str	/* IN */ )
{
    nash_st	*st = self->st;
    char *ptr;
    int r;

    ptr = strdup(str);
    r= parse(st, ptr);
    free(ptr);
    return r;

}

static string_t Nash_GetPrompt_m (
    Nash_cl	*self )
{
    nash_st	*st = self->st;

    return st->prompt;
}

static void Nash_Destroy_m (
    Nash_cl	*self )
{
    nash_st	*st = self->st;

    /* XXX; freeing up everything else may be an idea */
    free(st);
    return;
}

/*
 * End 
 */
