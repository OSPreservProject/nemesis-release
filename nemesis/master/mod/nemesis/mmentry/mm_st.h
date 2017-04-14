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
**      mod/nemesis/mmentry/mm_st.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Shared type definitions and includes for both mmentry 
**	and mmnotify. 
** 
** ENVIRONMENT: 
**
**      User space.
**
*/

#include <nemesis.h>
#include <VPMacros.h>

#include <autoconf/memsys.h>

#include <Mem.ih>
#include <GDB.ih>
#include <GDBMod.ih>
#include <ThreadF.ih>
#include <MMEntry.ih>
#include <MMNotify.ih>
#include <ChannelNotify.ih>
#include <StretchDriver.ih>

typedef struct _link_t {
    struct _link_t *next;
    struct _link_t *prev;
} link_t;

typedef struct _MMEntry_st {
    
    /* Closures */
    struct _MMEntry_cl 	entry_cl;

    /* Useful handles */
    MMNotify_clp	mmnfy;
    ThreadF_clp         thdf;
    Heap_clp            heap;

    /* Queues */
    link_t	     	idle;     /* Queue of free request structures     */
    link_t	     	pending;  /* Queue of requests pending service    */

    link_t              pool;     /* Queue of available (blocked) workers */
             
    uint32_t            maxf;     /* Max number of faults we can handle */
} MMEntry_st;



/* Struct containing info about the fault satisfaction request. */
typedef struct _fault_t {
    link_t        link;	              /* Must be first in structure    */
    MMEntry_Info  info;               /* Info about the fault          */
    Thread_cl    *thread;             /* Faulting thread               */
    bool_t        incs;               /* Was thd in a CS when faulted? */ 
} fault_t;

typedef struct _worker_t {
    link_t      link;                 /* Must be first in structure */
    Thread_cl  *thread;               /* Handle on worker thread    */
    MMEntry_st *mm_st;                /* Pointer back to mmentry st */
} worker_t; 


extern const char *const reasons[]; 

/*
** For now we define TNV, ACV, FOE, UNA and INST as 'unrecoverable'
** faults. We can't reasonably hope to continue the execution of
** the faulting thread, and instead we'll terminate it or enter
** a user-level debugger. 
** This set of faults is \emph{not} supposed to be written in 
** stone, however - it is quite possible that e.g. a INST fault could
** actually signal some form of break/checkpoint or hook; UNA might
** trigger a software library routine; etc, etc.
*/
#define UNRECOVERABLE(_code) (					\
    ((_code)==Mem_Fault_TNV) || ((_code)==Mem_Fault_ACV) ||	\
    ((_code)==Mem_Fault_FOE) || ((_code)==Mem_Fault_UNA) ||	\
    ((_code)==Mem_Fault_INST)					\
)

#define CASE_UNRECOVERABLE				\
    case Mem_Fault_TNV:					\
    case Mem_Fault_ACV:					\
    case Mem_Fault_FOE:					\
    case Mem_Fault_UNA:					\
    case Mem_Fault_INST			

/*
** Similarly, we define FOR, FOW and BPT as 'recoverable' - 
** intend to restart the faulting thread after we've dealt
** with the fault [PAGE is also in this class, but considered
** a special case]. 
** This is since FOR/FOW seem most likely to be used for 
** reference counting / garbage collection / etc, with 
** BPT being used in the traditional manner. 
** Again, their use for other reasons is acceptable. 
*/
#define RECOVERABLE(_code) (					\
    ((_code)==Mem_Fault_FOR) || ((_code)==Mem_Fault_FOW) ||	\
    ((_code)==Mem_Fault_BPT)				       

#define CASE_RECOVERABLE				\
    case Mem_Fault_FOR:					\
    case Mem_Fault_FOW:					\
    case Mem_Fault_BPT			


