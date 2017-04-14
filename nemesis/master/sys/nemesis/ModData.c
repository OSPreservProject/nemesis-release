/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      ModData.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implements a service to allow the loader to register data about
**      loaded modules, munging the data into a form parseable by gdb
**
**      NB this is a HACK
** 
** ENVIRONMENT: 
**
**      A server in the Nemesis domain
** 
** ID : $Id: ModData.c 1.2 Thu, 18 Feb 1999 14:17:29 +0000 dr10009 $
** 
** */

#include <autoconf/module_offset_data.h>
#ifdef CONFIG_MODULE_OFFSET_DATA

#include <stdio.h>
#include <nemesis.h>
#include <exceptions.h>
#include <ntsc.h>
#include <mod_data.h>

#include <ModData.ih>

#include <pip.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

static  ModData_Register_fn             ModData_Register_m;

static  ModData_op      ms = {
  ModData_Register_m
};

ModData_clp ModDataInit(void)
{
    mod_data_rec *m;
    ModData_clp clp;

    clp=Heap$Malloc(Pvs(heap), sizeof(*clp));
    CLP_INIT(clp,&ms,NULL);
    m=Heap$Malloc(Pvs(heap), sizeof(*m));
    
    m->max_entries = 0;
    m->num_entries = 0;
    m->entries = Heap$Malloc(Pvs(heap), 0);

    INFO_PAGE.mod_data = m;
    return clp;
}

/*---------------------------------------------------- Entry Points ----*/

static void ModData_Register_m (
        ModData_cl      *self,
        string_t        name    /* IN */,
        word_t  offset  /* IN */ )
{
    mod_data_rec *m;
    mod_data_entry *e;

    TRC(fprintf(stderr, "ModData: Adding module %s, address %p\n", name, 
		offset));
    
    if (!INFO_PAGE.mod_data) {
	eprintf("ModData: no module data rec\n");
	return;
    }

    m = INFO_PAGE.mod_data;

    if (m->max_entries == m->num_entries) {

	TRC(fprintf(stderr, "ModData: Expanding data area\n"));

	m->max_entries += 10;
	
	m->entries = Heap$Realloc(Pvs(heap), 
				  m->entries, 
				  m->max_entries * sizeof(*m->entries));

    }
    
    e = &m->entries[m->num_entries++];

    e->name = strdup(name);
    e->offset = offset;
}
#endif CONFIG_MODULE_OFFSET_DATA
