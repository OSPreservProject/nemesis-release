/*
 *      ntsc/generic/readnexus.c
 *      ------------------------
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * This file reads the first record out of the nexus and makes sure it
 * is plausable. It then returns a pointer to "struct nexus_primal"
 * record.
 *
 * $Id: readnexus.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 */

#include <kernel.h>
#include <nexus.h>

/* Uncomment the line below to print the nexus to the serial line on boot */
/* #define NEXUS_DEBUG */

/* _end is a symbol invented by the linker. _end is invariant
 * irrespective of how the C namespace is prepended. So this is the
 * best way to refer to it */

char *k_ident="";

extern unsigned long END __asm__("_end");

#ifdef NEXUS_DEBUG
static void output_nexus_primal(struct nexus_primal *n)
{
    k_printf("nexus_primal at %p:\n", n);
    k_printf("  entry=%p; namespace=%p; lastaddr=%p\n",
	     n->entry, n->namespace, n->lastaddr);
}
static void output_nexus_ntsc(struct nexus_ntsc *n)
{
    k_printf("nexus_ntsc at %p:\n", n);
    k_printf("  taddr=%p  daddr=%p  baddr=%p\n",
	     n->taddr, n->daddr, n->baddr);
    k_printf("  tsize=%p  dsize=%p  bsize=%p\n",
	     n->tsize, n->dsize, n->bsize);
}
static void output_nexus_nexus(struct nexus_nexus *n)
{
    k_printf("nexus_nexus at %p:\n", n);
    k_printf("  addr=%p  size=%p\n", n->addr, n->size);
}
static void output_nexus_module(struct nexus_module *n)
{
    k_printf("nexus_module at %p:\n", n);
    k_printf("  addr=%p  size=%p  refs=%p\n",
	     n->addr, n->size, n->refs);
}
static void output_nexus_namespace(struct nexus_name *n)
{
    int i;
    k_printf("nexus_name at %p:\n", n);
    k_printf("  naddr=%p  nsize=%p  refs=%p  nmods=%i\n",
	     n->naddr, n->nsize, n->refs, n->nmods);
    /* mod pointers here */
    for (i=0; ((void **)(n->naddr))[2*i]; i++) {
	k_printf("  Symbol name: %s  Address: %p",
		 ((void **)(n->naddr))[2*i], ((void **)(n->naddr))[2*i+1]);
	k_printf("  module: ");
	output_nexus_module(n->mods[i]);
    }
}
static void output_nexus_program(struct nexus_prog *n)
{
    k_printf("nexus_prog at %p:\n", n);
    k_printf("  taddr=%p  daddr=%p  baddr=%p\n",
	     n->taddr, n->daddr, n->baddr);
    k_printf("  tsize=%p  dsize=%p  bsize=%p\n",
	     n->tsize, n->dsize, n->bsize);
    k_printf("  name=`%s'\n", n->program_name);
    k_printf("  program namespace: ");
    output_nexus_namespace(n->name);
    k_printf("  program params: ");
    k_printf("  p=%t, s=%t, l=%t\n",
	     n->params.p, n->params.s, n->params.l);
}
static void output_nexus_EOI(struct nexus_EOI *n)
{
    k_printf("nexus_EOI at %p:\n", n);
    k_printf("  lastaddr=%p\n", n->lastaddr);
}
static void output_nexus_blob(struct nexus_blob *n)
{
    k_printf("nexus_blob at %p:\n", n);
    k_printf("  base=%p  len=%p\n",
	     n->base, n->len);
}


/* Work through the nexus, outputting it */
void output_nexus(void)
{
    nexus_ptr_t n;
    char *nexus_start;
    void *nexus_end;
    bool_t seen_eoi;
    /* If the compiler doesn't complain about this then I'll eat my lunch. */
    nexus_start=&END;
    n=nexus_start;

    nexus_end=nexus_start+FRAME_SIZE;	/* Upper bound */
    seen_eoi = 0;
    k_printf("Nexus starts at %p.\n",nexus_start);

    while (n.generic < nexus_end && !seen_eoi) {
	switch (*n.tag) {
	case nexus_primal:
	    output_nexus_primal(n.nu_primal);
	    n.nu_primal++;
	    break;
	case nexus_ntsc:
	    output_nexus_ntsc(n.nu_ntsc);
	    n.nu_ntsc++;
	    break;
	case nexus_nexus:
	    output_nexus_nexus(n.nu_nexus);
	    nexus_end=n.nu_nexus->size+nexus_start;
	    n.nu_nexus++;
	    break;
	case nexus_namespace:
	    output_nexus_namespace(n.nu_name);
	    n.generic=(char *)n.generic + n.nu_name->nmods*sizeof(void *);
	    n.nu_name++;
	    break;
	case nexus_module:
	    output_nexus_module(n.nu_mod);
	    n.nu_mod++;
	    break;
	case nexus_blob:
	    output_nexus_blob(n.nu_blob);
	    n.nu_blob++;
	    break;	    
	case nexus_program:
	    output_nexus_program(n.nu_prog);
	    n.nu_prog++;
	    break;
	case nexus_EOI:
	    output_nexus_EOI(n.nu_EOI);
	    n.nu_EOI++;
	    seen_eoi = 1;
	    break;
	default:
	    k_panic("Unknown entry in nexus\n");
	}
    }
    k_printf("End of nexus.\n");
}
#endif /* NEXUS_DEBUG */


struct nexus_primal *get_primal_info(void)
{
    nexus_ptr_t n;

#ifdef NEXUS_DEBUG
    output_nexus();
#endif /* NEXUS_DEBUG */

    /* The compiler doesn't complain about this ;-) */
    n.generic=&END;

    if (*n.tag != nexus_primal)
	k_panic("First entry in nexus isn't a nexus_primal record!\n");

    return n.nu_primal;
}

void get_nexus_ident(void)
{
    nexus_ptr_t n;
    char *nexus_start;
    void *nexus_end;
    bool_t seen_eoi;

    n.generic=&END;
    nexus_start=(char *)n.generic;
    nexus_end=(char *)n.generic+FRAME_SIZE;	/* Upper bound */
    seen_eoi = 0;

    while (n.generic < nexus_end && !seen_eoi) {
	switch (*n.tag) {
	case nexus_primal:
	    n.nu_primal++;
	    break;
	case nexus_ntsc:
	    n.nu_ntsc++;
	    break;
	case nexus_nexus:
	    nexus_end=n.nu_nexus->size+nexus_start;
	    n.nu_nexus++;
	    break;
	case nexus_namespace:
	    n.generic=(char *)n.generic + n.nu_name->nmods*sizeof(void *);
	    n.nu_name++;
	    break;
	case nexus_module:
	    n.nu_mod++;
	    break;
	case nexus_blob:
	    n.nu_blob++;
	    break;	    
	case nexus_program:
	    n.nu_prog++;
	    break;
	case nexus_EOI:
	    k_ident=n.nu_EOI->image_ident;
	    return;
	    n.nu_EOI++;
	    seen_eoi = 1;
	    break;
	default:
	    k_panic("Unknown entry in nexus\n");
	}
    }
    return;
}

/* end */
