/*
 * HISTORY
 */

/*
 * Definitions for nemboot program
 */
#define PAGSIZ          (1<<13)         /* page size for today          */

typedef unsigned long uint64_t;


typedef enum {
	False, 
	True
} bool_t;

typedef struct _neminfo {
	uint64_t    tsize;          /* text size in bytes, padded to DW bdry*/
	uint64_t    dsize;          /* initialized data "  "                */
	uint64_t    bsize;          /* uninitialized data "   "             */
	uint64_t    entry;          /* entry 								*/
	uint64_t    text_start;     /* base of text used for this file      */
	uint64_t    data_start;     /* base of data used for this file      */
	uint64_t    bss_start;      /* base of bss used for this file       */
	uint64_t	image;			/* pointer to the image in memory		*/
	uint64_t 	length;			/* total len (padded to page boundary) 	*/
} neminfo;


#define ALIGN_PAGE(l)  (((l) + (PAGSIZ-1)) & ~(PAGSIZ-1))
#define ALIGN_PAL(l)   (((l) + ((PAGSIZ<<1)-1)) & ~((PAGSIZ<<1)-1))
#define IMAGE_LEN(ni)  (ni->tsize + ni->dsize + ni->bsize)
#define FILE_LEN(ni)   (ni->tsize + ni->dsize)


/* XXX below are horrible hard-wirings, but will do for now */
#define BOOT_VA         (0x20000000)
#define BOOT_PA         (0x200000)
#define NEMESIS_PA      (0x220000)

#include <mach/std_types.h>

#if __osf__
typedef long integer_t;
#endif

struct exec {
	integer_t a_magic;	/* Use macros N_MAGIC, etc for access */
	vm_size_t a_text;	/* bytes of text in file */
	vm_size_t a_data;	/* bytes of data in file */
	vm_size_t a_bss;	/* bytes of auto-zeroed data */
	vm_size_t a_syms;	/* bytes of symbol table data in file */
	vm_offset_t a_entry;	/* start PC */
	vm_offset_t a_tstart;	/* text start, in memory */
	vm_offset_t a_dstart;	/* data start, in memory */
	vm_size_t a_trsize;	/* bytes of text-relocation info in file */
	vm_size_t a_drsize;	/* bytes of data-relocation info in file */
};

#define	__LDPGSZ	8192

#ifndef	OMAGIC
#define OMAGIC 0407
#define NMAGIC 0410
#define ZMAGIC 0413
#endif

#define EXEC_N_BADMAG(x) \
 (((x).a_magic)!=OMAGIC && ((x).a_magic)!=NMAGIC && ((x).a_magic)!=ZMAGIC)

/* Address of the bottom of the text segment. */
#define EXEC_N_TXTADDR(x) \
	((x).a_tstart)

/* Address of the bottom of the data segment. */
#define EXEC_N_DATADDR(x) \
	((x).a_dstart)

/* Text segment offset. */
#define	EXEC_N_TXTOFF(ex) \
	((ex).a_magic == ZMAGIC ? 0 : sizeof(struct exec))

/* Data segment offset. */
#define	EXEC_N_DATOFF(ex) \
	((EXEC_N_TXTOFF(ex)) + ((ex).a_magic != ZMAGIC ? (ex).a_text : \
	__LDPGSZ + ((ex).a_text - 1 & ~(__LDPGSZ - 1))))


 
