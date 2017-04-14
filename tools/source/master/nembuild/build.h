/*
 *	build.h
 *	-------
 *
 * $Id: build.h 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory
 * Copyright (c) 1996 Richard Black
 * Right to use is administered by University of Cambridge Computer Laboratory
 *
 */

#ifndef bfd_strerror
#define bfd_strerror (bfd_errmsg(bfd_get_error()))
#endif

#ifndef bfd_asymbol_flags
#define bfd_asymbol_flags(x) ((x)->flags)
#endif

#define TRC(x) do { if (dflg) printf x ; } while(0)

extern int dflg;
extern int blowruns;
extern FILE *gdbfile;

/* Endianness compatability macros:
 * TE32(x) returns 32-bit x in the target endianness
 */
#define little_end 0
#define big_end    1

#ifdef hpux
#define HOST_ENDIAN   big_end
#define TARGET_ENDIAN little_end
#else
#define HOST_ENDIAN   little_end
#define TARGET_ENDIAN little_end
#endif

#if HOST_ENDIAN != TARGET_ENDIAN
/* bogus if this isn't a 32-bit architecture */
/* TE32() - always 32 bits: */
#define TE32(x) ((((unsigned int)(x) & 0xff000000) >> 24) |	\
                 (((unsigned int)(x) & 0x00ff0000) >>  8) |	\
		 (((unsigned int)(x) & 0x0000ff00) <<  8) |	\
		 (((unsigned int)(x) & 0x000000ff) << 24))
/* XXX TElong() - a "long" - ie 32 bits on most things, 64 bits on Alpha */
#define TElong(x) TE32(x)
#define TEsize(x) TE32(x) /* a size_t */
#define TEaddr(x) TE32(x) /* an addr_t */
#define TE64(x) ((((unsigned long long)(x) & 0xff00000000000000ULL) >> 56) | \
		 (((unsigned long long)(x) & 0x00ff000000000000ULL) >> 40) | \
		 (((unsigned long long)(x) & 0x0000ff0000000000ULL) >> 24) | \
		 (((unsigned long long)(x) & 0x000000ff00000000ULL) >>  8) | \
		 (((unsigned long long)(x) & 0x00000000ff000000ULL) <<  8) | \
		 (((unsigned long long)(x) & 0x0000000000ff0000ULL) << 24) | \
		 (((unsigned long long)(x) & 0x000000000000ff00ULL) << 40) | \
		 (((unsigned long long)(x) & 0x00000000000000ffULL) << 56))
#else
#define TE32(x)   (x)
#define TElong(x) (x)
#define TEsize(x) (x)
#define TEaddr(x) (x)
#define TE64(x)   (x)
#endif

struct link {
    /* you might think we can use size_t here but we cant because
     * size_t is an int on ix86_linux and long on OSF1 (and
     * presumably!) on alpha_linux, but the printfs that use these
     * must be %lx (etc) just in case its an alpha which gives lots of
     * tedious warnings on the ix86_linux platform. */
    bfd_size_type	tsize, dsize, bsize;
    bfd_vma		taddr, daddr, baddr;
};

struct ntsc {
    char	* bk_binary;
    bfd		* bk_bfd;
    struct link	  bk;
};

struct nexus_module;

struct module {
    char	* bm_binary;	/* the name of the relocatable module */
    char	* bm_name;	/* the name used within the namespaces
				 * in this description file */
    int		  bm_refs;	/* number of references within the
				 * decription to this module. If none
				 * will be elided and a warning
				 * given. */
    bfd		* bm_bfd;	/* The bfd for the input module */
    struct link	  bm;
    long	  bm_n_symbols;
    asymbol	**bm_symbols;
    /* XXX more */
    struct nexus_module *bm_nexus;	/* nexus usage */
};


struct blob {
    char	* bb_binary;	/* the name of the relocatable module */
    char	* bb_name;	/* the name used within the namespaces
				 * in this description file */
    int		  bb_align;	/* alignement constraint */
    bfd_vma	  bb_index;	/* pointer to blob descriptor in image */
    int		  bb_refs;	/* number of references within the
				 * decription to this module. If none
				 * will be elided and a warning
				 * given. */
    int		  bb_fd;	/* fd open to read blob data */
    bfd_vma	  bb_vma;	/* 0, or base of blob */
    size_t	  bb_len;	/* length of the blob */
};


struct name {
    char	* bn_path;
    char	* bn_modname;
    const char	* bn_symbol;
    struct module * bn_modptr;
    int		  bn_type;
    /* XXX more */
};

struct nexus_prog;
struct prog {
    char		* bp_binary;
    struct namelist	* bp_names;
    struct params	  bp_params;
    bfd			* bp_bfd;
    struct link		  bp;
    struct nexus_prog	* bp_nexus;
	char   *name; /* XXX hack hack hack yuk */
    /* XXX more */
};

extern struct modlist	*global_modlist;
extern struct bloblist	*global_bloblist;
extern struct proglist	*global_proglist;	/* XXX needed? */
extern struct nameslist	*global_namespaces;

/* Functions in misc.c */

extern void		  myunlink (const char *const name);
extern bfd		* open_bfd_read			(char *binary);
extern struct module	* find_module_by_name		(char *name);
extern struct blob	* find_blob_by_name		(char *name);
extern int		  check_module_for_symbol	(struct module *m,
							 const char *s);
extern bfd_vma		  get_module_and_symbol (char *name, const char *s);
   

extern void		  sec_is_ro (bfd *abfd, sec_ptr sec, void *v);
extern void		  sec_calc_sizes (bfd *abfd, sec_ptr sec, void *v);
extern void		  sec_fill_in_sizes (bfd *abfd, sec_ptr sec, void *v);
extern struct namelist	* find_namespace_by_name  (char *name);
extern bfd		* perform_link (bfd *inbfd, struct link *addrs,
					int is_ntsc);
extern void		  write_out_bfd (bfd *bfd, void *base);
extern void		  write_out_bfd_netbsd(void *bfd, void *base, 
					       struct link *believe);

/* Functions in build.c */

#if defined(__GNUC__) && !defined(__PEDANTIC_ANSI__)

extern void		  warning	(char *format, ...)
    __attribute__ ((format (printf, 1, 2)));
extern void		  error		(char *format, ...)
    __attribute__ ((format (printf, 1, 2)));

#else

extern void		  warning	(char *format, ...);
extern void		  error		(char *format, ...);

#endif

/* ---------------------------------------------------------------------- */

struct machine {
    const char		* mc_bfd_target;
    const char		* mc_defsymname;
    int			  mc_pageshift;
    size_t		  mc_baseaddr;
    size_t		  mc_bootaddr;
    int			  mc_nlist;
    unsigned		  mc_constructors_grow;
};

extern const struct machine *machine;

#if defined(__alpha__) || defined(__NetBSD__)
#  define OPT_NLIST
#endif

/* End of build.h */
