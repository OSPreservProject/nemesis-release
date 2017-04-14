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
**      mod/nemesis/idc/IDCOfferMod.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Useful IDCOffer operations
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: IDCOfferMod.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <stdio.h>
#include <autoconf/net.h>

#include <IDCOfferMod.ih>
#include <IORConverter.ih>

#ifdef CONFIG_NET
#include <Net.ih>
#endif

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);

#define LOCAL_IOR_CONVERTER_NAME "LocalIORConverter"


static  IDCOfferMod_NewLocalIOR_fn              IDCOfferMod_NewLocalIOR_m;
static  IDCOfferMod_Localise_fn                 IDCOfferMod_Localise_m;
static  IDCOfferMod_NewOffer_fn                 IDCOfferMod_NewOffer_m;
static  IDCOfferMod_NewMerged_fn                IDCOfferMod_NewMerged_m;
static  IDCOfferMod_EncapsulateIORs_fn          IDCOfferMod_EncapsulateIORs_m;
static  IDCOfferMod_ExtractIORs_fn              IDCOfferMod_ExtractIORs_m;
static  IDCOfferMod_DupIORs_fn                  IDCOfferMod_DupIORs_m;

static  IDCOfferMod_op  offermod_ms = {
  IDCOfferMod_NewLocalIOR_m,
  IDCOfferMod_Localise_m,
  IDCOfferMod_NewOffer_m,
  IDCOfferMod_NewMerged_m,
  IDCOfferMod_EncapsulateIORs_m,
  IDCOfferMod_ExtractIORs_m,
  IDCOfferMod_DupIORs_m,
};

static const  IDCOfferMod_cl  offermod_cl = { &offermod_ms, NULL };

CL_EXPORT (IDCOfferMod, IDCOfferMod, offermod_cl);

static  IORConverter_Localise_fn                 LocalIORConverter_Localise_m;

static  IORConverter_op localconverter_ms = {
  LocalIORConverter_Localise_m
};

const static  IORConverter_cl localconverter_cl = { &localconverter_ms, NULL };

CL_EXPORT (IORConverter, LocalIORConverter, localconverter_cl);

static IDCOffer_Type_fn       SurrOffer_Type_m;
static IDCOffer_PDID_fn       SurrOffer_PDID_m;
static IDCOffer_GetIORs_fn    SurrOffer_GetIORs_m;
static IDCOffer_Bind_fn       SurrOffer_Bind_m;

static IDCOffer_op offer_ms = {
    SurrOffer_Type_m,
    SurrOffer_PDID_m,
    SurrOffer_GetIORs_m,
    SurrOffer_Bind_m
};

typedef struct _off_st {
    IDCOffer_cl offer_cl;
    Type_Code   tc;
    const IDCOffer_IORList * iorlist;
} off_st;



/*---------------------------------------------------- Entry Points ----*/

static void IDCOfferMod_NewLocalIOR_m (
        IDCOfferMod_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        IDCOffer_IOR    *ior    /* OUT */ )
{

    addr_t p;

    /* Stamp our Ethernet address and the offer pointer in the IOR */

    ior->transport = strdup(LOCAL_IOR_CONVERTER_NAME);
    SEQ_INIT(&ior->info, 16, Pvs(heap));
    p = SEQ_START(&ior->info);
    
#ifdef CONFIG_NET
    /* XXX This is inelegant */
    TRY {
	*((Net_EtherAddr *)p) = *NAME_FIND("dev>eth0>addr", Net_EtherAddrP);
    } CATCH_Context$NotFound(UNUSED n) {
	/* We don't have the network set up yet ... we won't be able
           to pass this offer to other machines and convert it back
           again, but this is unlikely to be a problem, really. */

	memset(p, 0, 8);
    } ENDTRY;
#endif
    
    *(uint64_t *)(p + 8) = (uint64_t) (pointerval_t) offer;
    
}

#define BUF_SIZE 128

static IDCOffer_clp 
IDCOfferMod_Localise_m (IDCOfferMod_clp self,
			const IDCOffer_IORList  *iorlist  /* IN */,
			Type_Code       tc      /* IN */ )
{

    /* This is the main IOR converter. It checks each IOR in turn by
       looking up a converter for it in "transports>" or "modules>" */

    IORConverter_clp cv;
    char buf[BUF_SIZE];
    
    IDCOffer_IOR *ior;
    int i;
    IDCOffer_clp offer;

    for(i = 0; i < SEQ_LEN(iorlist); i++) {

	Type_Any any;

	ior = &SEQ_ELEM(iorlist, i);

	if(strlen(ior->transport) > (BUF_SIZE - 12)) {
	    fprintf(stderr, "Localise: Transport name %s too long\n", 
		   ior->transport);
	    continue;
	}

	sprintf(buf, "transports>%s", ior->transport);

	TRC(fprintf(stderr, "Localise: Looking for '%s'\n", buf));

	if(Context$Get(Pvs(root), buf, &any) 
	   && ISTYPE(&any, IORConverter_clp)) {
	    cv = NARROW(&any, IORConverter_clp);
	    TRC(fprintf(stderr, "Localise: Trying converter %p\n", cv));
	    offer = IORConverter$Localise(cv, iorlist, i, tc);

	    if(offer) {
		TRC(fprintf(stderr, "Localise: Got offer %p\n", offer));
		return offer;
	    }

	    continue;
	}

	sprintf(buf, "modules>%s", ior->transport);

	TRC(fprintf(stderr, "Localise: Looking for '%s'\n", buf));
	
	if(Context$Get(Pvs(root), buf, &any) 
	   && ISTYPE(&any, IORConverter_clp)) {
	    cv = NARROW(&any, IORConverter_clp);
	    TRC(fprintf(stderr, "Localise: Trying converter %p\n", cv));
	    offer = IORConverter$Localise(cv, iorlist, i, tc);

	    if(offer) {
		TRC(fprintf(stderr, "Localise: Got offer %p\n", offer));
		return offer;
	    }
	}
    }
    
    /* Sorry, can't convert this IORList */

    TRC(fprintf(stderr, "Localise: Can't localise IORList\n"));
    return NULL;
}

static IDCOffer_clp IDCOfferMod_NewOffer_m(IDCOfferMod_clp         self,
					   const IDCOffer_IORList *iorlist,
					   Type_Code               tc) {
    
    off_st *st = Heap$Malloc(Pvs(heap), sizeof(*st));

    if(!st) RAISE_Heap$NoMemory();

    CL_INIT(st->offer_cl, &offer_ms, st);
    st->tc = tc;

    st->iorlist = iorlist;
    
    return &st->offer_cl;
}    
					   
static IDCOffer_clp IDCOfferMod_NewMerged_m(
    IDCOfferMod_clp self,
    const IDCOfferMod_OfferSeq *offers) {

    off_st *st = Heap$Malloc(Pvs(heap), sizeof(*st));
    IDCOffer_IORList *iorlist = NULL, * NOCLOBBER tmplist = NULL;
    IDCOffer_IOR *i;
    IDCOffer_clp o;
    int n;

    if(!st) RAISE_Heap$NoMemory();

    iorlist = SEQ_NEW(IDCOffer_IORList, 0, Pvs(heap));

    TRY {
	if(!SEQ_LEN(offers)) {
	    RAISE_IDCOfferMod$BadList(IDCOfferMod_ListProblem_Empty);
	}
	
	st->tc = IDCOffer$Type(SEQ_ELEM(offers, 0));
	
	for(n = 0; n < SEQ_LEN(offers); n++) {

	    o = SEQ_ELEM(offers, n);
	    
	    if(IDCOffer$Type(o) != st->tc) {
		RAISE_IDCOfferMod$BadList(IDCOfferMod_ListProblem_MixedTypes);
	    }

	    tmplist = IDCOffer$GetIORs(o);

	    for(i = SEQ_START(tmplist); i < SEQ_END(tmplist); i++) {
		
		SEQ_ADDH(iorlist, *i);

	    }

	    SEQ_FREE(tmplist);
	    tmplist = NULL;
	} 

    } CATCH_ALL {
	
	if(tmplist) SEQ_FREE(tmplist);
	if(iorlist) SEQ_FREE(iorlist);
	FREE(st);
	RERAISE;

    } ENDTRY;

    st->iorlist = iorlist;
    return &st->offer_cl;

}
	
	

static void IDCOfferMod_EncapsulateIORs_m (
        IDCOfferMod_cl  *self,
        const IDCOffer_IORList  *iorlist        /* IN */,
        IDCOffer_IORInfo  *info    /* OUT */ )
{

    int datalen = 8;
    IDCOffer_IOR *s;
    addr_t p;

    /* Take a list of IORs, and marshal them into the info portion of
       a single IOR. */

    /* Format used:

       uint64_t        number of encapsulated IORs
       char []         first IOR name (nul terminated, quadword padded)
       uint64_t        first IOR info length
       char []         first IOR data (quadword padded)
       char []         second IOR name ...
       
       */
       
    for(s = SEQ_START(iorlist); s < SEQ_END(iorlist); s++) {
	datalen += (strlen(s->transport) + 8) & ~7;
	
	datalen += 8;
	datalen += (SEQ_LEN(&s->info) + 7) & ~7;

    }

    SEQ_INIT(info, datalen, Pvs(heap));

    p = SEQ_START(info);
    *((uint64_t *)p)++ = SEQ_LEN(iorlist);

    for(s = SEQ_START(iorlist); s < SEQ_END(iorlist); s++) {

	strcpy(p, s->transport);
	p += (strlen(s->transport) + 8) & ~7;

	*((uint64_t *)p)++ = SEQ_LEN(&s->info);
	memcpy(p, SEQ_START(&s->info), SEQ_LEN(&s->info));
	p += (SEQ_LEN(&s->info) + 7) & ~7;

    }

    if(p != SEQ_END(info)) {
	ntsc_dbgstop();
    }
}

static IDCOffer_IORList *IDCOfferMod_ExtractIORs_m (
        IDCOfferMod_cl  *self,
        const IDCOffer_IORInfo      *info    /* IN */ )
{

    /* See comments in IDCOfferMod_EncapsulateIORs */
    IDCOffer_IORList *iorlist;
    IDCOffer_IOR     *s;
    uint32_t numiors;
    addr_t p;
    uint32_t i;


    TRC(fprintf(stderr, "ExtractIORs: extracting from %p\n", info));

    p = SEQ_START(info);

    numiors = *((uint64_t *)p)++;

    TRC(fprintf(stderr, "p = %p\n", p));
    TRC(fprintf(stderr, "Extracting %u iors\n", numiors));

    iorlist = SEQ_NEW(IDCOffer_IORList, numiors, Pvs(heap));

    for(s = SEQ_START(iorlist); s < SEQ_END(iorlist); s++) {
	TRC(fprintf(stderr, "Extracting IOR %p '%s'\n", p, (string_t)p));
	s->transport = strdup(p);
	p += (strlen(s->transport) + 8) & ~7;

	i = *((uint64_t *)p)++;
	TRC(fprintf(stderr, "Extracting %u bytes from %p\n", i, p));

	SEQ_INIT(&s->info, i, Pvs(heap));
	memcpy(SEQ_START(&s->info), p, SEQ_LEN(&s->info));
	p += (SEQ_LEN(&s->info) + 7) & ~7;
    }
    TRC(fprintf(stderr, "ExtractIORs: returning\n"));
    
    return iorlist;

}

static IDCOffer_IORList *IDCOfferMod_DupIORs_m(
    IDCOfferMod_clp self,
    const IDCOffer_IORList *list) {
    
    IDCOffer_IOR *from, *to;

    IDCOffer_IORList *newlist = 
	SEQ_NEW(IDCOffer_IORList, SEQ_LEN(list), Pvs(heap));

    /* Copy each IOR */

    for(to = SEQ_START(newlist), from = SEQ_START(list);
	to < SEQ_END(newlist); to++, from++) {
	
	to->transport = strdup(from->transport);
	SEQ_INIT(&to->info, SEQ_LEN(&from->info), Pvs(heap));
	memcpy(SEQ_START(&to->info), SEQ_START(&from->info), 
	       SEQ_LEN(&from->info) * SEQ_DSIZE(&from->info));
    }
    
    return newlist;
}

/*--IORConverter methods---------------------------------------------*/

static IDCOffer_clp 
LocalIORConverter_Localise_m(IORConverter_clp        self,
			     const IDCOffer_IORList *iorlist,
			     uint32_t                iornum,
			     Type_Code               tc) {

    IDCOffer_IOR *ior;
    IDCOffer_clp offer;
    addr_t p;

    bool_t isLocal;

    /* Simple IOR converter - if it is on this machine and has listed
       Nemesis Shm IDC as a IOR, get the offer pointer from the IOR
       data */

    ior = &SEQ_ELEM(iorlist, iornum);
    
    if(!strcmp(ior->transport, LOCAL_IOR_CONVERTER_NAME)) {
	
	/* This claims to be a Shm Offer, in which case the first
	   6 bytes of IOR data are the ethernet MAC address (yes,
	   I know, it's horrible) */
	
#ifdef CONFIG_NET
	Net_EtherAddrP ethaddr = NAME_FIND("dev>eth0>addr", Net_EtherAddrP);

	TRC(fprintf(stderr, "ShmIORConverter: Checking IOR Ethernet address\n"));

	isLocal = 
	    !memcmp(SEQ_START(&ior->info), ethaddr, sizeof(Net_EtherAddr));
#else
	/* If we don't have the network configured, we'll assume that
           all Shm IORs are necessarily for local offers */

	isLocal = True;
#endif
	if(isLocal) {
	    
	    /* This appears to be from the same machine. Get the
	       closure and see if it is the type it claims to be (we
	       will probably blow up here if anything has gone wrong),
	       but there's not really any other way of checking */
	    
	    TRC(fprintf(stderr, "ShmIORConverter: IOR is local\n"));

	    p = ior->info.data + 8; /* 2 bytes padding for alignment */
	    
	    offer = (IDCOffer_clp) (pointerval_t) *((uint64_t *)p);
	    
	    TRC(fprintf(stderr, "ShmIORConverter: Calling into offer %p\n", offer));

	    if(tc == IDCOffer$Type(offer)) {
		
		/* By jove, I think we've got it ... */
		
		return offer;
		
	    } else {
		
		/* We'd probably blow up before we got here */
		
		TRC(fprintf(stderr, "ShmIORConverter: "
			    "'offer' type %qx, expected %qx\n",
			    IDCOffer$Type(offer), tc));
		
	    }
	} else {
	    
	    TRC(fprintf(stderr, "ShmIORConverter: IOR is not local\n"));
	}
    } else {
	TRC(fprintf(stderr, "ShmIORConverter: Incorrect type '%s'\n", ior->transport));
    }
    
    /* The IOR is not appropriate for turning into an offer */
    
    return NULL;
    
}
		    

/*---------------------------------------------------- Entry Points ----*/


static Type_Code SurrOffer_Type_m(IDCOffer_clp self) {
    off_st *st = self->st;

    return st->tc;

}

static ProtectionDomain_ID SurrOffer_PDID_m(IDCOffer_clp self) {

    off_st *st = self->st;
    IDCOffer_clp offer = IDCOfferMod_Localise_m(IDCOfferMod,
						st->iorlist,
						st->tc);

    if(!offer) RAISE_IDCOffer$UnknownIOR();

    return IDCOffer$PDID(offer);
}

static IDCOffer_IORList *SurrOffer_GetIORs_m(IDCOffer_clp self) {
    off_st *st = self->st;

    IDCOffer_IORList *iorlist = 
	SEQ_NEW(IDCOffer_IORList, SEQ_LEN(st->iorlist), Pvs(heap));

    memcpy(SEQ_START(iorlist), SEQ_START(st->iorlist), 
	   SEQ_DSIZE(st->iorlist) * SEQ_LEN(st->iorlist));

    return iorlist;
}

static IDCClientBinding_clp SurrOffer_Bind_m(IDCOffer_clp self,
					     Gatekeeper_clp gk,
					     Type_Any *any) {

    off_st *st = self->st;
    IDCOffer_clp offer = IDCOfferMod_Localise_m(IDCOfferMod,
						st->iorlist,
						st->tc);


    if(!offer) RAISE_IDCOffer$UnknownIOR();
    
    return IDCOffer$Bind(offer, gk, any);
    
}

/*
 * End 
 */
