/*
 *	undef.c
 *	-------
 *
 * $Id: undefarm.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "arm.a.out.h"

extern char		*optarg;
extern int		optind, opterr;

/* ----------------------------- */

char		*progname;
struct exec	exec;

static void header(int fd)
{
    int		rc;
    
    lseek(fd, 0, 0);
    if ((rc = read(fd, &exec, sizeof(exec))) != sizeof(exec))
    {
	perror("undef: read");
	exit(2);
    }
    lseek(fd, 0, 0);
}

static void usage(void)
{
    fprintf(stderr, "usage: undef [-q] [-l] [-u] [-c] [-d] [-b] [-w] name\n");
    fprintf(stderr, "       -q for quiet output - return code only\n");
    fprintf(stderr, "       -l to ignore undefined linker generated symbols (e.g. _end)\n");
    fprintf(stderr, "       -c to ignore common symbols not yet allocted an address\n");
    fprintf(stderr, "       -u to ignore undefined symbols\n");
    fprintf(stderr, "       -d to delete offending binaries (useful in makefiles)\n");
    fprintf(stderr, "       -b to complain about a bss symbol (useful for shared libraries)\n");
    fprintf(stderr, "       -w to warn about problems (needed for Nemesis which has too many errors)\n");
    exit(2);
}

static void error(char *s)
{
    fprintf(stderr, "undef: %s\n", s);
    exit(2);
}

static int verbose = 1;
static int ldsyms_matter = 1;
static int commsyms_matter = 1;
static int undefs_matter = 1;
static int warnonly = 0;
static int failbss = 0;

static int onefile(char *name)
{
    struct stat		stat;
    int			rc;
    int			in;
    unsigned char	*buf;
    struct nlist	*nl;
    int			i;
    int			ok = 1;

    /* ------------- */

    if ((in = open(name, O_RDONLY)) < 0)
    {
	perror("undef: open");
	exit(2);
    }

    header(in);
    
    if (N_BADMAG(exec))
    {
	if (exec.a_magic == 0x72613c21)
	{
	    fprintf(stderr, "undef: caution: ignoring archive %s\n", name);
	    return ok;
	}
	fprintf(stderr, "undef: %s has bad magic number\n", name);
	exit(2);
    }
    
    /* ------------- */

    if ((rc = fstat(in, &stat)) != 0)
    {
	perror("undef: fstat");
	exit(2);
    }
    
    if ((buf = malloc(stat.st_size)) == 0)
    {
	fprintf(stderr, "undef: malloc: failed\n");
	exit(2);
    }
    
    if ((rc = read(in, buf, stat.st_size)) != stat.st_size)
    {
	perror("undef: read");
	exit(2);
    }
    
    if (close(in) < 0)
    {
	perror("undef: close");
	exit(2);
    }

    nl = (struct nlist *)(buf + N_SYMOFF(exec));
    
    for (ok=1, i= exec.a_syms; i; i-= sizeof(struct nlist), nl++)
    {
	char		*barf;
	int		type;
	
	if (nl->n_type & N_STAB)
	    continue;

	/* Wierd COMM symbol not in N_COMM. Why, oh why? */

	type = nl->n_type & N_TYPE;
	if (type == N_UNDF && nl->n_value)
	    type = N_COMM;
	
	switch(type)
	{
	case N_COMM:
	    if (!commsyms_matter)
		continue;
	    break;
	    
	case N_UNDF:
	    if (!undefs_matter)
		continue;
	    break;
	    
	case N_BSS:
	    if (!failbss)
		continue;
	    break;
	    
	default:
	    continue;
	}
	
	barf = buf + N_STROFF(exec) + nl->n_un.n_strx;
	    
	if (!ldsyms_matter)
	{
	    if (!strcmp(barf, "__DYNAMIC") || !strcmp(barf, "_edata") ||
		!strcmp(barf, "__bss_start") || !strcmp(barf, "_end") ||
		!strcmp(barf, "__end"))
		continue;
	}
	
	/* Here then we are unhappy */

	if (verbose)
	{
	    printf("undef: file %s: symbol %s\n", name, barf);
	    ok = 0;
	}
	else
	{
	    ok = 0;
	    break;
	}
    }
    
    free(buf);

    return ok;
}

int main(int argc, char **argv)
{
    int		c;
    int		ok = 1;
    int		delete = 0;
    
    while ((c = getopt(argc, argv, "lqcudbw")) != EOF)
    {
	switch (c)
	{
	case 'q':
	    verbose = 0;
	    break;

	case 'd':
	    delete = 1;
	    break;

	case 'l':
	    ldsyms_matter = 0;
	    break;
	    
	case 'c':
	    commsyms_matter = 0;
	    break;
	    
	case 'u':
	    undefs_matter = 0;
	    break;
	    
	case 'b':
	    failbss = 1;
	    break;
	    
	case 'w':
	    warnonly = 1;
	    break;
	    
	default:
	    usage();
	}
    }

    if (optind == argc)
    {
	ok = onefile("a.out");
	if (!ok && delete)
	    unlink("a.out");
    }
    else
    {
	while (optind < argc)
	{
	    int		tmp = onefile(argv[optind]);
	    ok &= tmp;
	    if (!tmp && delete)
		unlink(argv[optind]);
	    optind++;
	}
    }
    
    if (warnonly)
    {
	if (!ok)
	{
	    fprintf(stderr, "********************************************\n");
	    fprintf(stderr, "* Illegal BSS / Common / Undefined Symbols *\n");
	    fprintf(stderr, "* System contains bugs and may be faulty   *\n");
	    fprintf(stderr, "********************************************\n");
	}
	ok = 1;
    }

    exit( ok ? 0 : 1 );
}

