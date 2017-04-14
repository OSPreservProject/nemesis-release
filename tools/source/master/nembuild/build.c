/*
 *	build.c
 *	-------
 *
 * $Id: build.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory
 * Copyright (c) 1996 Richard Black
 * Right to use is administered by University of Cambridge Computer Laboratory
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

/* On some systems this is defined by ansidecl.h which is included by bfd.h
 * On some its not. Wierd */
#if defined(__linux) && !defined(linux_for_arm)
#  define PARAMS(x) x
#endif

#include <bfd.h>   /* needs PARAMS */

#define FREE(x) free(x)

#include "descrip.h"
#include "build.h"


#define BOOTFLAG_X 0x01
#define BOOTFLAG_K 0x02
#define BOOTFLAG_B 0x04

/* ---------------------------------------------------------------------- */

static char *prog_name;
static char *ident="";
int dflg = 0;
int blowruns = 0;
static int num_bflags_seen = 0;
static char *outname = "a.out";
static int verbose = 0;
static int Primal = -1;

void non_fatal(char *format, ...)
{
    va_list		ap;

    va_start(ap, format);
    fprintf(stderr,"%s: ", prog_name);
    vfprintf(stderr, format, ap);
    if (!(format[strlen(format)-1] == '\n')) fputc('\n', stderr);
    fflush(stderr);
    va_end(ap);
}

#ifdef __GNUC__
void fatal(char *format, ...) __attribute__((noreturn));
#endif /* __GNUC__ */

void fatal(char *format, ...)
{
    va_list		ap;

    va_start(ap, format);
    fprintf(stderr, "%s: ", prog_name);
    vfprintf(stderr, format, ap);
    if (!(format[strlen(format)-1] == '\n')) fputc('\n', stderr);
    fflush(stderr);
    va_end(ap);

    exit(1);
}

/* ---------------------------------------------------------------------- */

char *parser_input_file_name;
char *depend_output_file_name;
FILE *depend_output_file;
FILE *gdbfile = NULL;
extern int yylineno;
static int error_occured = 0;

size_t blobbase = 0;
size_t bloblen = 0;

void warning(char *format, ...)
{
    va_list		ap;

    va_start(ap, format);
    fprintf(stderr, "%s:%d: warning: ", parser_input_file_name, yylineno);
    vfprintf(stderr, format, ap);
    if (!(format[strlen(format)-1] == '\n')) fputc('\n', stderr);
    fflush(stderr);
    va_end(ap);
}

void error(char *format, ...)
{
    va_list		ap;

    va_start(ap, format);
    fprintf(stderr, "%s:%d: error: ", parser_input_file_name, yylineno);
    vfprintf(stderr, format, ap);
    if (!(format[strlen(format)-1] == '\n')) fputc('\n', stderr);
    fflush(stderr);
    va_end(ap);

    error_occured = 1;
}

/* ---------------------------------------------------------------------- */

static void per_section(bfd *abfd, sec_ptr sec, void *v)
{
    printf("sec: %-10s size %6lu flags 0x%04x start %#lx\n",
	   bfd_get_section_name(abfd, sec),
	   bfd_get_section_size(sec),
	   bfd_get_section_flags(abfd, sec),
	   bfd_get_section_vma(abfd, sec));
}

static void per_symbol(asymbol *s)
{
    printf("sym: %-20s flags 0x%04x value %#lx\n",
	   bfd_asymbol_name(s),
	   bfd_asymbol_flags(s),
	   bfd_asymbol_value(s));
}

int str2flags(const char *str)
{
    const char *p = str;
    int flags = 0;

    if (!p)
    {
	non_fatal("internal error: str2flags() passed NULL pointer\n");
	return 0;
    }

    while(*p)
    {
	switch(*p)
	{
	case 'x':
	    flags |= BOOTFLAG_X;
	    break;
	
	case 'k':
	    flags |= BOOTFLAG_K;
	    break;

	case 'b':
	    flags |= BOOTFLAG_B;
	    num_bflags_seen++;
	    break;

	default:
	    warning("unrecognized boot flag `%c'\n", *p);
	    break;
	}
	p++;
    }

    return flags;
}	    

static void all_symbols(bfd *abfd)
{
    long	number_of_symbols;
    long	storage_needed;
    asymbol	**symbol_table;
    long	i;
    
    if ((storage_needed = bfd_get_symtab_upper_bound(abfd)) < 0)
	fatal("bfd_get_symtab_upper_bound: %s", bfd_strerror);
    fprintf(stderr,"Need %ld for symbol table\n", storage_needed);
    symbol_table = xmalloc(storage_needed);
    
    if ((number_of_symbols = bfd_canonicalize_symtab(abfd, symbol_table)) < 0)
	fatal("bfd_canonicalize_symtab: %s\n", bfd_strerror);
    
    for(i=0; i<number_of_symbols; i++)
	per_symbol(symbol_table[i]);
}

static int blob1(char *foo)
{
    bfd		*abfd;
    
    if (!(abfd = bfd_openr(foo, NULL)))
	fatal("bfd_openr: %s: %s\n", foo, bfd_strerror);
    
    if (!bfd_check_format(abfd, bfd_object))
	fatal("bfd_check_format: %s: %s\n", foo, bfd_strerror);
    
    printf("target of %s is %s\n", foo, bfd_get_target(abfd));
    
    bfd_map_over_sections(abfd, per_section, NULL);

    all_symbols(abfd);
    return 0;
}

#if 0
static void blob2()
{
    /* XXX maybe this will be useful some day. At the moment we dont
     * use it because we prefer to get a .run file that can be used by
     * gdb */
    bfd_get_relocated_section_contents
}
#endif /* 0 */

/* ------------------------------------------------------------ */

/* ===================================================================== */
/* This section deals with the platform specific sections of
 * code. Platform should be specified on the command line using the
 * '-p' switch. When the first binary file is come across (the ntsc)
 * the bfd_get_target is used to sanity check which set of machine
 * specific data items and functions are used.
 */

/* XXX to be cleaned up to header files and machine specific files
 * (e.g. for page table code?) */


/* at the moment this doesnt need to be #ifdefd out, but some day it
 * may have platform specific code in it which may in turn be
 * sizeof(long) dependant */
#ifdef __i386__
struct machine intel_machine = {
    /* The Intel image will always end up located at 0x100000; either
     * loaded there initially, or uncompressed from below 640K */
    "elf32-i386", "_start", 12, 0x100000, 0, 0
};
#endif /* __i386__ */
#ifdef __alpha__
struct machine eb64_machine = {
    "ecoff-littlealpha", "_start", 13, 0x220000, 0x200000, 1, 0
};
struct machine axp3000_machine = {
    "ecoff-littlealpha", "_start", 13, 0x210000, 0x200000, 1, 0
};
struct machine eb164_machine = {
    "ecoff-littlealpha", "_start", 13, 0x210000, 0x200000, 1, 0
};
#endif /* __alpha__ */

#if defined(hpux) || defined(linux_for_arm)
struct machine arm8000_machine = {
    "coff-arm-little", "_start", 12, 0x8000, 0x8000, 0, 16
};
struct machine arm0_machine = {
    "coff-arm-little", "_start", 12, 0x0, 0x0, 0, 16
};
struct machine shark_machine = {
    "coff-arm-little", "_start", 12, 0xf0000000, 0xf0000000, 1, 16
};
#endif
#if defined(__NetBSD__)
struct machine armf0000000_machine = {
    "a.out-arm-netbsd", "_start", 12, 0xf0000000, 0xf0000000, 1, 0
};
#endif

const struct machine *machine = NULL;

/* Mapping from a pl_name given on the command line to a struct machine */
struct platform {
    const char *pl_name;
    struct machine *pl_info;
} platforms[] = {
#ifdef __i386__
    { "intel", &intel_machine },
#endif /* __i386__ */
#ifdef __alpha__
    { "axp3000", &axp3000_machine },
    { "eb64",    &eb64_machine },
    { "eb164",   &eb164_machine },
#endif /* __alpha__ */
#if defined(hpux) || defined(linux_for_arm)
    { "fpc3",    &arm8000_machine },
    { "riscpc",  &arm8000_machine },
    { "srcit",   &arm0_machine },
    { "shark",   &shark_machine },
#endif
#if defined(__NetBSD__)
    { "shark",   &armf0000000_machine },
#endif
    { NULL, NULL }
};

const struct platform *platform = NULL;

/* we've encountered an NTSC with bfd-target 'name': check for sanity */
static void check_machine(const char *name)
{
    if (strcmp(machine->mc_bfd_target, name))
	non_fatal("warning: platform %s's NTSC should have "
		  "bfd_target of `%s': but found `%s'\n",
		  platform->pl_name, platform->pl_info->mc_bfd_target, name);
}

/* ===================================================================== */

/* This is really nexus.h */

enum nexus_info {
    nexus_primal,
    nexus_ntsc,
    nexus_nexus,
    nexus_module,
    nexus_namespace,
    nexus_program,
    nexus_EOI,           /* End Of Image record */
    nexus_blob
};

struct nexus_primal {
    enum nexus_info	tag;
    size_t		entry;         /* address to jump to to enter Primal */
    size_t		namespace;     /* address of Primal's namespace */
    size_t		firstaddr;     /* address of first page of the image */
    size_t		lastaddr;      /* address of first free page after
					* Nemesis image. Same as nexus_EOI
					* record. */
};
struct nexus_ntsc {
    enum nexus_info	tag;
    size_t		taddr, daddr, baddr;
    size_t		tsize, dsize, bsize;
};
struct nexus_nexus {
    enum nexus_info	tag;
    size_t		addr;
    size_t		size;
};
struct nexus_module {
    enum nexus_info	tag;
    size_t		addr;
    size_t		size;
    int			refs;
};
struct nexus_name {
    enum nexus_info	tag;
    size_t		naddr;
    size_t		nsize;
    int			refs;
    int			nmods;
    struct nexus_module	*mods[0];		/* really [nmods] */
};
#define MAX_PROGNAME_LEN 32
struct nexus_prog {
    enum nexus_info	tag;
    size_t		taddr, daddr, baddr;
    size_t		tsize, dsize, bsize;
	char   program_name[MAX_PROGNAME_LEN]; /* XXX hack */
    struct nexus_name	*name;
    struct params	params;
};
struct nexus_EOI {
    enum nexus_info	tag;
    size_t 		lastaddr;  /* first free page after image */
    char		image_ident[80];
};
struct nexus_blob {
    enum nexus_info	tag;
    size_t 		base;	   /* base of the blob section */
    size_t		len;	   /* length of it */
};

typedef union {
    enum nexus_info	*tag;
    struct nexus_primal *nu_primal;
    struct nexus_ntsc	*nu_ntsc;
    struct nexus_nexus	*nu_nexus;
    struct nexus_module	*nu_mod;
    struct nexus_name	*nu_name;
    struct nexus_prog	*nu_prog;
    struct nexus_EOI	*nu_EOI;
    struct nexus_blob	*nu_blob;
    void		*generic;		/* for general movement */
} nexus_ptr_t;

static size_t write_out_nexus(void *base, size_t address,
			      struct ntsc *ntsc, size_t lastaddr)
{
    int			i, j, k, l;
    nexus_ptr_t		n;
    struct module	*mod;
    struct namelist	*names;
    struct prog		*prog;
    struct nexus_module	**p, *m;
    struct nexus_nexus	*us;
    
    n = (nexus_ptr_t)(base + address);

    n.nu_primal->tag = TE32(nexus_primal);
    n.nu_primal->entry = TEsize(global_proglist->bpl_list[Primal].bp.taddr);
    n.nu_primal->namespace =
	TEaddr(global_proglist->bpl_list[Primal].bp_names->bnl_addr);
    n.nu_primal->firstaddr = TEsize(machine->mc_bootaddr);
    n.nu_primal->lastaddr = TEsize(lastaddr);
    n.nu_primal++;

    n.nu_ntsc->tag = TEsize(nexus_ntsc);
    n.nu_ntsc->taddr = TEsize(ntsc->bk.taddr);
    n.nu_ntsc->daddr = TEsize(ntsc->bk.daddr);
    n.nu_ntsc->baddr = TEsize(ntsc->bk.baddr);
    n.nu_ntsc->tsize = TEsize(ntsc->bk.tsize);
    n.nu_ntsc->dsize = TEsize(ntsc->bk.dsize);
    n.nu_ntsc->bsize = TEsize(ntsc->bk.bsize);
    n.nu_ntsc++;
    
    us = n.nu_nexus;  /* for filling in size later */
    n.nu_nexus->tag = TE32(nexus_nexus);
    n.nu_nexus->addr = TEsize(address);
    /* n.nu_nexus->size = later */
    n.nu_nexus++;
    
    for(i=0; i<global_modlist->bml_num; i++)
    {
	mod = &global_modlist->bml_list[i];
	
	n.nu_mod->tag  = TE32(nexus_module);
	n.nu_mod->addr = TEsize(mod->bm.taddr);
	n.nu_mod->size = TEsize(mod->bm.tsize + mod->bm.dsize);
	n.nu_mod->refs = TE32(mod->bm_refs);
	mod->bm_nexus  = n.generic-base; /* XXX void pointer arithmetic... */
	n.nu_mod++;
    }

    /* write a blob record */
    if (bloblen)
    {
	n.nu_blob->tag  = TE32(nexus_blob);
	n.nu_blob->base = TEsize(blobbase);
	n.nu_blob->len  = TEsize(bloblen);
	n.nu_blob++;
    }

    for(i=0; global_namespaces && i<global_namespaces->bnnl_num; i++)
    {
	names = &global_namespaces->bnnl_list[i];

	n.nu_name->tag   = TE32(nexus_namespace);
	n.nu_name->naddr = TEsize(names->bnl_addr);
	n.nu_name->nsize = TEsize(names->bnl_space);
	n.nu_name->refs  = TE32(names->bnl_refs);
	names->bnl_nexus = n.generic-base; /* XXX void pointers */
	p = &n.nu_name->mods[0];
	/* Now for a nice n squared algorithm */
	break_and_continue:
	for(k=j=0; j<names->bnl_num;j++)
	{
	    if (names->bnl_list[j].bn_type == NAME_TYPE_SYMBOL)
	    {
		m = names->bnl_list[j].bn_modptr->bm_nexus;

		m = TEaddr(m); /*caution: target endian conversion done early*/

		for(l=0; l<k; l++)
		    if (p[l] == m)
			goto break_and_continue;
	    
		p[k++] = m;
	    }
	}
	
	n.nu_name->nmods = TE32(k);
	n.nu_name ++;
	n.generic += k * sizeof(void *);
    }
    
    for(i=0; i<global_proglist->bpl_num; i++)
    {
	prog = &global_proglist->bpl_list[i];
	
	n.nu_prog->tag   = TE32(nexus_program);
	n.nu_prog->taddr = TEsize(prog->bp.taddr);
	n.nu_prog->daddr = TEsize(prog->bp.daddr);
	n.nu_prog->baddr = TEsize(prog->bp.baddr);
	n.nu_prog->tsize = TEsize(prog->bp.tsize);
	n.nu_prog->dsize = TEsize(prog->bp.dsize);
	n.nu_prog->bsize = TEsize(prog->bp.bsize);
	n.nu_prog->name  = TEaddr(prog->bp_names->bnl_nexus);
/* Marshal the structure manually: */
/* XXX assumes the structures will be laid out the same! */
/*	n.nu_prog->params = prog->bp_params;*/
	n.nu_prog->params.p = TE64(prog->bp_params.p);
	n.nu_prog->params.s = TE64(prog->bp_params.s);
	n.nu_prog->params.l = TE64(prog->bp_params.l);
	n.nu_prog->params.astr = TElong(prog->bp_params.astr);
	n.nu_prog->params.stack = TElong(prog->bp_params.stack);
	n.nu_prog->params.heap  = TElong(prog->bp_params.heap);
	n.nu_prog->params.nctxts= TE32(prog->bp_params.nctxts);
	n.nu_prog->params.neps  = TE32(prog->bp_params.neps);
	n.nu_prog->params.flags = TE32(prog->bp_params.flags);
	n.nu_prog->params.nframes = TE32(prog->bp_params.nframes);
	
	if(prog->name)
		strncpy(n.nu_prog->program_name, prog->name, MAX_PROGNAME_LEN);
	else fprintf(stderr, "Oops: program name seems to be NULL.\n");
	prog->bp_nexus = n.nu_prog;
	n.nu_prog++;
    }

    /* add nexus record for the final size of the image */
    n.nu_EOI->tag      = TE32(nexus_EOI);
    n.nu_EOI->lastaddr = TE32(lastaddr);
    strncpy(n.nu_EOI->image_ident, ident, 79);
    n.nu_EOI->image_ident[79]=0;
    n.nu_EOI++;

    us->size = TEsize(n.generic - (base + address));
    
    return address + TEsize(us->size);
}

/* ===================================================================== */
/* This section contains the data structures and associated functions
 * called from the parser which builds up the tables of things that
 * need to be done. This permits us to generate error and warnings as
 * we go, and also to store things in a more useful internal format
 * (such as indexes into tables rather than ascii strings).
 */

struct modlist		*global_modlist;

struct bloblist		*global_bloblist;

struct proglist		*global_proglist;

struct nameslist	*global_namespaces;

#define LISTINIT(list)				\
do {						\
    if (!(list))				\
    {						\
	(list) = xmalloc(sizeof(*(list)));	\
	memset((list), 0, sizeof(*(list)));	\
    }						\
} while(0)

#define LISTADD(list, prefix, item)					     \
do {									     \
    (list)->prefix##_num++;						     \
    (list)->prefix##_list = xrealloc((list)->prefix##_list,		     \
				     ((list)->prefix##_num) * sizeof(item)); \
    memcpy(&((list)->prefix##_list[((list)->prefix##_num)-1]),		     \
	   &item, sizeof(item));					     \
} while(0)

/* ------------------------------------------------------------ */

struct namelist *parse_name(struct namelist *list, struct simplename *s)
{
    struct name		n, *p;
    int			i;
    
    LISTINIT(list);
    
    /* Basic strategy is to convert s to a struct name doing any
     * sanity checking at the time, followed by adding the new name to
     * the namelist list. */

    memset(&n, 0, sizeof(struct name));
    n.bn_path = s->path;
    n.bn_modname = s->mod;
    n.bn_symbol = s->symbol ? s->symbol : machine->mc_defsymname;
    n.bn_type = s->type;
    FREE(s);

    /* Check that the path being defined hasn't been used already */
    for(i=0; i<list->bnl_num; i++)
    {
	p = &list->bnl_list[i];
	if (!strcmp(n.bn_path, p->bn_path))
	{
	    error("namespace path \"%s\" redeclared.\n", p->bn_path);
	    return list;
	}
    }

    if (n.bn_type == NAME_TYPE_SYMBOL)
    {
	struct module	*m;

	/* Must check that the module referred to actually exists. Then
	 * that it has such a symbol name */
	if (!(m = find_module_by_name(n.bn_modname)))
	{
	    error("module `%s' not defined\n", n.bn_modname);
	    return list;
	}

	if (!(check_module_for_symbol(m, n.bn_symbol)))
	{
	    error("module `%s' does not define symbol `%s'\n", n.bn_modname,
		  n.bn_symbol);
	    return list;
	}
    
	n.bn_modptr = m;

	/* Looks OK at this stage - bump reference count for module and
	 * update string storage space requirements in namelist */
	m->bm_refs ++;
    }
    else
    {
	struct blob		*b;

	/* check the blob exists */
	if (!(b = find_blob_by_name(n.bn_modname)))
	{
	    error("blob `%s' not defined\n", n.bn_modname);
	    return list;
	}

	b->bb_refs ++;
    }
	
    list->bnl_space += strlen(n.bn_path) + 1;

    /* Here then this is OK */
    LISTADD(list, bnl, n);

    return list;
}

/* ------------------------------------------------------------ */

struct ntsc *parse_ntsc(char *name)
{
    bfd			*bfd;
    asection		*sec;
    bfd_vma		 ntscbase;
    struct ntsc		*n;

    bfd = open_bfd_read(name);
    if (!bfd)
    {
	FREE(name);
	return NULL;
    }
    
    check_machine(bfd_get_target(bfd));
    sec = bfd_get_section_by_name(bfd, ".text");
    if (!sec)
	fatal("bfd_get_section_by_name(NTSC, \".text\") returned NULL\n");
    ntscbase = bfd_get_section_vma(bfd, sec);
    /* XXX Bletch -- netbsd is broken, the a.out format has no way of
     * expresing a segment base address. Must just assume that the
     * Makefile in the ntsc directory did the same as what we have in
     * our database! */
    if (!strcmp(machine->mc_bfd_target, "a.out-arm-netbsd"))
    {
	ntscbase = machine->mc_baseaddr;
	non_fatal("warning: netbsd is broken: assuming vma %#x\n", ntscbase);
    }
    else if (ntscbase != machine->mc_baseaddr)
    {
	non_fatal("warning: NTSC text segment starts at %#x, was expecting "
		  "%#x\n",
		  ntscbase, machine->mc_baseaddr);
    }

    TRC(("Need %x for ntsc struct\n",sizeof(struct ntsc)));
    n = xmalloc(sizeof(struct ntsc));
    n->bk_binary = name;
    n->bk_bfd = bfd;

    bfd_map_over_sections(bfd, sec_fill_in_sizes, &n->bk.tsize);
    /* XXX Perform the same hack */
    if (!strcmp(machine->mc_bfd_target, "a.out-arm-netbsd"))
    {
	n->bk.taddr += ntscbase;
	n->bk.daddr += ntscbase;
	n->bk.baddr += ntscbase;
    }
	
    return n;
}

/* ------------------------------------------------------------ */
/* This function is called by the parser when it detects a module
 * command to build the initial information about the module. */

struct modlist *parse_module(struct modlist *list, struct simplemod *s)
{
    int			i;
    struct module	t, *p;

    LISTINIT(list);

    /* Make up a temporary struct mod which will be added to the list
     * if all is well */
    
    memset(&t, 0, sizeof(t));
    t.bm_name = s->name;
    t.bm_binary = s->path;
    FREE(s);
    
    /* We perform a preliminary scan over the modules listed
     * already. In this pass we detect when a module name has been
     * used already (an error) and a case where a second instance of
     * some module should be loaded (e.g. for breakpoints in one and
     * not t'other) and copy appropraite information from that one. */
    for(i=0; i<list->bml_num; i++)
    {
	p = &list->bml_list[i];
	if (!strcmp(t.bm_name, p->bm_name))
	{
	    error("module identifier `%s' reused.\n", p->bm_name);
	    goto bad;
	}


	if (!strcmp(t.bm_binary, p->bm_binary))
	{
	    t.bm_bfd = p->bm_bfd;
	    /* Slightly tacky. Keep a count of the number of times
	     * we're reuseing the same bfd to make closing easier by
	     * casting the usrdata to an int */
	    bfd_usrdata(t.bm_bfd)++;
	    t.bm.tsize = p->bm.tsize;
	}
    }

    /* If we have not already got a bfd then make one */
    if (!t.bm_bfd)
    {
	if (!(t.bm_bfd = open_bfd_read(t.bm_binary)))
	    goto bad;

	/* Check that image only has read-only "stuff" not read-write
	 * this will update the size in "t" while it does its work. */
	bfd_map_over_sections(t.bm_bfd, sec_is_ro, &t);

	/* On some machines we expect this to grow when constructors
         * are resolved - add in the magic delta here. */
	t.bm.tsize += machine->mc_constructors_grow;

	/* Believe we dont have to check for undefined symbols by
         * scanning over all the symbols found in this bfd on account
         * of the check that sections are fully located in sec_is_ro
         * */
    }
    
    /* Here then this is OK */
    LISTADD(list, bml, t);
    return (global_modlist = list);

    /* ---------- */
 bad:
    FREE(t.bm_name);
    FREE(t.bm_binary);
    
    return (global_modlist = list);
}

/* ------------------------------------------------------------ */
/* This function is called by the parser when it detects a blob
 * command to build the initial information about the blob. */

struct bloblist *parse_blob(struct bloblist *list, struct simpleblob *s)
{
    int			i;
    struct blob		t, *p;
    struct stat		sb;

    LISTINIT(list);

    /* Make up a temporary struct blob which will be added to the list
     * if all is well */
    
    memset(&t, 0, sizeof(t));
    t.bb_name  = s->name;
    t.bb_binary= s->path;
    t.bb_align = s->align;
    FREE(s);

    /* We perform a preliminary scan over the blobs listed already. In
     * this pass we detect when a blob name has been used already (an
     * error) and a warning if the same file has been re-used */
    for(i=0; i<list->bbl_num; i++)
    {
	p = &list->bbl_list[i];
	if (!strcmp(t.bb_name, p->bb_name))
	{
	    error("blob identifier `%s' reused.\n", p->bb_name);
	    goto bad;
	}

	if (!strcmp(t.bb_binary, p->bb_binary))
	{
	    warning("blob file `%s' already referenced, "
		    "will be included again", p->bb_binary);
	}
    }

    /* Open the file, to get the fd on it, and make sure it's readable */
    t.bb_fd = open(t.bb_binary, O_RDONLY);
    if (t.bb_fd < 0)
    {
	error("opening `%s': %s", t.bb_binary, strerror(errno));
	goto bad;
    }

    /* stat it to see how big it is */
    if (fstat(t.bb_fd, &sb))
    {
	error("stat `%s': %s",  t.bb_binary, strerror(errno));
	close(t.bb_fd);
	goto bad;
    }

    /* note how large this is */
    t.bb_len = sb.st_size;

    /* add the new item to the list */
    LISTADD(list, bbl, t);
    return (global_bloblist = list);

    /* ---------- */
 bad:
    FREE(t.bb_name);
    FREE(t.bb_binary);
    
    return (global_bloblist = list);
}


/* ------------------------------------------------------------ */

struct nameslist *parse_names(struct nameslist *list, struct namelist *s)
{
    int			i;
    struct namelist	*p;

    LISTINIT(list);

    /* Check that this namespace has not been given a name which
     * clashes with some other namespace name. */
    for(i=0; i<list->bnnl_num; i++)
    {
	p = &list->bnnl_list[i];
	if (!strcmp(s->bnl_name, p->bnl_name))
	{
	    error("namespace name `%s' reused.\n", s->bnl_name);
	    goto bad;
	}
    }
    
    /* XXX more ?? (e.g. check that a "/" and a "/types" are defined? */

    LISTADD(list, bnnl, *s);

 bad:		/* and good too */
    FREE(s);

    return (global_namespaces = list);
}    

/* ------------------------------------------------------------ */

/* This function is called by the parser when it detects a program
 * command to build the initial information about the program. */

struct proglist *parse_program(struct proglist *list, struct simpleprog *s)
{
    int			i;
    bool_t		is_primal;
    struct prog		t, *p;

    LISTINIT(list);

    /* Make up a temporary struct prog which will be added to the list
     * if all is well */
    
    memset(&t, 0, sizeof(t));
    t.bp_names = find_namespace_by_name(s->names);
    t.bp_binary = s->binary;
    t.bp_params = *(s->params);
    t.name= strdup(s->name);    /* XXX smh22 hacking */

    /* check the values seem sensible */
    if (t.bp_params.s > t.bp_params.p)
	warning("timeslice is longer than period\n");

    if (!(t.bp_names))
    {
	error("namespace `%s' not defined\n", s->names);
	goto bad_s;
    }
    t.bp_names->bnl_refs++;  /* bump the reference count on the namelist */

    is_primal = s->is_primal; /* squirrel this away for later */

    FREE(s->names);
    FREE(s->params);
    FREE(s->name);
    FREE(s);

    /* We perform a preliminary scan over the programs listed
     * already. In this pass we detect a case where a second instance
     * of some program should be loaded and copy appropriate
     * information from that one. */
    for(i=0; i<list->bpl_num; i++)
    {
	p = &list->bpl_list[i];
	if (!strcmp(t.bp_binary, p->bp_binary))
	{
	    t.bp_bfd = p->bp_bfd;
	    /* Slightly tacks. Keep a count of the number of times
	     * we're reuseing the same bfd to in the usrdata pointer
	     * */
	    bfd_usrdata(t.bp_bfd)++;
	    t.bp.tsize = p->bp.tsize;
	    t.bp.dsize = p->bp.dsize;
	    t.bp.bsize = p->bp.bsize;
	}
    }

    /* If we have not already got a bfd then make one */
    if (!t.bp_bfd)
    {
	if (!(t.bp_bfd = open_bfd_read(t.bp_binary)))
	    goto bad;

	/* This will update the sizes in "t" while it does its work. */
	bfd_map_over_sections(t.bp_bfd, sec_calc_sizes, &t.bp.tsize);

	/* On some machines we expect this to grow when constructors
         * are resolved - add in the magic delta here. */
	t.bp.tsize += machine->mc_constructors_grow;

	/* Believe we dont have to check for undefined symbols by
         * scanning over all the symbols found in this bfd on account
         * of the check that sections are fully located in
         * sec_calc_sizes. */
    }

    /* Here then this is OK */
    LISTADD(list, bpl, t);

    /* If this is the Primal program, then note its details for later.
     * Also make sure we only have one Primal. */
    if (is_primal)
    {
	if (Primal != -1)
	{
	    error("only one `primal' line allowed");
	    exit(2);
	}
	Primal = list->bpl_num - 1;
    }    

    return (global_proglist = list);
    
    /* ---------- */
 bad_s:
    FREE(s->names);
    FREE(s->params);
    FREE(s);
 bad:
    FREE(t.bp_binary);
    return (global_proglist = list);
}



/* ============================================================ */

static void write_out_namespace(void *base, struct namelist *names, size_t address)
{
    int		i;
    size_t	strings;
    struct name	*n;
    
    strings = address + ((names->bnl_num+1) * 2 * sizeof(void *));

    for(i=0; i<names->bnl_num; i++)
    {
	n = &names->bnl_list[i];
	TRC(("Working on name %s, type %s\n", n->bn_symbol,
	     n->bn_type == NAME_TYPE_SYMBOL ? "symbol" : "blob"));
	((bfd_vma *)(base + address))[0] = TE32(strings);

	if (n->bn_type == NAME_TYPE_SYMBOL)
	{
	    TRC(("Calling get_module_and_symbol\n"));
	    ((bfd_vma *)(base + address))[1] =
		TE32(get_module_and_symbol(n->bn_modname,
					   n->bn_symbol));
	}
	else
	{
	    struct blob *b;
	    TRC(("Calling find_blob_by_name\n"));
	    b = find_blob_by_name(n->bn_modname);
	    ((bfd_vma *)(base + address))[1] = TE32(b->bb_index);
	}	    
	    
	TRC(("Doing strcpy\n"));
	strcpy(base + strings, n->bn_path);
	strings += strlen(n->bn_path) + 1;
	address += 2 * sizeof(bfd_vma);
	TRC(("Done name\n"));
    }
    /* null terminate the list */
    ((bfd_vma *)(base + address))[0] = 0;
    ((bfd_vma *)(base + address))[1] = 0;
}

/* ------------------------------------------------------------ */

/* Write out the blob data areas, then the blob descriptors,
 * and return the next free address */
static void write_out_blobs(void *base, struct bloblist *blobs)
{
    int		i;
    struct blob	*b;
    int		remaining, nread, totread;
    size_t	address;

    /* write out data */
    for(i=0; i<blobs->bbl_num; i++)
    {
	b = &blobs->bbl_list[i];
	address = b->bb_vma;

	TRC(("Writing out blob %s, base:%#x len:%#x refs:%d align:%d\n",
	     b->bb_binary, address, b->bb_len, b->bb_refs, b->bb_align));

	if (!b->bb_refs)
	    non_fatal("warning: blob %s never referenced; including anyway",
		      b->bb_name);

	remaining = b->bb_len;
	totread = 0;

	while (remaining > 0 &&
	       (nread = read(b->bb_fd, (base+address+totread), remaining)) > 0)
	{
	    remaining -= nread;
	    totread   += nread;
	}
	/* did we exit because of a read error, or natural causes? */
	if (nread < 0)
	    fatal("read %s: %s", b->bb_binary, strerror(errno));
	if (remaining != 0)
	    warning("urk!  file %s has shrunk", b->bb_binary);
	/* should also test for file growing, but that's more complex */

	close(b->bb_fd);
    }

    /* write out blob descriptors */
    address = b->bb_vma + b->bb_len;  /* uses last "b" from for loop */
    address = align_power(address, 3);
    for(i=0; i<blobs->bbl_num; i++)
    {
	b = &blobs->bbl_list[i];
	TRC(("Blob descriptor for %s at %#x\n", b->bb_binary, address));
	b->bb_index = address;
	((bfd_vma *)(base + address))[0] = TE32(b->bb_vma); /* XXX 32 vs 64 */
	((bfd_vma *)(base + address))[1] = TE32(b->bb_len);

	address += 2 * sizeof(bfd_vma *);
    }
}



/* ====================================================================== */
/* This function iterates over everything working out where it should
 * be put and filling this information in. This takes a number of passes.
 */

#ifndef TRY_BFD_CLOSE
#  undef bfd_close
#  define bfd_close(bfd) (void)0
#endif

#define bfd_finished(bfd)			\
do {						\
    if (bfd_usrdata(bfd))			\
	bfd_usrdata(bfd)--;			\
    else					\
	bfd_close(bfd);				\
    bfd=NULL;					\
} while(0)

/* ---------- */

#define ALIGN(x)	align_power((x), machine->mc_pageshift)
#define MINIALIGN(x)	align_power((x), 4)

static void allocate_locations(struct ntsc *ntsc)
{
    size_t		address;
    size_t		nexus_begin_addr, nexus_last_ok_addr;
    size_t		size;
    struct module	*mod;
    struct blob		*blob;
    struct prog		*prog;
    struct namelist	*name;
    int			i;
    void		*image, *base;
    bfd			*bfd;
    int			out;
    
    /* -------------------- */
    /* PHASE ZERO: Emit warnings about unused modules and namespaces */
    
    for(i=0; i<global_modlist->bml_num; i++)
    {
	mod = &global_modlist->bml_list[i];
	
	if (!mod->bm_refs)
	    non_fatal("module `%s' is not referenced "
		      "(but will be included anyway)\n",
		      mod->bm_name);
    }

    for(i=0; global_namespaces && i<global_namespaces->bnnl_num; i++)
    {
	name = &global_namespaces->bnnl_list[i];
	if (!name->bnl_refs)
	    non_fatal("namespace `%s' is not referenced "
		      "(but will be included anyway)\n",
		      name->bnl_name);
    }

    if (Primal == -1)
	fatal("At least one `primal' line must be specified");

    if (num_bflags_seen == 0)
	fatal("Need to give at least one 'b' flag");

    /* -------------------- */
    /* PHASE ONE: ADD IT ALL UP (assumption - we need to know for
     * allocating them) */
    /* This phase is now skipped, since text, data & bss are now
     * allocated consecutively - and1000 */

#if 0
    total_progs_text = total_progs_data = total_progs_bss = 0;

    for(i=0; i<global_proglist->bpl_num; i++)
    {
	prog = &global_proglist->bpl_list[i];
	
	total_progs_text += ALIGN(prog->bp.tsize);
	total_progs_data += ALIGN(prog->bp.dsize);
	total_progs_bss  += ALIGN(prog->bp.bsize);
    }
    
    if (verbose)
	printf("totals for programs are T%#x, D%#x, B%#x\n",
	       total_progs_text, total_progs_data, total_progs_bss);
#endif /* 0 */

    /* -------------------- */
    /* PHASE TWO: Assign locations. At the moment we allocate the ntsc
     * sequentially (including bss) followed by the modules, then the
     * blobs, followed by the namespaces followed by all the program
     * (texts, data, & bss) Program data & bss are contiguous */

    /* put base of image at NTSC base */
#if 0
    address = machine->mc_baseaddr;

    ntsc->bk.taddr = address;
    /* no need to separate ntsc data and bss since the bss goes in the
     * boot image anyway.
     */
    ntsc->bk.daddr = (address += ALIGN(ntsc->bk.tsize));
    ntsc->bk.baddr = (address += MINIALIGN(ntsc->bk.dsize));
#endif /* 0 */

    /* next free address is end of NTSC bss */
    address = ntsc->bk.baddr + ntsc->bk.bsize;

    nexus_begin_addr = address;

    if (verbose)
      printf("NTSC at T%#x D%#x B%#x\n",
	     ntsc->bk.taddr, ntsc->bk.daddr, ntsc->bk.baddr);
    
    address = ALIGN(address);
    address += ALIGN(1);
    nexus_last_ok_addr = address;

    if (verbose)
	printf("nexus space reserved from %#x to %#x\n",
	       nexus_begin_addr, nexus_last_ok_addr);

    for(i=0; i<global_modlist->bml_num; i++)
    {
	mod = &global_modlist->bml_list[i];
	
	mod->bm.taddr = address;

	if (verbose)
	    printf("module `%s' text at %#x", mod->bm_name, address);

	/* put in a data segment for Alpha .lita sections */
	if (mod->bm.dsize)
	    mod->bm.daddr = address + mod->bm.tsize;
	if (verbose)
	    printf(" data at %#x (size %#x)\n", mod->bm.daddr, mod->bm.dsize);
	address += ALIGN(mod->bm.dsize + mod->bm.tsize);
    }


    /* blobs */
    if (global_bloblist)
    {
	blobbase = address;
	for(i=0; i<global_bloblist->bbl_num; i++)
	{
	    blob = &global_bloblist->bbl_list[i];

	    address = align_power(address, blob->bb_align);
	    blob->bb_vma = address;

	    if (verbose)
		printf("blob `%s' data at %#x, len %d\n",
		       blob->bb_name, blob->bb_vma, blob->bb_len);
	
	    address += blob->bb_len;
	}
	address = align_power(address, 3);
	if (verbose)
	    printf("blob descriptor base at %#x\n", address);
	address += global_bloblist->bbl_num * 2 * sizeof(bfd_vma *);
	bloblen = address - blobbase;
	address = ALIGN(address);  /* skip to the start of the next page */
    }


    /* namespaces */
    for(i=0; global_namespaces && i<global_namespaces->bnnl_num; i++)
    {
	name = &global_namespaces->bnnl_list[i];
	name->bnl_addr = address;
	if (verbose)
	    printf("Namespace `%s' at %#x\n", name->bnl_name, address);
	address += ALIGN(((name->bnl_num +1) * 2 * sizeof(void *))
			 + name->bnl_space);
    }


    for(i=0; i<global_proglist->bpl_num; i++)
    {
	prog = &global_proglist->bpl_list[i];
	
	prog->bp.taddr = address;
	prog->bp.daddr = (address += ALIGN(prog->bp.tsize));
	if (!strcmp(machine->mc_bfd_target, "a.out-arm-netbsd"))
	    /* dont align because linker doesnt support -B */
	    prog->bp.baddr = (address += prog->bp.dsize);
	else
	    prog->bp.baddr = (address += MINIALIGN(prog->bp.dsize));
	address += prog->bp.bsize;
	address = ALIGN(address);

	if (verbose)
	    printf("Program `%s' T%#x D%#x B%#x\n",
		   prog->bp_binary,
		   prog->bp.taddr,
		   prog->bp.daddr,
		   prog->bp.baddr);		   
    }
    
    /* at this point, address contains the last address of the bss of
     * the last program, round up to the next page.  This is used to
     * make the nexus_EOI record. */

    /* base for output */
    size = address - machine->mc_baseaddr;
    TRC(("Need %lx\n", size));
    image = xmalloc(size);
    /* logical base */
    base = image - machine->mc_baseaddr;

    if (size > (2 * 1024 * 1024))  /* 2Mb */
	non_fatal("caution: excessivly large output image %#x\n", size);



    /* -------------------- */
    /* PHASE THREE: Perform the linking. This is done by a sub function */

    TRC(("Reading NTSC\n"));
    bfd = perform_link(ntsc->bk_bfd, &ntsc->bk, 1);
    bfd_finished(ntsc->bk_bfd);
    if (!strcmp(machine->mc_bfd_target, "a.out-arm-netbsd"))
    {
	/* XXX yet another horrendous hack. Since we know that the bfd
         * will lie to write_out_bfd and give the base address as zero
         * instead of what it ought to be we must add on that as an
         * adjustment here */
	write_out_bfd_netbsd(bfd, base, &ntsc->bk);
    }
    else
	write_out_bfd(bfd, base);
    bfd_finished(bfd);

    TRC(("Linking modules\n"));
    for(i=0; i<global_modlist->bml_num; i++)
    {
	mod = &global_modlist->bml_list[i];
	
	bfd = perform_link(mod->bm_bfd, &mod->bm, 0);
	/* get rid of unlinked one, but *keep* new one for use by
         * namespace symbolname lookup code */
	bfd_finished(mod->bm_bfd);
	/* FREE(mod->bm_symbols); */
	mod->bm_symbols = NULL;
	if (!strcmp(machine->mc_bfd_target, "a.out-arm-netbsd"))
	{
	    /* XXX yet another horrendous hack. Since we know that the bfd
	     * will lie to write_out_bfd and give the base address as zero
	     * instead of what it ought to be we must add on that as an
	     * adjustment here */
	    write_out_bfd_netbsd(bfd, base, &mod->bm);
	}
	else
	    write_out_bfd(bfd, base);
	mod->bm_bfd = bfd;
    }

    TRC(("Linking progs\n"));
    for(i=0; i<global_proglist->bpl_num; i++)
    {
	prog = &global_proglist->bpl_list[i];
	
	bfd = perform_link(prog->bp_bfd, &prog->bp, 0);
	if (!strcmp(machine->mc_bfd_target, "a.out-arm-netbsd"))
	{
	    /* XXX yet another horrendous hack. Since we know that the bfd
	     * will lie to write_out_bfd and give the base address as zero
	     * instead of what it ought to be we must add on that as an
	     * adjustment here */
	    write_out_bfd_netbsd(bfd, base, &prog->bp);
	}
	else
	    write_out_bfd(bfd, base);
    }


    /* -------------------- */
    /* PHASE THREE (and a bit): Writing out blobs */
    if (global_bloblist)
    {
	TRC(("Making Blobs\n"));
	write_out_blobs(base, global_bloblist);
    }


    /* -------------------- */
    /* PHASE FOUR: Make the namespaces */

    TRC(("Making namespaces\n"));
    for(i=0; global_namespaces && i<global_namespaces->bnnl_num; i++)
    {
	name = &global_namespaces->bnnl_list[i];
	TRC(("Writing out %s...", name->bnl_name));
	write_out_namespace(base, name, name->bnl_addr);
	TRC((" done\n"));
    }

    /* -------------------- */
    /* PHASE FIVE: Make the "nexus" as EAH would have called it */

    TRC(("Making Nexus\n"));
    if (write_out_nexus(base, nexus_begin_addr, ntsc, address)
	>= nexus_last_ok_addr)
	fatal("internal error: nexus has overshot!\n");
	
    /* -------------------- */
    /* PHASE SIX: Write it out */
    TRC(("Writing out\n"));
    if ((out = open(outname, O_WRONLY | O_TRUNC | O_CREAT, 0777)) < 0)
	fatal("open: %s: %s\n", outname, strerror(errno));

    /* finished with some prog bfds */

    TRC(("Closing prog bfd\n"));
    for(i=0; i<global_proglist->bpl_num; i++)
    {
	prog = &global_proglist->bpl_list[i];
	
	bfd_finished(prog->bp_bfd);
    }
    
    if (write(out, image, size) != size)
    {
	non_fatal("write: %s: %s\n", outname, strerror(errno));
	myunlink(outname);
	exit(1);
    }
    
    if (close(out) < 0)
    {
	non_fatal("close: %s: %s\n", outname, strerror(errno));
	myunlink(outname);
	exit(1);
    }

    /* finish off the gdb script file */
    if (gdbfile)
    {
	fprintf(gdbfile, "echo \\nDone\\n\n");
	fclose(gdbfile);
    }

    /* DONE! */
}

/* ====================================================================== */



static void describe_image(struct ntsc *ntsc, struct modlist *modlist,
			   struct nameslist *nameslist,
			   struct proglist *proglist)
{
    struct namelist *k;
    struct prog *p;
    const char *s;
    int i,j;
    
    printf("describe_image: cooked data structure is...\n");
    printf("ntsc: %s (sizes %#lx, %#lx, %#lx)(addresses %#lx, %#lx, %#lx)\n",
	   ntsc->bk_binary,
	   ntsc->bk.tsize, ntsc->bk.dsize, ntsc->bk.bsize,
	   ntsc->bk.taddr, ntsc->bk.daddr, ntsc->bk.baddr);
    printf("mods: ");
    for(i=0; modlist && i<modlist->bml_num; i++)
    {
	printf("(%s as %s at %#lx), ",
	       modlist->bml_list[i].bm_name,
	       modlist->bml_list[i].bm_binary,
	       modlist->bml_list[i].bm.taddr);
    }
    
    printf("\n");
    printf("names:\n");
    for(j=0; nameslist && j<nameslist->bnnl_num; j++)
    {
	k = &nameslist->bnnl_list[j];
	printf("  %s: (addr %#lx)", k->bnl_name, k->bnl_addr);
	for(i=0; i<k->bnl_num; i++)
	{
	    s = k->bnl_list[i].bn_symbol;
	    printf("(%s for %s %s",
		   k->bnl_list[i].bn_path,
		   k->bnl_list[i].bn_type == NAME_TYPE_SYMBOL? "symbol":"blob",
		   k->bnl_list[i].bn_modname);
	    if (k->bnl_list[i].bn_type == NAME_TYPE_SYMBOL)
		printf("%s%s",   s ? "$" : "",   s ? s : "");
	    printf("), ");
	}
	printf("\n");
    }
    printf("progs: ");
    for(i=0; proglist && i<proglist->bpl_num; i++)
    {
	p = &proglist->bpl_list[i];
	printf("(%s with %s sizes %#lx, %#lx, %#lx)(addresses %#lx, %#lx, %#lx), ",
	       p->bp_binary,
	       p->bp_names ? p->bp_names->bnl_name : "XXX",
	       p->bp.tsize, p->bp.dsize, p->bp.bsize,
	       p->bp.taddr, p->bp.daddr, p->bp.baddr);
    }
    printf("\n");
}


/* Called from parser when config file fully parsed */
void make_image(struct ntsc *ntsc, struct modlist *modlist,
		struct nameslist *nameslist, struct proglist *proglist)
{
    assert(modlist == global_modlist);
    assert(nameslist == global_namespaces);
    assert(proglist == global_proglist);

    if (!(ntsc && modlist && nameslist && proglist))
	fatal("need to supply at least an ntsc, a module, a namespace and "
	      "a program");
    
    /* This is the point at which to bail if there have been errors so far */
    if (error_occured)
    {
	if (verbose)
	    describe_image(ntsc, modlist, nameslist, proglist);
	exit(1);
    }
    
    /* Next pass is to run over everything and work out where it is to
     * be put */

    allocate_locations(ntsc);

    if (verbose)
	describe_image(ntsc, modlist, nameslist, proglist);
}    


static void usage(void)
{
    fprintf(stderr, "usage: %s -p platform [-d] [-b] [-v] [-x file] "
	    "[-o file] [-M] [name]\n"
	    "\t-p plat  specify which platform to build for\n"
	    "\t-d       turn on debugging info\n"
	    "\t-b       blow away stale .run files\n"
	    "\t-v       turns on verbosity\n"
	    "\t-x file  dumps the sections and symbols of \"file\"\n"
	    "\t-o file  set the output filename to \"file\"\n"
	    "\t-M       output dependancy information\n"
	    "\t-g file  output gdb script file\n"
	    "\tname     set .nbf control filename (defaults to stdin)\n",
	    prog_name);
    exit(2);
}

int main(int argc, char **argv)
{
    extern FILE	*yyin;
    int c;
    struct platform *p;
    FILE *devnull;
    
    prog_name = argv[0];
    verbose = 0;
   
    bfd_init();
    
    /* unless otherwise specified, dependencies get written to /dev/null */ 
    devnull = fopen("/dev/null", "w");
    if (!devnull)
    {
	warning("couldn't open /dev/null for write\n");
	devnull = stderr;
    }

    depend_output_file = devnull;
    while((c = getopt(argc, argv, "x:o:dbp:vMg:i:")) != EOF)
	switch(c)
	{
	case 'd':
	    dflg++;
	    break;

	case 'b':
	    blowruns=1;
	    break;

	case 'x':
	    blob1(optarg);
	    exit(0);

	case 'o':
	    outname = optarg;
	    break;

	case 'p':
	    for(p=platforms; p->pl_info; p++)
	    {
		if (!strcmp(p->pl_name, optarg))
		{
		    machine = p->pl_info;
		    platform = p;
		    break;
		}
	    }
	    if (!platform)
	    {
		fprintf(stderr, "%s: platform `%s' is not a valid platform\n",
			prog_name, optarg);
		fprintf(stderr, "\tValid platforms are: ");
		for(p=platforms; p->pl_info; p++)
		    fprintf(stderr, "%s ", p->pl_name);
		fprintf(stderr, "\n");
		exit(1);
	    }
	    break;

	case 'v':
	    verbose = 1;
	    break;

	case 'M':
	    depend_output_file_name =
		xmalloc(sizeof(char)*(strlen(outname)+2));

	    strcpy(depend_output_file_name, outname);
	    strcat(depend_output_file_name, ".d");

	    if (!(depend_output_file = fopen(depend_output_file_name, "w")))
	    {
		warning("could not create depend file\n");
		depend_output_file = stderr;
	    }
	    fprintf(depend_output_file,"%s : ", outname);
	    break;

	case 'g':
	    gdbfile = fopen(optarg, "w");
	    if (!gdbfile)
		fatal("couldn't open gdb script file `%s' for write: %s",
		      optarg, strerror(errno));
	    fprintf(gdbfile, "echo Loading symbol files:\n");
	    break;

	case 'i':
	    ident=strdup(optarg);
	    break;

	default:
	    usage();
	}
    
    if (!platform) fatal("Must supply '-p' option");

    if (optind == argc)
    {
	yyin = stdin;
	parser_input_file_name = "(stdin)";
	depend_output_file = devnull;
    }
    else if (optind + 1 == argc)
    {
	parser_input_file_name = argv[optind];
	yyin = fopen(parser_input_file_name, "r");
	if (!yyin)
	    fatal("fopen: %s: %s", parser_input_file_name, strerror(errno));
	/* the output file is bound to depend on the input file */
	fprintf(depend_output_file," %s ", parser_input_file_name);
    }
    else
	usage();

    if (yyparse())
	exit(1);
    if (error_occured)
	exit(1);
    fprintf(depend_output_file,"\n");
    exit(0);
}

/* End of build.c */
