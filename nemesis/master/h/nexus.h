/*
 *      h/nexus.h
 *      ---------
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * This header defines the nexus data structures.
 *
 * XXX This header needs changing whenever nembuild's idea of what the
 * nexus is is altered. Perhaps it should be shared somehow between
 * the nemesis main and tool trees?
 *
 * $Id: nexus.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 */

#ifndef _nexus_h
#define _nexus_h

#include <nemesis.h>
#include <stddef.h>
#include <Time.ih>

#define BOOTFLAG_X 0x01  /* domain gets eXtra time */
#define BOOTFLAG_K 0x02  /* domain has Kernel privileges */
#define BOOTFLAG_B 0x04  /* domain gets started automatically on boot */

struct params {
    Time_ns     p, s, l;
    long        astr;     /* activation stretch size, in words */
    long	stack;    /* default stack size, in words      */
    long	heap;     /* pervasive heap size, in words     */
    int		nctxts;   /* number of context slots           */
    int		neps;     /* number of endpoints               */
    int		nframes;  /* size of initial frame stack       */
    int		flags;    /* some of BOOTFLAG_* or'ed together */
};


enum nexus_info {
    nexus_primal,
    nexus_ntsc,
    nexus_nexus,
    nexus_module,
    nexus_namespace,
    nexus_program,
    nexus_EOI,
    nexus_blob
};

struct nexus_primal {
    enum nexus_info	tag;
    size_t		entry;         /* address to jump to to enter Primal */
    addr_t		namespace;     /* address of Primal's namespace */
    size_t		firstaddr;     /* address of first page of image */
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

struct nexus_blob {
    enum nexus_info	tag;
    size_t 		base;	   /* base of the blob section */
    size_t		len;	   /* length of it */
};

struct nexus_name {
    enum nexus_info	tag;
    size_t		naddr;
    size_t		nsize;
    int			refs;
    int			nmods;
    struct nexus_module	*mods[0];		/* really [nmods] */
};

typedef struct _namespace_entry namespace_entry;

struct _namespace_entry {
  char *name;
  void *ptr;
};

#define MAX_PROGNAME_LEN 32
struct nexus_prog {
    enum nexus_info	tag;
    size_t		taddr, daddr, baddr;
    size_t		tsize, dsize, bsize;
    char        program_name[MAX_PROGNAME_LEN]; /* XXX smh22: hack hack hack */
    struct nexus_name	*name;
    struct params	params;
};

typedef struct nexus_prog nexus_prog;

struct nexus_EOI {
    enum nexus_info	tag;
    size_t 		lastaddr;  /* last address used by the image */
    char		image_ident[80];
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

extern struct nexus_primal *get_primal_info(void);
extern void get_nexus_ident(void);

#endif /* _nexus_h */
