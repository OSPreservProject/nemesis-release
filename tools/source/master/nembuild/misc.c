/*
 *	misc.c
 *	------
 *
 * $Id: misc.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory
 * Copyright (c) 1996 Richard Black
 * Right to use is administered by University of Cambridge Computer Laboratory
 *
 */

#define DEBUG_BFD_SEND 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <assert.h>		/* temp */

/* On some systems this is defined by ansidecl.h which is included by bfd.h
 * On some its not. Wierd */
#if defined(__linux) && !defined(linux_for_arm)
#  define PARAMS(x) x
#endif
#include <bfd.h>   /* needs PARAMS */

#include "descrip.h"
#include "build.h"

#ifdef OPT_NLIST
#  include <nlist.h>
#endif

/* Work-around for broken ARM backend that doesn't bother to set
 * the BFD backend's symbol_leading_char value correctly */
#ifndef FORCE_UNDERSCORE
#ifdef hpux
#define FORCE_UNDERSCORE 1
#else
#define FORCE_UNDERSCORE 0
#endif /* hpux */
#endif /* ! FORCE_UNDERSCORE */

/* ---------------------------------------------------------------------- */
/* open_bfd_read is just a commoned out case for the code to open a
 * read only mode bfd on some object file and check that it is a real
 * object file etc. */

bfd *open_bfd_read(char *binary)
{
    bfd		*abfd;
    
    if (!(abfd = bfd_openr(binary, NULL)))
    {
	error("bfd_openr: %s: %s\n", binary, bfd_strerror);
	return NULL;
    }
    
    if (!bfd_check_format(abfd, bfd_object))
    {
	error("bfd_check_format: %s: %s\n", binary, bfd_strerror);
	return NULL;
    }
    
    return abfd;
}

bfd *try_open_bfd_read(char *binary)
{
    bfd		*abfd;
    
    if (!(abfd = bfd_openr(binary, NULL)))
	return NULL;
    
    if (!bfd_check_format(abfd, bfd_object))
	return NULL;
    
    return abfd;
}

/* ---------------------------------------------------------------------- */

struct module *find_module_by_name(char *name)
{
    int i;
    struct modlist *l = global_modlist;
    struct module *m;

    if (!l) return NULL;

    for(i=0; i<l->bml_num; i++)
	if (!strcmp(name, (m = &l->bml_list[i])->bm_name))
	    return m;
    return NULL;
}

struct blob *find_blob_by_name(char *name)
{
    int i;
    struct bloblist *l = global_bloblist;
    struct blob *b;

    if (!l) return NULL;

    for(i=0; i<l->bbl_num; i++)
	if (!strcmp(name, (b = &l->bbl_list[i])->bb_name))
	    return b;
    return NULL;
}

/* ---------------------------------------------------------------------- */

/* Common code to get the address of a symbol in a module.
 *  Returns 1 and sets retval on success, or 0 without modifying retval
 *  for failure. */
static int sym_in_mod(struct module *m, const char *s, bfd_vma *retval)
{
    long	storage_needed;
    long	i;
    char	*sym;
    char	sym_leader;

    /* if this BFD backend adds underscores, then tack on onto the front
     * of the symbol we're comparing */
    if (FORCE_UNDERSCORE)
	sym_leader = '_';
    else
	sym_leader = bfd_get_symbol_leading_char(m->bm_bfd);

    if (sym_leader)
    {
	TRC(("sym_in_mod: BFD backend uses `%c' as symbol leader\n",
	     sym_leader));
	sym = xmalloc(strlen(s)+2);
	sprintf(sym, "%c%s", sym_leader, s);
    }
    else
    {
	TRC(("sym_in_mod: BFD backend uses no symbol leader\n"));
	sym = s;
    }

#if defined(OPT_NLIST)
    if (machine->mc_nlist)
    {
	struct nlist nl[2];
	
	nl[0].n_name = sym;
	nl[1].n_name = 0;

	nlist(bfd_get_filename(m->bm_bfd), &nl);
	if (nl[0].n_value == 0) {
	    printf("get_module_and_symbol [nlist]: symbol %s not defined in %s.\n",
		   sym, bfd_get_filename(m->bm_bfd));
	    return 0;
	}
	(*retval) = nl[0].n_value;
	return 1;
    }
#endif /* OPT_NLIST */	

    if (!(m->bm_symbols))
    {
	/* we haven't already read the symbol table in, so do that now */
	TRC(("bfd->xvec->_bfd_get_symtab_upper_bound = %p\n",
	     (m->bm_bfd)->xvec->_bfd_get_symtab_upper_bound));
	storage_needed = bfd_get_symtab_upper_bound(m->bm_bfd);
	if (storage_needed < 0)
	    fatal("bfd_get_symtab_upper_bound: %s: %s", m->bm_binary,
		  bfd_strerror);

	m->bm_symbols = xmalloc(storage_needed);
	m->bm_n_symbols = bfd_canonicalize_symtab(m->bm_bfd, m->bm_symbols);
	if (m->bm_n_symbols < 0)
	    fatal("bfd_canonicalize_symtab: %s: %s\n", m->bm_binary,
		  bfd_strerror);
    }

    for(i=0; i<m->bm_n_symbols; i++)
    {
#if 0
	TRC(("sym_in_mod: comparing `%s' with `%s'\n",
	     sym, bfd_asymbol_name(m->bm_symbols[i])));
#endif /* 0 */
	if (!strcmp(sym, bfd_asymbol_name(m->bm_symbols[i]))
	    /* XXX is this second clasuse (a) needed (b) done properly? */
	    /* XXX it appears to be "R" on some systems for read-only
	     * data, but we havnt decided yet whether we *want* it to
	     * be code or data! */
	    && ((bfd_decode_symclass(m->bm_symbols[i]) == 'T') ||
		(bfd_decode_symclass(m->bm_symbols[i]) == 'R')))
	{
	    TRC(("sym_in_mod: found! symbol `%s' has value %#x\n",
		 sym, bfd_asymbol_value(m->bm_symbols[i])));
	    if (sym_leader)
		free(sym);
	    (*retval) = bfd_asymbol_value(m->bm_symbols[i]);
	    return 1;
	}
    }

    /* not found - return 0 */
    return 0;
}

/* ---------------------------------------------------------------------- */
/* Now the "official" exported interfaces to the sym_in_mod(): */

int check_module_for_symbol(struct module *m, const char *s)
{
    bfd_vma	junk_vma;

    return sym_in_mod(m, s, &junk_vma);
}

bfd_vma get_module_and_symbol(char *name, const char *s)
{
    struct module	*m;
    bfd_vma		vma;
    int			found;

    if (!(m = find_module_by_name(name)))
	fatal("get_module_and_symbol: module %s dissappeared!\n",
	      name, s);
    TRC(("Found module %x\n", m));

    found = sym_in_mod(m, s, &vma);

    if (found)
	return vma;

    free(m->bm_symbols);
    m->bm_symbols = NULL;
    fatal("get_module_and_symbol: symbol %s$%s dissappeared!\n", name, s);
}

/* ---------------------------------------------------------------------- */

static char *flags2ascii(flagword f)
{
    static char buf[256];  /* XXX hack! */
    
    buf[0] = '\0';

    if (f & SEC_ALLOC)    strcat(buf, "ALLOC ");
    if (f & SEC_LOAD)     strcat(buf, "LOAD ");
    if (f & SEC_RELOC)    strcat(buf, "RELOC ");
    if (f & SEC_READONLY) strcat(buf, "READONLY ");
    if (f & SEC_CODE)     strcat(buf, "CODE ");
    if (f & SEC_DATA)     strcat(buf, "DATA ");
    if (f & SEC_ROM)      strcat(buf, "ROM ");  /* how likely is this? :) */
    /* ... some C++ related gunk */
    if (f & SEC_HAS_CONTENTS) strcat(buf, "HAS_CONTENTS ");
    if (f & SEC_NEVER_LOAD) strcat(buf, "NEVER_LOAD ");
    if (f & SEC_IS_COMMON) strcat(buf, "SEC_IS_COMMON ");
    if (f & SEC_DEBUGGING) strcat(buf, "DEBUGGING ");
    if (f & SEC_IN_MEMORY) strcat(buf, "IN_MEMORY ");
    
    /* extend as required */
    f &= ~(SEC_ALLOC | SEC_LOAD | SEC_RELOC | SEC_READONLY | SEC_CODE |
	   SEC_DATA | SEC_ROM | SEC_HAS_CONTENTS | SEC_NEVER_LOAD |
	   SEC_IS_COMMON | SEC_DEBUGGING | SEC_IN_MEMORY);

    if (f) strcat(buf, "..."); /* warn about others */

    if (buf[strlen(buf)-1] == ' ')
	buf[strlen(buf)-1]='\0';

    return buf;
}

/* ---------------------------------------------------------------------- */

#define sec_return_if_irrelevant(abfd, sec, flags)			   \
do {									   \
    const char *secname;						   \
									   \
    if (!((flags = bfd_get_section_flags(abfd, sec)) & SEC_ALLOC))	   \
	/* Some section which doesnt have virtual addresses allocated	   \
         * to it */							   \
	return;								   \
    if ((!strcmp(bfd_get_target(abfd), "ecoff-littlealpha"))		   \
	&& ((!strcmp(secname = bfd_get_section_name(abfd, sec), ".pdata")) \
	    || (!strcmp(secname, ".xdata"))))				   \
	/* DEC exception mumblies - ripped out by final link */		   \
	    return;							   \
    if (!bfd_get_section_size(sec))			   \
	/* Its zero bytes. Must be irrelevant */			   \
	return;								   \
} while(0)

/* ---------------------------------------------------------------------- */

/* Return 1 if 'secname' is a .lit? section, 0 otherwise.
 * Print warning if 'secname' is not ".lita", ".lit4", or ".lit8" */
static int litsec_p(const char *secname)
{
    if (!strncmp(secname, ".lit", 4))
    {
	if (strlen(secname) != 5 ||
	    (secname[4] != 'a' &&
	     secname[4] != '4' &&
	     secname[4] != '8'))
	    non_fatal("warning, found a `%s' section, "
		      "treating it as a .lita section\n", secname);
	return 1;
    }
    
    return 0;
}

/* This function ensures that a section holds relevant stuff for being
 * in a module.
 */

void sec_is_ro(bfd *abfd, sec_ptr sec, void *v)
{
    struct module	*mod = v;
    flagword		 flags;
    const char		*secname;
    
    /* The flags that seem to matter here are: SEC_ALLOC (means it
     * contains stuff that needs memory), SEC_READONLY,
     * SEC_HAS_CONTENTS, and SEC_RELOC (which is supposed to mean
     * "section contains relocation information" which we would *want*
     * to ensure (i.e. has not been located already) but it doesnt
     * appear to mean this in practice, it appears to mean "section
     * contains references to unbound variables"). Test on size is
     * because otherwise a zero sized bss/data will rule you out. */
    /* XXX alpha_ecoff has non readonly text segment for code */

    sec_return_if_irrelevant(abfd, sec, flags);

    /* Now test the things we dont like independently so we can give
     * better error messages */
    if (!(flags & SEC_READONLY) && !(flags & SEC_CODE))
    {
	error("%s: section `%s' is not read only (flags %#x)\n",
	      mod->bm_binary, bfd_get_section_name(abfd, sec), flags);
    }
#if 0
    if (flags & SEC_RELOC)
    {
	error("%s: section `%s' is not fully resolved (flags %#x)\n",
	      mod->bm_binary, bfd_get_section_name(abfd, sec), flags);
    }
#endif /* 0 */
    /* XXX alpha ecoff gives sections addresses even before linking */
#if 0
    if (bfd_get_section_vma(abfd, sec) != 0)
    {
	error("%s: section `%s' is located already (at %p)\n",
	      mod->bm_binary, bfd_get_section_name(abfd, sec),
	      (void*)bfd_get_section_vma(abfd, sec));
    }
#endif /* 0 */
    /* XXX May need to worry about alignment here and/or size after
     * relocation. See bfd manual about page 22 */

    /* On Alphas, ".lita", ".lit4" and ".lit8" sections are flagged as
     * readonly, but should be accounted to the data segment for the
     * purposes of the linker. Hence this test. */
    secname = bfd_get_section_name(abfd, sec);
    TRC(("sec_is_ro(): got %s section\n", secname));
    if (litsec_p(secname))
	mod->bm.dsize += bfd_get_section_size(sec);
    else
	mod->bm.tsize += bfd_get_section_size(sec);
}

/* ---------------------------------------------------------------------- */
/* This function calculates the total sizes of the text, data and bss
 * sections of an object. It assumes it has a pointer to an
 * appropriate structure being passed in. */

void sec_calc_sizes(bfd *abfd, sec_ptr sec, void *v)
{
    struct link		*sizes = v;
    flagword		 flags;
    const char		*secname;
    
    sec_return_if_irrelevant(abfd, sec, flags);

#if 0
    /* debugging use */
    printf("sec_calc_sizes(%p, %p, %p)\n", abfd, sec, v);
    printf("Section info:\n\tname:\t%s\n\tindex:\t%d\n\tflags:\t%s\n"
	   "\tvma:\t%08x\n\tcooked_size:\t%08x\n\traw_size:\t%08x\n",
	   sec->name,
	   sec->index,
	   flags2ascii(sec->flags),
	   sec->vma,
	   sec->_cooked_size,
	   sec->_raw_size);
#endif /* 0 */

    /* Now test the things we dont like independently so we can give
     * better error messages */
#if 0
    if (flags & SEC_RELOC)
    {
	error("%s: section `%s' is not fully resolved (flags %#x)\n",
	      bfd_get_filename(abfd), bfd_get_section_name(abfd, sec),
	      bfd_get_section_flags(abfd, sec));
    }
#endif /* 0 */
    /* XXX alpha ecoff gives sections addresses even before linking */
#if 0
    if (bfd_get_section_vma(abfd, sec) != 0)
    {
	error("%s: section `%s' is located already (at %p)\n",
	      bfd_get_filename(abfd), bfd_get_section_name(abfd, sec),
	      (void*)bfd_get_section_vma(abfd, sec));
    }
#endif /* 0 */

    /* Is this a funky Alpha .lita section? */
    secname = bfd_get_section_name(abfd, sec);
    if (litsec_p(secname))
    {
	/* always count .lit? sections as data: cos the linker does */
	sizes->dsize += bfd_get_section_size(sec);
    }
    else
    {
	/* Work out what type of section this is:
	 * text: SEC_READONLY | SEC_LOAD or
         *       SEC_CODE | SEC_LOAD or
         *       SEC_CODE | SEC_LOAD | SEC_READONLY
	 * data: SEC_LOAD 
	 * bss: (none) */
	switch(flags & (SEC_READONLY | SEC_LOAD | SEC_CODE))
	{
	case (SEC_CODE | SEC_LOAD):
	case (SEC_READONLY | SEC_LOAD):
	case (SEC_READONLY | SEC_LOAD | SEC_CODE): 
	    sizes->tsize += bfd_get_section_size(sec);
	    break;
	case (SEC_LOAD):
	    sizes->dsize += bfd_get_section_size(sec);
	    break;
	case (SEC_NO_FLAGS):
	    sizes->bsize += bfd_get_section_size(sec);
	    break;
	default:
	    error("%s: section `%s' has bizzare flags %#x\n",
		  bfd_get_filename(abfd), bfd_get_section_name(abfd, sec),
		  flags);
	    break;
	}
    }
}

/* ---------------------------------------------------------------------- */
/* This function calculates the total sizes of the text, data and bss
 * sections of an object. It assumes it has a pointer to an
 * appropriate structure being passed in. It also fills in the link
 * addresses of the various sections. */

void sec_fill_in_sizes(bfd *abfd, sec_ptr sec, void *v)
{
    struct link		*sizes = v;
    flagword		 flags;
    const char		*secname;

    sec_return_if_irrelevant(abfd, sec, flags);

    TRC(("sec_fill_in_sizes(xx, %p, %p)\n", sec, v));
    
    /* Now test the things we dont like independently so we can give
     * better error messages */
    if (flags & SEC_RELOC)
    {
	error("%s: section `%s' is not fully resolved (flags %#x)\n",
	      bfd_get_filename(abfd), bfd_get_section_name(abfd, sec),
	      bfd_get_section_flags(abfd, sec));
    }

    /* Is this a funky Alpha .lita section? */
    secname = bfd_get_section_name(abfd, sec);
    if (litsec_p(secname))
    {
	/* always count .lit? sections as data: cos the linker does */
	sizes->dsize += bfd_get_section_size(sec);
	if (!sizes->daddr)
	    sizes->daddr = bfd_get_section_vma(abfd, sec);
    }
    else
    {
	/* Work out what type of section this is:
	 * text: SEC_READONLY | SEC_LOAD or
         *       SEC_CODE | SEC_LOAD or
         *       SEC_CODE | SEC_LOAD | SEC_READONLY
	 * data: SEC_LOAD 
	 * bss: (none) */
	switch(flags & (SEC_READONLY | SEC_LOAD | SEC_CODE))
	{
	case (SEC_CODE | SEC_LOAD):
	case (SEC_READONLY | SEC_LOAD):
	case (SEC_READONLY | SEC_LOAD | SEC_CODE): 
	    sizes->tsize += bfd_get_section_size(sec);
	    if (!sizes->taddr)
		sizes->taddr = bfd_get_section_vma(abfd, sec);
	    break;

	case (SEC_LOAD):
	    sizes->dsize += bfd_get_section_size(sec);
	    if (!sizes->daddr)
		sizes->daddr = bfd_get_section_vma(abfd, sec);
	    break;

	case (SEC_NO_FLAGS):
	    sizes->bsize += bfd_get_section_size(sec);
	    if (!sizes->baddr)
		sizes->baddr = bfd_get_section_vma(abfd, sec);
	    break;

	default:
	    error("%s: section `%s' has bizzare flags %#x\n",
		  bfd_get_filename(abfd), bfd_get_section_name(abfd, sec),
		  flags);
	    break;
	}
    }
}

/* ---------------------------------------------------------------------- */
/* This function copies the contents of the section into the target
 * area of memory (which will subsequently be written to the output
 * file). */

static void sec_write_out(bfd *abfd, sec_ptr sec, void *base)
{
    flagword		 flags;
    void		*loc;
    bfd_size_type	 size;
    
    sec_return_if_irrelevant(abfd, sec, flags);

    loc = base + bfd_get_section_vma(abfd, sec);
    size = bfd_get_section_size(sec);

    TRC(("copy to %#lx size %#lx base %#lx\n", loc, size, base));
    
    if (!bfd_get_section_contents(abfd, sec, loc, 0, size))
	fatal("%s: section `%s': contents unavailable.\n",
	      bfd_get_filename(abfd), bfd_get_section_name(abfd, sec));
}

/* ---------------------------------------------------------------------- */

struct namelist *find_namespace_by_name(char *name)
{
    int i;
    struct nameslist *l = global_namespaces;
    struct namelist *n;

    if (!l) return NULL;
    
    for(i=0; i<l->bnnl_num; i++)
	if (!strcmp(name, (n = &l->bnnl_list[i])->bnl_name))
	    return n;
    return NULL;
}

/* ---------------------------------------------------------------------- */

/* This function determins if the linker doesnt need to be
 * called. That is that the .run file generated from the .o on some
 * previous incarnation is still up to date and linked for the correct
 * address. */

static int is_run_ok(char *run, struct link *link, long mtime, bfd **pbfd)
{
    bfd		*bfd;
    struct link	link2;
    
    memset(&link2, 0, sizeof(link2)); 

    /* sec_fill_in_sizes will not fill in for missing segments */
    if (!link->tsize) link2.taddr = link->taddr;
    if (!link->dsize) link2.daddr = link->daddr;
    if (!link->bsize) link2.baddr = link->baddr;
    
    if (!(bfd = try_open_bfd_read(run)))
	return 0;
    TRC(("...try_open_bfd_read succeded"));
    if (bfd_get_mtime(bfd) < mtime)
	goto no;			/* .run is older */
    TRC(("...mtime test succeded..."));
    bfd_map_over_sections(bfd, sec_fill_in_sizes, &link2);

    if (1) {
	/* XXX Horrible hack - if netbsd then do not check the bases
	 * -- assume they are OK. */
	if (!strcmp(machine->mc_bfd_target, "a.out-arm-netbsd"))
	{
	    if (   (link->tsize != link2.tsize)
		|| (link->dsize != link2.dsize)
		|| (link->bsize != link2.bsize))
	    {
		TRC(("hacked memcmp test failed!\n"));
		TRC(("text size is %lx vs %lx\n", link2.tsize, link->tsize));
		TRC(("data size is %lx vs %lx\n", link2.dsize, link->dsize));
		TRC(("bss  size is %lx vs %lx\n", link2.bsize, link->bsize));
		goto no;
	    }
	}
	else 
	{
	    if (memcmp(link, &link2, sizeof(struct link)))
	    {
		TRC(("memcmp test failed!\n"));
		TRC(("text size is %lx vs %lx\n", link2.tsize, link->tsize));
		TRC(("data size is %lx vs %lx\n", link2.dsize, link->dsize));
		TRC(("bss  size is %lx vs %lx\n", link2.bsize, link->bsize));
		TRC(("text addr is %lx vs %lx\n", link2.taddr, link->taddr));
		TRC(("data addr is %lx vs %lx\n", link2.daddr, link->daddr));
		TRC(("bss  addr is %lx vs %lx\n", link2.baddr, link->baddr));
		
		goto no;	/* different somehow, wierd */
	    }
	}
    }
	
    TRC(("...memcmp test succeded..."));
    *pbfd = bfd;
    return 1;

 no:
    /* comment this out cos it might be causing problems 
       bfd_close(bfd);
       */
    return 0;
}

/* ---------------------------------------------------------------------- */

void myunlink(const char *const name)
{
    /* we call remove rather than unlink because it has a prototype */
    if (remove(name) < 0 && errno != ENOENT)
	fatal("unlink: %s: %s\n", name, strerror(errno));
}

static void mysystem(char *cmd)
{
    int rc;
    
    printf("    %s\n", cmd);
    fflush(stdout);
    switch(rc = system(cmd))
    {
    case 0:
	break;
    case -1:
	fatal("system: %s\n", strerror(errno));
    default:
	fatal("child gave exit status %d\n", rc);
    }
}

/* ---------------------------------------------------------------------- */

bfd *perform_link(bfd *inbfd, struct link *addrs, int is_ntsc)
{
    char		*input = bfd_get_filename(inbfd);
    char		cmd[1024];
    char		run[1024];
    char		*path;
    char		*run2;
    bfd			*runbfd;
    int                 ret;

    path = strrchr(input, '/');
    if (!path) path=input; else path++;
    
    TRC(("Peforming link\n"));
    sprintf(run, "%s.%lx-%lx-%lx.run", path,
	    addrs->taddr, addrs->daddr, addrs->baddr);
    
    run2 = strdup(run);		/* since run on stack! */

    if (is_ntsc)
    {
	sprintf(cmd, "ln -sf %s %s", input, run);
    }
    else
    {
	
#if defined(__osf__)
	sprintf(cmd, "ld -N -T %lx -D %lx -B %lx -o %s %s",
		addrs->taddr, addrs->daddr, addrs->baddr, run, input);

#elif defined(hpux)
    sprintf(cmd, "ldarm -defsym _start=0 -N -Ttext %lx -Tdata %lx -Tbss %lx -o %s %s",
	    addrs->taddr, addrs->daddr, addrs->baddr, run, input);

#elif defined(__NetBSD__)

    assert(addrs->daddr + addrs->dsize == addrs->baddr);
    
    sprintf(cmd, "ld -N -Ttext %lx -Tdata %lx -o %s %s",
	    addrs->taddr, addrs->daddr, run, input);

#elif defined(linux_for_arm)

    sprintf(cmd, "arm-unknown-coff-ld -defsym _start=0 -N -Ttext %lx -Tdata %lx -Tbss %lx -o %s %s",
	    addrs->taddr, addrs->daddr, addrs->baddr, run, input);

#else 
    sprintf(cmd, "ld -defsym _start=0 -N -Ttext %lx -Tdata %lx -Tbss %lx -o %s %s",
	    addrs->taddr, addrs->daddr, addrs->baddr, run, input);
#endif
    }

    /* write the entry for the gdb script file */
    if (gdbfile)
    {
	fprintf(gdbfile, "add-symbol-file %s %#x\n", run, addrs->taddr);
	fprintf(gdbfile, "echo \\ %s\n", path);
    }
    
    /* see if .run file exists and is sensible */
    TRC(("Calling is_run_ok\n"));
    ret = is_run_ok(run2, addrs, bfd_get_mtime(inbfd), &runbfd);

    if (ret)
    {
        TRC(("Skipping link; .run file already exists\n"));
	printf("#   %s\n", cmd);
    }
    else
    {
	if (blowruns)
	{
	    TRC(("unlink..."));
	    sprintf(run, "rm -f %s.*.run", path);
	    mysystem(run);
	}
	TRC(("invoke\n"));
	mysystem(cmd);
	TRC(("...check\n"));
	if (!(is_run_ok(run2, addrs, bfd_get_mtime(inbfd), &runbfd)))
	    fatal("unexpected changes or failure during linking!\n");
    }
    return runbfd;
}

/* ---------------------------------------------------------------------- */
/* This function writes all the appropriate sections of the bfd to the
 * in memory copy of the target file. */

void write_out_bfd(bfd *bfd, void *base)
{
    bfd_map_over_sections(bfd, sec_write_out, base);
}

/* ---------------------------------------------------------------------- */

/* On netbsd bfd_get_section_vma systematically lies. Instead we
 * calculate what should be done and then assume the linker has done
 * the right thing. */

static struct link *netbsd_hack;

static void sec_write_out_netbsd(bfd *abfd, sec_ptr sec, char *base)
{
    flagword		 flags;
    void		*loc;
    bfd_size_type	 size;
    char		*name;
    
    sec_return_if_irrelevant(abfd, sec, flags);

    name = bfd_get_section_name(abfd, sec);
    if (!strcmp(name, ".text"))
	loc = base + netbsd_hack->taddr;
    else if (!strcmp(name, ".data"))
	loc = base + netbsd_hack->daddr;
    else if (!strcmp(name, ".bss"))
	loc = base + netbsd_hack->baddr;
    else
	fatal("sec_write_out_netbsd: section %s unknown\n", name);
    
    size = bfd_get_section_size(sec);

    TRC(("copy to %#lx size %#lx base %#lx\n", loc, size, base));
    
    if (!bfd_get_section_contents(abfd, sec, loc, 0, size))
	fatal("%s: section `%s': contents unavailable.\n",
	      bfd_get_filename(abfd), bfd_get_section_name(abfd, sec));
}

void write_out_bfd_netbsd(void *bfd, void *base, struct link *believe)
{
    netbsd_hack = believe;
    
    bfd_map_over_sections(bfd, sec_write_out_netbsd, base);

    netbsd_hack = NULL;
}

/* End of misc.c */
