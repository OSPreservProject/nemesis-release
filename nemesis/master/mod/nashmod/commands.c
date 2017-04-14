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
** FILE :	commands.c
** CREATED :	Fri Jan 31 1997
** AUTHOR :	Paul Barham (Paul.Barham@cl.cam.ac.uk)
** ID : 	$Id: commands.c 1.5 Thu, 10 Jun 1999 14:42:18 +0100 dr10009 $
** 
**
*/


#include <nemesis.h>
#include <exceptions.h>
#include <ntsc.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <salloc.h>

#include <IDCMacros.h>

#include <DomainMgr.ih>
#include <ThreadF.ih>
#include <Closure.ih>
#include <CLine.ih>
#include <FSUtil.ih>
#include <FSClient.ih>
#include <FSDir.ih>
#include <Load.ih>
#include <Exec.ih>
#include <ClangerMod.ih>
#include <StringTblMod.ih>
#include <WordTblMod.ih>
#include <bstring.h>
#include <autoconf/bootloader.h>
#include <autoconf/memctxts.h>
#include <autoconf/memsys.h>
#include <CExec.ih>
#include <domid.h>

#include <ecs.h>
#include <time.h>
#include <dcb.h>
#include "nashst.h"

#define STACK_HEXDUMP_SIZE 128


void printstring(char*s) {
    while (*s) fputc(*s++, stdout);
 }

/*************************************************************/

int builtin_help (nash_st *st, int argc, char **argv) {
    int i;

    FTRC("entered, argc=%d, argv=%p.\n");

    if (argc != 2) {
	printstring(builtins[0].description);
	for (i=0; builtins[i].name; i++) {
	    printf("%s ", builtins[i].name);
	}
	printf("\n");
	return 0;
    } else {

	
	for (i=0; builtins[i].name; i++) {
	    if (!strcmp(builtins[i].name, argv[1])) {
		printf("%s\n\n", builtins[i].name);
		printstring(builtins[i].description);
		return 0;
	    }
	}
	
	printf("There is no built in command named %s\n", argv[1]);

    }
    return 1;
}

/*************************************************************/

int builtin_cd (nash_st *st, int argc, char **argv) 
{
    char * NOCLOBBER path;

    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    if(st->fsctx == NULL) {
	printf("cd: no filesystem (try 'mountfs').\n");
	return 0;
    }
    
    switch (argc) {
    case 1:
	path = st->homedir;
	break;
    case 2:
	path = argv[1];
	break;
    default:
	printf("usage: cd [directory]\n");
	return 0;
	break;
    }

    if (path[0] == '~') {
	char *newargv[2];
	newargv[0] = argv[0];
	newargv[1] = st->homedir;
	builtin_cd(st, 2, newargv);
	if (path[1] == '/') {
	    path++; 
	} else if (path[1]) {
	    newargv[1] = "..";
	    builtin_cd(st, 2, newargv);
	}
	path++;
    }
	
    if(FSDir$Lookup(st->pwd,
		    path,
		    True) 
       != FSTypes_RC_OK)
    {
	printf("nash: %s: No such file or directory\n", path);
	return 0;
    }

    if(FSDir$CWD(st->pwd)
       != FSTypes_RC_OK)
    {
	printf("nash: %s: could not CWD\n", path);
	return 0;
    }

    /* Work out what the name of the cwd is now */
    if (path[0] == '/')
    {
	strcpy(st->cwdname, path);
    }
    else
    {
	/* tack on the path, then re-canonicalize it */
	strcat(st->cwdname, "/");
	strcat(st->cwdname, path);
	canon_path(st->cwdname);
    }

    return 0;
}

/*************************************************************/

int builtin_pwd (nash_st *st, int argc, char **argv) 
{
    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    if(st->fsctx == NULL) {
	printf("pwd: no filesystem (try 'mountfs').\n");
	return 0;
    }

    printf("%s:%s\n", st->current_fs, st->cwdname);
    return 0;
}

/*************************************************************/

int builtin_really(nash_st *st, int argc, char **argv) 
{
    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    switch (argc) {
    case 1:
	if(!st->really) {
	    st->really = True;
	    st->prompt[strlen(st->prompt)-1] = '#';
#if 0
	    Context_clp cpu = NAME_LOOKUP("qos>cpu", st->env, Context_clp);
	    Context$Remove(cpu, "k");
	    CX_ADD_IN_CX(cpu, "k", st->really, bool_t);
#endif /* 0 */

	} else printf("nash: already in priv mode.\n");
	break;
    case 2:
	if(st->really) {
	    if(!strcmp(argv[1], "-n")) {
#if 0
		Context_clp cpu = NAME_LOOKUP("qos>cpu", st->env, Context_clp);

		Context$Remove(cpu, "k");
		CX_ADD_IN_CX(cpu, "k", st->really, bool_t);
#endif /* 0 */
		st->really = False;
		st->prompt[strlen(st->prompt)-1] = '$';

	    } else printf("usage: really [ -n ]\n");
	} else printf("nash: not in priv mode.\n");
	break;
    default:
	printf("usage: really [ -n ]\n");
	return 0;
	break;
    }

    return 0;
}

/*************************************************************/

static int my_strcmp(const void *k1, const void *k2) 
{
    return strcmp(* (const char **) k1,* (const char **) k2);
}


typedef SEQ_STRUCT(string_t) string_sequence;

static void ls_dir(nash_st *st, string_t name)
{
    FSTypes_InodeType type;
    FSTypes_DirentSeq *names;
    string_sequence   *all_names;
    int                j;
    uint32_t           width, cols;

    /* At this point the current FSDir is pointing at a file or directory
       (or something else, but let's not worry about that now). We want
       to DTRT whatever it is. */

    if (FSDir$InodeType(st->pwd,&type)!=FSTypes_RC_OK) {
	printf("ls: can't stat %s\n",name);
    }

    switch(type) {
    case FSTypes_InodeType_Dir:

	all_names = SEQ_NEW(string_sequence, 0, Pvs(heap));
	for(;;)
	{
	    FTRC("initialising names.\n");
	    if ((FSDir$ReadDir(st->pwd,&names)!= FSTypes_RC_OK) ||
		SEQ_LEN(names) == 0)
	    {
		FTRC("freeing names.\n");
		SEQ_FREE(names);
		break;
	    }	       		
	    for (j=0;j<SEQ_LEN(names);j++) {
		SEQ_ADDH(all_names, strdup(SEQ_ELEM(names, j).name));
		FREE(SEQ_ELEM(names,j).name);
	    }
		
	    SEQ_FREE(names);
	}

	FTRC("printing names.\n");

    /* Sort them */
	qsort (SEQ_START(all_names), SEQ_LEN(all_names), 
	       SEQ_DSIZE(all_names), my_strcmp); 
    
	width = 1;
	/* Print them */
	for (j=0; j<SEQ_LEN(all_names); j++) {
	    if (strlen(SEQ_ELEM(all_names,j)) > width)
		width = strlen(SEQ_ELEM(all_names,j));
	}
	width += 2;
	cols = 79/width;
	for (j=0; j<SEQ_LEN(all_names); j++) {
	    printf("%-*s ", width-1, SEQ_ELEM(all_names, j));
	    if ((j % cols) == (cols-1)) printf("\n");
	}
	j--;
	if ((j % cols) != (cols-1)) printf("\n");
    
	FTRC("freeing all_names.\n");
	SEQ_FREE_ELEMS(all_names);
	SEQ_FREE(all_names);
    
	printf("\n");
	break;
    case FSTypes_InodeType_File:
	printf("%s is a file\n",name);
	break;
    case FSTypes_InodeType_Link:
	printf("%s is a symlink\n",name);
	break;
    case FSTypes_InodeType_Special:
	printf("%s is a special file\n",name);
	break;
    default:
	printf("%s is a typo (XXX should never happen)\n",name);
	break;
    }
}

int builtin_ls (nash_st *st, int argc, char **argv) 
{
    char                      *path;
    int NOCLOBBER              i;
    int                        gotsome = 0;

    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    if(st->fsctx == NULL) {
	printf("ls: no filesystem (try 'mountfs').\n");
	return 0;
    }

    FTRC("looking at arguments.\n");
    for (i = 1; i < argc; i++) {    
	if (!strcmp(argv[i], "-l")) {
	    FTRC("found -l.\n");
	    printf("Nash's not unix!\n");
	} else {
	    gotsome = 1;

	    /* Absolute path? */
	    path = argv[i];	   	    

	    FTRC("looking at directory \"%s\".\n", path);

	    if(FSDir$Lookup(st->pwd,
			    path,
			    True) 
	       != FSTypes_RC_OK)
	    {
		printf("ls: No such file or directory \"%s\"\n", path);
		continue;
	    }
	    
	    printf("%s:\n", path);
	    ls_dir(st, path);
	}

	FTRC("i=%d, argc=%d.\n", i, argc);
    }

    if(!gotsome)
    {
	FTRC("looking at directory \".\".\n");

	if(FSDir$Lookup(st->pwd,
			".",
			True) 
	   != FSTypes_RC_OK)
	{
	    printf("ls: No such file or directory \".\"\n");
	    return 0;
	}
	ls_dir(st, ".");
    }

    FTRC("returning.\n");
    return 0;
}

/*************************************************************/

void hexdump(uint64_t *ptr, void *base, int count) {
    int i;
    int j;
    j = 0;
    while (1) {
	printf("%p: ", base + j);
	for (i=0; i<4; i++) {
	    printf("%qx ", *ptr++);
	    count-=8;
	    j+= 8;
	    if (count <= 0) return;
	}
	printf("\n");
    }
}


#define PREFIX(_str, _prefix) !strncmp(_str, _prefix, strlen(_prefix))

#define POSTFIX(_str, _prefix) 						\
({									\
  int length, prefixlength;						\
  int result;								\
  length = strlen(_str);						\
  prefixlength = strlen(_prefix);					\
  result = 0;								\
  if (length > prefixlength) {						\
      if (!strcmp( _str + (length-prefixlength), _prefix)) result = 1;	\
  }									\
  result;								\
})


#define HEAP_STAT_FIELDS 4
#ifdef CONFIG_MEMSYS_STD
#define DOMINFO_STACKSIZING
#define DOMINFO_EPCOUNTING
#define DOMINFO_HEAPSIZING 
#define DOMINFO_PROGSTRSIZING
#endif
#ifdef CONFIG_MEMSYS_EXPT
/* stack sizing doesn't work on MEMSYS_EXPT because stacks are demand
   backed with memory and the stack sizing routine below expects */
#define DOMINFO_EPCOUNTING
#define DOMINFO_HEAPSIZING 
#define DOMINFO_RSSMONITOR
#define DOMINFO_PROGSTRSIZING
#endif

#ifdef DOMINFO_RSSMONITOR
#include <autoconf/rssmonitor_anon.h>
#ifdef CONFIG_RSSMONITOR_ANON_SUPPORT
#include <RSSMonitor.ih>
#endif
#define DOMINFO_RSSMONITOR_FORREAL
#endif

int builtin_dominfo(nash_st *st, int argc, char **argv) {
    NOCLOBBER int i;
    int domnum;
    char *domname;
    bool_t text_monitor, data_monitor;
    Context_clp dominfo;
    int nthreads, threadnum;
    int nheaps, heapnum;
    int nprogstrs, progstrnum;
    Context_Names *names;
    struct threadinfo {
	string_t name;
	Stretch_clp stretch;
	addr_t base;
	word_t size;
	word_t space;
    } *threads;
    struct heapinfo {
	string_t name;
	Heap_clp heap;
	uint64_t stats[HEAP_STAT_FIELDS];
    } *heaps;
    struct progstrinfo {
	string_t name;
	bool_t   isdata;
	Stretch_clp stretch;
	bool_t   monitored;
	addr_t  base;
	word_t  size;
    } *progstrs;

    if (argc == 1) {
	printf("Usage: dominfo [options] domain number\n");
#ifdef DOMINFO_RSSMONITOR_FORREAL
	printf("  -t   show RSS sizes of text stretches\n");
	printf("  -d   show RSS sizes of data stretches\n");
#endif
    }

    text_monitor=False;
    data_monitor=False;

    for (i=1; i<argc-1; i++) {
	if (!strcmp(argv[i], "-t")) {
	    text_monitor = True;
	}
	if (!strcmp(argv[i], "-d")) {
	    data_monitor = True;
	}
    }

    domnum = atoi(argv[argc-1]);
    
    domname = malloc(64);
    if (!domname) {
	printf("Malloc failed\n");
	return 1;
    }
    sprintf(domname, "proc>domains>%qx", DOM_NEMESIS + domnum);
    printf("Domain %d context %s; ", domnum, domname);
    if (!NAME_EXISTS(domname)) {
	printf("Domain proc context does not exist; did you type a dead or nonexsitent domain number in?\n");
	return 2;
    }
    dominfo = NAME_FIND(domname, Context_clp);
    if (NAME_EXISTS_IN_CX("name", dominfo)) {
	printf("Name %s\n", NAME_LOOKUP("name", dominfo, string_t));
    } else {
	printf("(noname!)\n");
    }


    names = Context$List(dominfo);

    nthreads = 0;    
    nheaps = 0;
    nprogstrs = 0;
    for (i=0; i<SEQ_LEN(names); i++) {
	string_t name = SEQ_ELEM(names, i);
	if (PREFIX(name, "thread_") && POSTFIX(name, "_stack")) nthreads++;
	if (PREFIX(name, "heap_")) nheaps++;
	if ( (PREFIX(name, "data_") || PREFIX(name, "text_")) 
	     && NAME_LOOKUP(name, dominfo, Stretch_clp)) nprogstrs++;
    }

    printf("Number of threads: %d\n", nthreads);
    printf("Number of heaps:   %d\n", nheaps);
    printf("Number of program stretches: %d\n", nprogstrs);

    threads = calloc(sizeof(struct threadinfo), nthreads);
    heaps = calloc(sizeof(struct heapinfo), nheaps);
    progstrs = calloc(sizeof(struct progstrinfo), nprogstrs);
    threadnum = 0;
    heapnum = 0;
    progstrnum = 0;

    for (i=0; i<SEQ_LEN(names); i++) {
	string_t name = SEQ_ELEM(names, i);
	if (PREFIX(name, "thread_") && POSTFIX(name, "_stack") ) {
	    if (threadnum == nthreads) {
		printf("Number of threads in domain grew under dominfo's feet; try again\n");
		return 1; 
	    }
	    threads[threadnum].name = name;
	    threads[threadnum].stretch = NAME_LOOKUP(name, dominfo, Stretch_clp);
	    threadnum++;
	}
	if (PREFIX(name, "heap_")) {
	    if (heapnum == nheaps) {
		printf("Number of heaps in domain grew under dominfo's feet; try again\n");
		return 1;
	    }
	    heaps[heapnum].name =name;
	    heaps[heapnum].heap = NAME_LOOKUP(name, dominfo, Heap_clp);
	    heapnum++;
	}   

	if (PREFIX(name,"text_") || PREFIX(name,"data_")) {
	    Stretch_clp stretch = NAME_LOOKUP(name, dominfo, Stretch_clp);
	    if (stretch) {
		if (progstrnum == nprogstrs) {
		    printf("Number of program stretches in domain grew under dominfo's feet; try again\n");
		    return 1;
		}
		progstrs[progstrnum].name = name;
		progstrs[progstrnum].stretch = stretch;
		if (PREFIX(name,"data_")) {
		    progstrs[progstrnum].isdata = True;
		} else {
		    progstrs[progstrnum].isdata = False;
		}
		progstrs[progstrnum].monitored = False;
		progstrnum++;
	    }
	}
    }

    if (heapnum != nheaps || threadnum != nthreads || progstrnum != nprogstrs) {
	printf("Number of heaps, threads or program stretches in domain shrunk under dominfo's feet; try again (%d,%d,%d) instead of (%d,%d,%d)\n",
	       heapnum, threadnum, progstrnum,
	       nheaps, nthreads, nprogstrs);
	return 1;
    }
    
    /* now do some things that need kernel privileges, if we have them */
    if (NAME_FIND("self>kpriv", bool_t) && NAME_EXISTS_IN_CX("dcbro", dominfo)) {
	/* engaging kernel privileges for further 
	   grunging around inside */
	uint32_t free_endpoints = 0;
	uint32_t neps;
	dcb_ro_t *dcbro;
	int i, j;

	dcbro = (dcb_ro_t*) NAME_LOOKUP("dcbro", dominfo, addr_t);
	/* printf("(Engaging kernel privileges...\n"); */
	ENTER_KERNEL_CRITICAL_SECTION();
#ifdef DOMINFO_EPCOUNTING
#ifdef INTEL
	/* count the free endpoints */
	neps = dcbro->num_eps;
	{
	    ep_rw_t *eprw;
	    eprw = dcbro->dcbrw->free_eps;
	    while (eprw) {
		free_endpoints ++;
		eprw = (ep_rw_t*) eprw->ack;
	    }
	}
#endif		
#endif
#ifdef DOMINFO_STACKSIZING
	/* size the stacks */
	for (i=0; i<nthreads; i++) {
	    uint32_t * ptr;
	    int inarow;
/* look for MAGICWORD repeated MAGICLENGTH times */
#define MAGICWORD 0xd0d0d0d0U
#define MAGICLENGTH 16
	    threads[i].base = STR_RANGE(threads[i].stretch, &(threads[i].size));
	    inarow = 0;
	    /* start at 4 bytes down from the top of the stack */
	    ptr = ((uint32_t *)threads[i].base);
	    ptr += (threads[i].size / (sizeof(uint32_t)));
	    ptr -= 1;
	    while (inarow < MAGICLENGTH && ptr > ((uint32_t *) threads[i].base)) {
		if (*ptr-- == MAGICWORD) {
		    inarow++;
		} else {
		    inarow = 0;
		}
	    }
	    
	    threads[i].space = (char *) ptr - (char *) (threads[i].base);
	}
#endif DOMINFO_STACKSIZING
#ifdef DOMINFO_HEAPSIZING
	for (i=0; i<nheaps; i++) {
	    for (j=0; j<HEAP_STAT_FIELDS; j++) {
		if (!Heap$Query(heaps[i].heap, (Heap_Statistic) j, &(heaps[i].stats[j]))) {
		    eprintf("Heap Query on heap name %s parameter %d failed\n", heaps[i].name, j);
		}
	    }
	}
#endif
#ifdef DOMINFO_PROGSTRSIZING
	for (i=0; i<nprogstrs; i++) {
	    progstrs[i].base = 
		STR_RANGE(progstrs[i].stretch, &(progstrs[i].size));
	}
#endif
	LEAVE_KERNEL_CRITICAL_SECTION();
	/* printf("...kernel privileges released)\n"); */
	/* now report on the things we gleaned with kernel privileges */

#ifdef DOMINFO_EPCOUNTING
	printf("Endpoints: %d used of %d (%d free)\n", neps - free_endpoints, neps, free_endpoints);
#endif
#ifdef DOMINFO_STACKSIZING
	for (i=0; i<nthreads; i++) {
	    /* assume < 2GB stacks ... */
	    printf("%s stack (%p,0x%x) unused space 0x%x\n", 
		   threads[i].name, threads[i].base, 
		   (uint32_t) threads[i].size, (uint32_t)threads[i].space);
	    if (!threads[i].space) {
		void *bytes = malloc(STACK_HEXDUMP_SIZE);
		printf("Potential stack overflow; Start of stack is:\n");
		ENTER_KERNEL_CRITICAL_SECTION();
		memcpy(bytes, threads[i].base, STACK_HEXDUMP_SIZE);
		LEAVE_KERNEL_CRITICAL_SECTION();
		hexdump(bytes, threads[i].base, STACK_HEXDUMP_SIZE);
		free(bytes);
	    }
	}
#endif
#ifdef DOMINFO_HEAPSIZING
	for (i=0; i<nheaps; i++) {
	    printf(
		"%s holds 0x%x currently 0x%x limit 0x%x overhead 0x%x\n", 
		heaps[i].name,
		(uint32_t) heaps[i].stats[Heap_Statistic_UsefulData],
		(uint32_t) heaps[i].stats[Heap_Statistic_CurrentSize],
		(uint32_t) heaps[i].stats[Heap_Statistic_MaximumSize],
		(uint32_t) heaps[i].stats[Heap_Statistic_AllocatedData]-
		(uint32_t) heaps[i].stats[Heap_Statistic_UsefulData]);
	}
#endif
#ifdef DOMINFO_PROGSTRSIZING
	for (i=0; i<nprogstrs; i++) {
	    printf("%s base %p size %x\n", progstrs[i].name,
		   progstrs[i].base, (uint32_t) progstrs[i].size);
	}
#endif
    }
#ifdef DOMINFO_RSSMONITOR_FORREAL
    if (text_monitor || data_monitor) {
	RSSMonitor_clp rss;
	RSSMonitor_LongCardinalSequence *seq, *oldseq;
	int i, j;
	if (!NAME_EXISTS_IN_CX("RSSMonitor", dominfo)) {
	    printf("RSSMonitor module is not installed; try rebuilding with tweak('Builder', makevars, {'DEFINES': '-DEXPORT_RSSMONITOR'}) in your choices file to enable\n");
	} else {
	    rss = IDC_OPEN_IN_CX(dominfo, "RSSMonitor", RSSMonitor_clp);
	    
	    printf("Adding stretches\n");
	    for (i = 0; i<nprogstrs; i++) {
		if ( (progstrs[i].isdata && data_monitor) ||
		     (!progstrs[i].isdata && text_monitor)) {
		    bool_t success;
		    printf("Adding %s (%p)\n", progstrs[i].name,
			   progstrs[i].stretch);
		    PAUSE(SECONDS(1));
		    success = RSSMonitor$AddStretch(rss,  progstrs[i].stretch);
		    PAUSE(SECONDS(1));
		    if (!success) {
			printf("Failed to add stretch\n");
		    } else {
			progstrs[i].monitored = True;
		    }
		}
	    }
	    
	    printf("Resetting all stretches\n");
	    seq = RSSMonitor$SampleAll(rss, RSSMonitor_Action_RESET);
	    oldseq = seq;
	    /* pause for a while for the measurement to happen */
	    printf("Pausing for 10 seconds...");
	    PAUSE(SECONDS(10));
	    printf("\n");
	    seq = RSSMonitor$SampleAll(rss, RSSMonitor_Action_SAMPLE);
	    free(oldseq);
	    j = 0;
	    for (i=0; i<nprogstrs; i++) {
		if (progstrs[i].monitored) {
		    if (j == SEQ_LEN(seq)) {
			printf("Too many results from RSSMonitor$SampleAll\n");
		    } else {
			uint64_t r = SEQ_ELEM(seq, i);
			uint64_t size = progstrs[i].size;
			int percent = (r*4096*100)/size;
			printf("Stretch %s (%p) 0x%x touched pages of 0x%x (%d%%)\n",
			       progstrs[i].name, progstrs[i].stretch, 
			       (uint32_t) r, (uint32_t) (size / 4096), percent);
			j++;
		    }
		}
	    }
	    free(seq);
	}
    
    }
#endif

    free(domname);
    free(threads);
    free(heaps);
    return 0;
        
    }

int builtin_ps (nash_st *st, int argc, char **argv) 
{
    NOCLOBBER int i;
    int domnum;
    Context_clp proccx;
    Context_Names *names;
#ifdef CONFIG_MEMCTXTS
    NOCLOBBER word_t allmem, totrsvd;
#endif
    
    
    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    proccx = NAME_FIND("proc>domains", Context_clp);

    
    names  = Context$List(proccx);
#ifdef CONFIG_MEMCTXTS
    allmem = 0; 
    printf("DomID priv neps nctx memory %-*s name\n", sizeof(addr_t)*2 + 2, "dcbro"); 
#else
    printf("DomID priv neps nctx %-*s name\n", sizeof(addr_t)*2 + 2, "dcbro"); 
#endif
    for (i=0; i < SEQ_LEN(names); i++) {
#ifdef CONFIG_MEMCTXTS
	Context_cl *NOCLOBBER memcx; 
	Context_Names *strs; 
	IDC_Buffer *curbuf;
	string_t curstr;
	NOCLOBBER word_t totmem; 
#endif
	Context_clp domcx;
	string_t name;
	string_t realname;
	bool_t   k;
	uint32_t neps;
	uint32_t nctx;
	addr_t dcbro;

	name = SEQ_ELEM(names, i);
	
	TRY {
	    domcx = NAME_LOOKUP(name, proccx, Context_clp);
	    /* set buf to be something like 000000004 */
	    /* XXX; assume that the domain id is d000.......number */
	    domnum = strtol(name+8, (char**)NULL, 16);

	    realname = NAME_LOOKUP("name", domcx, string_t);
	    k = NAME_LOOKUP("kpriv", domcx, bool_t);
	    neps = NAME_LOOKUP("neps", domcx, uint32_t);
	    nctx = NAME_LOOKUP("nctxts", domcx, uint32_t);
	    dcbro = NAME_LOOKUP("dcbro", domcx, addr_t);

#ifdef CONFIG_MEMCTXTS	
	    totmem = 0; 
	    TRY {
		memcx = NAME_LOOKUP("mem", domcx, Context_clp);
	    } CATCH_ALL {
		memcx = NULL; 
	    } ENDTRY;
	   
	    if(memcx) {
		int j; 

		strs  = Context$List(memcx); 
		for(j = 0; j < SEQ_LEN(strs); j++) {
		    curstr  = SEQ_ELEM(strs, j); 
		    curbuf  = NAME_LOOKUP(curstr, memcx, IDC_Buffer); 
		    totmem += curbuf->w; 
		}
		SEQ_FREE_ELEMS(strs);
		SEQ_FREE(strs);
		totmem >>= 10; /* turn into Kb */
		printf("%4d  %s %4d %4d %6d %p %s\n", domnum, k ? "root":"user", 
		       neps, nctx, totmem, dcbro, realname);
		allmem  += totmem;
	    } else 
		printf("%4d  %s %4d %4d ?????? %p %s\n", domnum, k ? "root":"user", 
		       neps, nctx, dcbro, realname);
#else
	    printf("%4d  %s %4d %4d %p %s\n", domnum, k ? "root":"user", 
		   neps, nctx, dcbro, realname);
#endif
	} CATCH_ALL {
	    printf("%s went missing...\n",SEQ_ELEM(names,i));
	} ENDTRY;

    }

#ifdef CONFIG_MEMCTXTS
    TRY { 
		totrsvd = NAME_FIND("proc>meminfo>rsvd>total", word_t); 
	} CATCH_ALL { 
		totrsvd = 0; 
	} ENDTRY; 
    totrsvd >>= 10; /* turn into Kb */
    printf("\nMemory: %dK used (reserved %dK, domains %dK).\n\n", 
	   totrsvd + allmem, totrsvd, allmem); 
#endif
    SEQ_FREE_ELEMS(names);
    SEQ_FREE(names);
    return 0;
}

/*************************************************************/

int kill(nash_st *st, Domain_ID id) 
{
    FTRC("entered.\n");

    TRY {

	DomainMgr$Destroy(st->dm, id);

    } CATCH_Binder$Error(which) {
	if (which == Binder_Problem_BadID)
	    printf("nash: binder said BadID - maybe domain already dead?\n");
	else
	    RERAISE;
    } CATCH_ALL {
	printf("DomainMgr$Destroy raised an exception\n");
	RERAISE;
    } ENDTRY;

    return 0;
}    

int builtin_kill (nash_st *st, int argc, char **argv) 
{
    uint32_t j;
    char *ptr;
    char *num;
    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    if (argc == 2) {
	num =argv[1];
	j = strtol(num, &ptr, 0);
	if (j==0 && ptr == num) {
	    printf("Kill argument %s invalid\n", num);
	    return 1;
	}
	kill(st, 0xd000000000000000ull + j);

    } else {
	printf("usage: kill [%%job|id]\n");
    }
    return 0;
}

/*************************************************************/

int builtin_printenv (nash_st *st, int argc, char **argv) 
{
#if 0
    Context_Names *env;
    Type_Any      any;
    int i;
#endif /* 0 */

    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    printf("What do you expect this to do?\n");
#if 0
    env = Context$List(st->env);
    for (i = 0; i < SEQ_LEN(env); i++) {
	Context$Get(st->env, SEQ_ELEM(env, i), &any);
	printf("%s=%a\n", SEQ_ELEM(env, i), &any);
    }
    SEQ_FREE_ELEMS(env);
    SEQ_FREE(env);
#endif /* 0 */
    
    return 0;
}

/*************************************************************/

int builtin_setenv (nash_st *st, int argc, char **argv)
{
#if 0
    Type_Any any;
#endif /* 0 */

    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    if (argc != 3) return 0;

    printf("What do you expect this to do?\n");
#if 0
    if (Context$Get(st->env,argv[1], &any)) {
	FREE((string_t) ((word_t) any.val));
    } 
    Context$Remove(st->env, argv[1]);
    ANY_INIT(&any, string_t, strdup(argv[2]));
    Context$Add(st->env, argv[1], &any);
#endif /* 0 */
    return 0;
}

/*************************************************************/

int builtin_gdb(nash_st *st, int argc, char **argv) {
    Closure_clp netcons;
    Type_Any any;
    int port;
    Rd_clp ord;
    Wr_clp owr;
    port = 4099;

    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    if (Context$Get(Pvs(root), "nettermport", &any)) {
	port = (uint32_t) (NARROW(&any, int64_t));
	Context$Remove(Pvs(root),"nettermport");
    }
    port++;
    CX_ADD_IN_CX(Pvs(root), "nettermport", (int64_t)port, int64_t);
    netcons = NAME_FIND("modules>netterm", Closure_clp);
    printf("Setting up debugging on port %d\n", port);
    printf("Network debugging starting; execute:\nugdb %s %d\n************************************************************\n\n\n", NAME_FIND("svc>net>eth0>myHostname", string_t), port);


    ord  = Pvs(in); owr = Pvs(out);
    TRY {
	Closure$Apply(netcons);
	st->gdbrd = Pvs(in); st->gdbwr = Pvs(out);
    } CATCH_ALL {
	Pvs(in) = ord; Pvs(out) = owr;
	printf("Netcons raised exception\n");
    } ENDTRY;
    Pvs(in) = ord; Pvs(out) = owr;
    
    printf("Starting domain\n");
    printf("gdb in is %x, gdb out is %x\n", st->gdbrd, st->gdbwr);
    printf("Pvs(in) is %x, Pvs(out) is %x\n", Pvs(in), Pvs(out));
    builtin_really(st, 1, NULL);
    builtin_exec(st, argc-1, argv+1);
    st->gdbrd = NULL;
    st->gdbwr = NULL;
    return 0;
}

/*************************************************************/

int builtin_inherit(nash_st *st, int argc, char **argv) {
    int i;
    int rot;
    
    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    for (i=1; i<argc; i++) {
	rot = 0;
	if (!strcmp(argv[i]+1, "in")) rot = INHERIT_IN;
	if (!strcmp(argv[i]+1, "out")) rot = INHERIT_OUT;
	if (!strcmp(argv[i]+1, "err")) rot = INHERIT_ERR;
	if (!strcmp(argv[i]+1, "fs")) rot = INHERIT_FS;
	if (argv[i][0] == '-' && rot) st->inherit_flag &= ~rot;
	if (argv[i][0] == '+' && rot) st->inherit_flag |= rot;
    }

    if (st->inherit_flag & INHERIT_IN) printf("Pvs(in) will be inherited from nash by children\n");
    if (st->inherit_flag & INHERIT_OUT) printf("Pvs(out) will be inherited from nash by children\n");
    if (st->inherit_flag & INHERIT_ERR) printf("Pvs(err) will be inherited from nash by children\n");
    if (st->inherit_flag & INHERIT_FS) printf("Filing system information and cwd will be inherited from nash by children\n");

    return 0;
}
	

/* set _name to _val, regardless of any previous binding */
#define CX_SET(_cx, _name, _val, _type)		\
do {						\
    Type_Any _any;				\
						\
    if (Context$Get(_cx, _name, &_any))		\
	Context$Remove(_cx, _name);		\
						\
    CX_ADD_IN_CX(_cx, _name, _val, _type);	\
} while(0)
        
/*************************************************************/

int builtin_source(nash_st *st, int argc, char **argv) {
    FILE *f;
    char *newbuf;
    int nread;

    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    newbuf = malloc(1024);
    if (!newbuf) {
	printf("out of memory\n");
	return 1;
    }

    if (argc != 2) {
	printf("usage: source filename\n");
    }

    f = fopen(argv[1], "r");

    if (!f) {
	printf("script %s not found\n", argv[1]);
	return 1;
    }
#undef EOF
    while (!Rd$EOF(f->rd)) {
	nread = Rd$GetLine(f->rd, newbuf,  1024);
	newbuf[nread-1] = 0;
	if (argc == 3) {
	    if (!strcmp(argv[2], "echo")) {
		printf("\t%s\n", newbuf);
	    }
	}
	parse(st, newbuf);
    }
    fclose(f);
    free(newbuf);
    return 0;
}

/*************************************************************/

/* used by builtin_exec() to process argc and argv into a form
 * suitable for the new program */
static void build_env(nash_st *st, Context_clp cx, int argc, char **argv)
{
    Context_clp      proot;
    Context_clp      b_env;
    Type_Any 	     any;
    uint32_t 	     i, cnt;
    char           **index;

    /* XXX heap needs to be readable by child, probably also modifiable :( */
    if(!(index = Heap$Malloc(Pvs(heap), (argc + 1) * sizeof(char *))))
	return;

    cnt=0;
    for(i=0; i<argc; i++)
    {
	/* leave out any "&" we find along the way */
	if (strcmp(argv[i], "&"))
	    index[cnt++] = strdup(argv[i]);  /* XXX heap issues */
    }

    /* NULL-terminate the arg list */
    index[cnt] = NULL;

    /* create the b_env context if it's not there */
    if (Context$Get(cx, "b_env", &any)) {
	b_env = NARROW(&any, Context_clp);
    } else {
	b_env = CX_NEW_IN_CX(cx, "b_env");
    }

    /* create the priv_root cx if its not there */
    if (Context$Get(b_env, "priv_root", &any)) {
	proot = NARROW(&any, Context_clp);
    } else {
	proot = CX_NEW_IN_CX(b_env, "priv_root");
    }

    /* Set 'k' flag if necessary */
    if (st->really) {
	if (!Context$Get(cx,"qos",&any)) {
	    CX_NEW_IN_CX(cx,"qos");
	}
	if (!Context$Get(cx,"qos>cpu",&any)) {
	    CX_NEW_IN_CX(cx,"qos>cpu");
	}
	CX_SET(cx, "qos>cpu>k",st->really,bool_t);
    }

    /* We need to have IDCOffers in our hands for stdout and stderr in
       order to pass them down to our child. This is slightly dodgy;
       we don't want to re-export an IDC stub if we're using one. As a
       heuristic, if env>stdout exists then copy it to the new domain,
       otherwise export our Pvs(out). Likewise for stderr. */

    /* If we want to pass down our current working directories, we
       need to set these up explicitly in cx fs_cert. */

    /* XXX we don't want to pass down stdin; it's _ours_ and we're
       using it.  To provide stdin to children, we should really
       implement a Reader ourselves, and support switching. This it
       not hard, just tedious. */

    if (st->inherit_flag & INHERIT_OUT) {
	if (Context$Get(Pvs(root), "env>stdout", &any)) {
	    Context$Add(b_env, "stdout", &any);
	} else {
	    IDC_EXPORT_IN_CX(b_env, "stdout", Wr_clp, Pvs(out));
	}
    }

    if (st->inherit_flag & INHERIT_ERR) {
	if (Context$Get(Pvs(root), "env>stderr", &any)) {
	    Context$Add(b_env, "stderr", &any);
	} else {
	    IDC_EXPORT_IN_CX(b_env, "stderr", Wr_clp, Pvs(err));
	}
    }

    /* XXX no stdin */

    if (st->inherit_flag & INHERIT_FS) 
    {
	/* Build a fs_cert context in the new environment */
	Context_clp cert;
	StringTblIter_clp i;
	string_t fs, cwd=NULL;
	Type_Any any;

	cert=CX_NEW_IN_CX(b_env, "fs_cert");
	i=StringTbl$Iterate(st->fs_cwds);
	while (StringTblIter$Next(i, &fs, &cwd)) {
	    ANY_INIT(&any, string_t, cwd);
	    Context$Add(cert, fs, &any);
	}
	StringTblIter$Dispose(i);
    }

#if 0
    /* put gdb stuff in proot */
    if (st->gdbrd && st->gdbwr) {
	IDC_EXPORT_IN_CX(proot, "gdbrdoffer", Rd_clp, st->gdbrd);
	IDC_EXPORT_IN_CX(proot, "gdbwroffer", Wr_clp, st->gdbwr);
    } 
#endif /* 0 */

    {
      /* arguments */
      Context_clp    ctx;
      uint32_t       i;
      char           buf[32];

      ctx=CX_NEW_IN_CX(proot,"arguments");
      
      CX_SET(ctx, "argc", cnt, uint32_t);
      for (i=0; i<cnt; i++){
        sprintf (buf, "a%d", i);
        CX_SET(ctx, buf, index[i], string_t);
      }
    }

#if 0
    {
	/* environment */
	/* we only pass the bindings with strings to new domains */
	Context_clp    ctx;
	ContextMod_clp cmod;
	uint32_t       i;
	uint32_t       num;
	Context_Names *ctx_names;
	string_t       key;
	Type_Any       any;
	
	ctx_names = Context$List( st->env );
	num = SEQ_LEN(ctx_names);
	
	cmod  = NAME_FIND ("modules>ContextMod", ContextMod_clp);
	ctx = ContextMod$NewContext (cmod, Pvs(heap), Pvs(types));
	
	for (i=0; i < num; i++) {
	    key = SEQ_ELEM (ctx_names, i);
	    if (Context$Get (st->env, key, &any))
		if (ISTYPE(&any, string_t))
		    Context$Add (ctx, key, &any);
	}
	CX_SET (proot, "environ", ctx, Context_clp);
    }
#endif /* 0 */

#if 0
    CX_SET(proot, "argc", cnt, uint32_t);
    CX_SET(proot, "argv", index, addr_t);
#endif
}

/*************************************************************/

int builtin_exec(nash_st *st, int argc, char **argv) 
{
    FILE               *f;
    Rd_clp              rd;
    Type_Any            module;
    NOCLOBBER int j;

    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    f  = fopen((char*)(argv[0]), "r");
    
    if(f == NULL)
    {
	printf("nash: no such file or directory \"%s\".\n", argv[0]);
	return 0;
    }
    
    rd = f->rd;

    ANY_INIT(&module, Rd_clp, rd);
    FREE(f); /* XXX will work, but we're messing with stdio internals... */

    TRY {
	Type_Any any;
	char envnamebuf[128];
	Context_clp NOCLOBBER environment;

	/* We _assume_ (XXX) that any special environment for programs we
	   run has been set up in the penv>progname context, by running
	   bits of clanger. Nice for somebody to document this... */
	strcpy(envnamebuf, "penv>");
	strcat(envnamebuf, argv[0]);
	if (Context$Get(st->clangerroot, envnamebuf, &any) &&
	    ISTYPE(&any, Context_clp)) {
	    Context_clp env;
	    Context_clp NOCLOBBER newenv;

	    env = NARROW(&any, Context_clp);

	    TRC(printf("Using template environment %s\n",envnamebuf));
	    /* We copy this template environment; we'll add things to
	       the copy, and then free it later on. */
	    TRY {
		newenv = Context$Dup (env, Pvs(heap), NULL);
	    }
	    CATCH_ALL {
		printf("Got exception whilst duplicating env\n");
	    }
	    ENDTRY;
	    environment=newenv;
	} else {
	    TRC(printf("No template environment %s\n",envnamebuf));
	    environment=ContextMod$NewContext(st->cmod, Pvs(heap), Pvs(types));
	}

	build_env(st, environment, argc, argv);
	
#ifdef DEBUG
	CX_DUMP(environment,Context_clp);
#endif
	CExec$Run(NAME_FIND("modules>CExec", CExec_clp), argv[0], environment);
#if 0
	did = Exec$Run(st->exec, &module, environment, 
		       NULL_PDID /*VP$ProtDom(Pvs(vp))*/,
		       argv[0]);
#endif
	/* sprintf(buffer, "proc>domains>%qx>vp", did); XXX why? */

	/* remove arguments and environment variable context 
	 * to avoid memory leakage
	 */
	if (Context$Get(environment, "b_env>priv_root>arguments", &any)) {
	  TRY {
	    Context_Names *ctx_names;
	    Context_clp ctx;
	    uint32_t       num;
	    string_t       key;
	    NOCLOBBER uint32_t       i;
	    	    
	    /* Remove arguments from priv_root */
	    Context$Remove(environment, "b_env>priv_root>arguments");
	    ctx = NARROW(&any, Context_clp);

	    /* Free up its content */
	    ctx_names = Context$List( ctx );
	    num = SEQ_LEN(ctx_names);
	    
	    for (i=0; i < num; i++) {
	      key = SEQ_ELEM (ctx_names, i);
	      TRY {
		Context$Remove (ctx, key);
	      } CATCH_ALL {
		printf ("Error freing element %s from arguments context.\n");
	      } ENDTRY;
	    }    
	    /* Destroy context */
	    Context$Destroy (ctx);
	  } CATCH_ALL {
	    printf ("Error removing argument for %s.\n", argv[0]);
	  } ENDTRY;
	}
	/* Shouldn't exist */
	if (Context$Get(environment, "b_env>priv_root>environ", &any)) {
	  TRY {
	    Context_Names *ctx_names;
	    Context_clp ctx;
	    uint32_t       num;
	    string_t       key;
	    NOCLOBBER uint32_t       i;
	    
	    /* Remove environ from priv_root */
	    Context$Remove(environment, "b_env>priv_root>environ");
	    ctx = NARROW(&any, Context_clp);
	    
	    /* Free up its content */
	    ctx_names = Context$List( ctx );
	    num = SEQ_LEN(ctx_names);
	    
	    for (i=0; i < num; i++) {
	      key = SEQ_ELEM (ctx_names, i);
	      TRY {
		Context$Remove (ctx, key);
	      } CATCH_ALL {
		printf ("Error freing element %s from environ context.\n");
	      } ENDTRY;
	    }    
	    /* Destroy context */
	    Context$Destroy (ctx);
	  } CATCH_ALL {
	    printf ("Error removing environ for %s.\n", argv[0]);
	  } ENDTRY;
	}
    } CATCH_Load$Failure(why) {
	printf("%s: cannot execute (%d)\n", argv[0], why);
	j = -1;
    } ENDTRY;


    return j;
}

/*************************************************************/

int builtin_clanger(nash_st *st, int argc, char **argv) {
    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    printf("builtin_clanger executed by error\n");
    /* this should never be called, as clanger is a special case 
       handled in parse()
       */
    return 1;
}

/*************************************************************/

int builtin_whoami(nash_st *st, int argc, char **argv) 
{
    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    printf("%s\n", st->really ? "somebody":"nobody");
    return 0;
}

/*************************************************************/

int builtin_boot(nash_st *st, int argc, char **argv) 
{
    string_t NOCLOBBER image;
    Rd_clp             rd;
    FileIO_clp         fio;
    Type_Any           any;
    string_t           mem;
    int                i;
    char               cmdline[1024];

    extern void boot(Rd_cl *, bool_t, string_t);     

    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    image=st->hostname;

    if (argc > 1)
	image=argv[1];
    else
	image="image";

    printf("Loading %s\n",image);

    if(FSDir$Lookup(st->pwd, image, True) != FSTypes_RC_OK)
    {
	fprintf(stderr, 
		"nash bootloader: no such file or directory \"%s\".\n",
		image);
	return -1;
    }
    if(FSDir$Open(st->pwd, 0, FSTypes_Mode_Read, 0, &fio) != FSTypes_RC_OK)
    {
	fprintf(stderr, 
		"nash bootloader: failed to open \"%s\".\n",
		image);
	return -1;
    }

    rd = FSUtil$GetRd(st->fsutils,
		      fio,
		      Pvs(heap),
		      True);

    /* If there are any extra arguments, tack them on the end of the
       command line. */
    mem=NULL;
    if (Context$Get(Pvs(root),"proc>cmdline>mem",&any)) {
	mem=NARROW(&any, string_t);
    }

    sprintf(cmdline,"BOOT_IMAGE=%s",image);
    if (mem) {
	strcat(cmdline," mem=");
	strcat(cmdline,mem);
    }

    for (i=2; i<argc; i++) {
	strcat(cmdline," ");
	strcat(cmdline,argv[i]);
    }
    printf("(using command line %s)\n",cmdline);

    boot(rd, True,cmdline);

    /* :-) */

    Rd$Close(rd);

    return 0;
}    


/*************************************************************/

int builtin_reboot(nash_st *st, int argc, char **argv) 
{
    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    ntsc_reboot();
    return 0;     /* keep gcc happy ;-) */
}

/*************************************************************/

int builtin_debug(nash_st *st, int argc, char **argv) 
{
    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    ntsc_dbgstop();
    return 0;     /* keep gcc happy ;-) */
}

/*************************************************************/

static int cdfs(nash_st *st, string_t fs)
{
    Type_Any any,dummy;
    string_t path;
    char buff[80];

    if (!st->fsctx || !st->fs_cwds) return 1;
    if (!StringTbl$Get(st->fs_cwds, fs, &path)) {
	fprintf(stderr,"nash: cdfs: %s not mounted\n",fs);
	return 1;
    }

    sprintf(buff,"cwd_%s",fs);
    if (!Context$Get(st->fsctx,buff,&any)) {
	fprintf(stderr,"nash: cdfs: bogosity: %s doesn't have a cwd\n",fs);
	fprintf(stderr,"(bogosity: (n) (slang); the degree of unexpectedness and brokenness of some object or event)\n");
	return 1;
    }
    if (st->current_fs) FREE(st->current_fs);
    st->current_fs=strdup(fs);
    if (Context$Get(st->fsctx,"cwd",&dummy)) {
	Context$Remove(st->fsctx, "cwd");
    }
    Context$Add(st->fsctx, "cwd", &any);
    st->pwd=NARROW(&any, FSDir_clp);
    st->cwdname=path;
    return 0;
}


int builtin_cdfs(nash_st *st, int argc, char **argv)
{
    if (argc!=2) {
	fprintf(stderr,"Usage: cdfs fs\n");
	return 1;
    }
    return cdfs(st,argv[1]);
}

/*************************************************************/

static int mount_namedfs(nash_st *st, string_t fs)
{
    NOCLOBBER int result=0;
    TRY {
	string_t cwd;
	char buff[80];
	Type_Any any;
	Type_Any *anyp;
	FSDir_clp fsd;
	FSClient_clp fsc;
	FSTypes_RC rc;
	string_t cert;
	TRC(printf("Mounting fs %s\n",fs));

	if (StringTbl$Delete(st->fs_cwds, fs, &cwd)) {
	    FREE(cwd);
	}
	/* We delete the FSDir if we already have one; after the mount
	   we should be back in the default directory for that fs. */
	sprintf(buff, "cwd_%s",fs);
	if (Context$Get(st->fsctx, buff, &any)) {
	    fsd=NARROW(&any, FSDir_clp);
	    Context$Remove(st->fsctx, buff);
	    FSDir$Close(fsd);
	}

	sprintf(buff, "client_%s",fs);
	if (Context$Get(st->fsctx, buff, &any)) {
	    fsc=NARROW(&any, FSClient_clp);
	} else {
	    fsc=IDC_OPEN_IN_CX(st->mounts, fs, FSClient_clp);
	    ANY_INIT(&any, FSClient_clp, fsc);
	    Context$Add(st->fsctx, buff, &any);
	}

	sprintf(buff, "env>fs_cert>%s", fs);
	if (Context$Get(Pvs(root), buff, &any)) {
	    cert=NARROW(&any, string_t);
	} else {
	    cert="/";
	}

	rc=FSClient$GetDir(fsc, "", True, &anyp);
	/* XXX check rc */
	fsd=NARROW(anyp,FSDir_clp);
	FREE(anyp);

	if (cert) {
	    FSDir$Lookup(fsd,cert,True);
	    FSDir$CWD(fsd);
	}

	StringTbl$Put(st->fs_cwds, fs, strcpy(Heap$Malloc(Pvs(heap), 256),
					      cert));

	sprintf(buff, "cwd_%s",fs);
	ANY_INIT(&any, FSDir_clp, fsd);
	Context$Add(st->fsctx, buff, &any);

    } CATCH_ALL {
	printf("mountfs: failed on %s (caught exception %s)\n", 
	       fs, exn_ctx.cur_exception);
	result=1;
    } ENDTRY;

    return result;
}

int builtin_mountfs (nash_st *st, int argc, char **argv) 
{
    FTRC("entered, argc=%d, argv=%p.\n", argc, argv);

    if (!st) {
	fprintf(stderr,"No state record!\n");
	return -1;
    }

    if (argc==2 && st->fs_cwds) {
	return mount_namedfs(st, argv[1]);
    }

    if (st->fs_cwds) {
	StringTblIter_clp i;
	string_t key, val;
	/* We've already enumerated our filesystems; list them nicely
	   for the user. */
	i=StringTbl$Iterate(st->fs_cwds);
	while (StringTblIter$Next(i, &key, &val)) {
	    printf("%-20s %s\n", key, val);
	}
	StringTblIter$Dispose(i);
	    
	return 0;
    }

    /* Check if everything has been sorted out */
    TRC(printf("Discovering filing systems\n"));
    TRY {
	Type_Any any;
	Context_Names *mounted_fs;
	int i;

	TRC(printf("Finding mounts\n"));
	st->mounts = NAME_FIND("mounts", Context_clp);
	TRC(printf("Getting fs context\n"));
	TRC(printf("Finding fs\n"));
	if (Context$Get(Pvs(root),"fs",&any)) {
	    st->fsctx = NARROW(&any, Context_clp);
	} else {
	    st->fsctx=CX_NEW("fs");
	}

	TRC(printf("Listing mounted filesystems...\n"));
	mounted_fs=Context$List(st->mounts);

	TRC(printf("Creating filesystem table\n"));
	st->fs_cwds=StringTblMod$New(NAME_FIND("modules>StringTblMod",
					       StringTblMod_clp), Pvs(heap));
	
	for (i=0; i<SEQ_LEN(mounted_fs); i++)
	    mount_namedfs(st, SEQ_ELEM(mounted_fs, i));

	cdfs(st, "init");

    } CATCH_Context$NotFound(name) {
	printf("mountfs: lookup of %s failed.\n", name);
	st->fsctx   = (Context_clp)NULL;
	st->pwd     = (FSDir_clp )NULL;
	st->fs_cwds = NULL;
	return -1;
    } CATCH_ALL {
	st->fsctx   = (Context_clp)NULL;
	st->pwd     = (FSDir_clp )NULL;
	st->fs_cwds = NULL;
	printf("mountfs: failed (caught exception %s)\n", 
	       exn_ctx.cur_exception);
	return -1;
    }
    ENDTRY;

    return 0;
}
