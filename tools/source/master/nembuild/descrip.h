/*
 *	descrip.h
 *	---------
 *
 * $Id: descrip.h 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory
 * Copyright (c) 1996 Richard Black
 * Right to use is administered by University of Cambridge Computer Laboratory
 *
 */

/* ------------------------------------------------------------ */

struct ntsc;

#ifdef __alpha
typedef long nanoseconds;
#else
#ifndef __GNUC__
#error Mothys choice of Nanoseconds means you must have a 64BIT type
#else
typedef long long nanoseconds;
#endif
#endif

typedef enum {False, True} bool_t;

extern FILE *depend_output_file;


extern int str2flags(const char *str);

struct params {
    nanoseconds	p, s, l;
    long 	astr;
    long	stack;
    long	heap;
    int		nctxts;
    int		neps;
    int		nframes;
    int		flags;
};

/* ---------- */

struct simplemod {
    char	*name;
    char	*path;
};

struct simpleblob {
    char	*name;
    char	*path;
    int		 align;
};

struct module;

struct modlist {
    int			  bml_num;
    struct module	* bml_list;
};

/* ---------- */

#define NAME_TYPE_SYMBOL	0
#define NAME_TYPE_BLOB		1

struct simplename {
    char	*path;
    char	*mod;
    int		type;
    const char	*symbol;
};

struct blob;

struct bloblist {
    int			  bbl_num;
    struct blob		* bbl_list;
};


struct name;

struct nexus_name;
struct namelist {
    char	* bnl_name;
    int		  bnl_num;
    int		  bnl_refs;
    int		  bnl_space;		/* total space required for strings */
    unsigned long bnl_addr;
    struct nexus_name	*bnl_nexus;	/* nexus usage */
    struct name	* bnl_list;
};

/* ---------- */

struct simpleprog {
    bool_t	 is_primal;
    char	*binary;
	char	*name;		/* XXX smh22 hacking */
    char	*names;
    struct params *params;
};

struct prog;

struct proglist {
    int		  bpl_num;
    struct prog	* bpl_list;
};

/* ---------- */
struct nameslist {
    int		  	 bnnl_num;
    struct namelist	*bnnl_list;
};

/* ------------------------------------------------------------ */

extern struct ntsc	* parse_ntsc	(char *);
extern struct modlist	* parse_module	(struct modlist *,
					  struct simplemod *);
extern struct namelist	* parse_name	(struct namelist *,
					 struct simplename *);
extern struct proglist	* parse_program	(struct proglist *list,
					 struct simpleprog *s);
extern struct nameslist	* parse_names	(struct nameslist *,
					 struct namelist *);

extern void *xmalloc(size_t);

extern void make_image(struct ntsc *ntsc, struct modlist *modlist,
		       struct nameslist *nameslist, struct proglist *proglist);

/* End of descrip.h */
