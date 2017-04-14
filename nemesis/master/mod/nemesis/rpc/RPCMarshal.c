/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	RPCMarshal.c
**
**
** ID : $Id: RPCMarshal.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
**
*/

#include "RPCTransport.h"

#define TRC(x)
#define DEBUG(x)
#define ASSERT(cond,fail) {if (!(cond)) { {fail}; while(1); }}

/* 
 * Macros
 */

#define ROUNDUP(p,TYPE)	( ((p) + sizeof (TYPE) - 1) & -sizeof (TYPE) )

/*
 * Prototypes
 */

static  IDC_mCustom_fn RPC_mCustom_m;
static  IDC_uCustom_fn RPC_uCustom_m;

IDC_op RPCMarshal_ms = {

    NULL,  /* mBoolean_m, */
    NULL,  /* mShortCardinal_m, */
    NULL,  /* mCardinal_m, */
    NULL,  /* mLongCardinal_m, */
    NULL,  /* mShortInteger_m, */
    NULL,  /* mInteger_m, */
    NULL,  /* mLongInteger_m, */
    NULL,  /* mReal_m, */
    NULL,  /* mLongReal_m, */
    NULL,  /* mOctet_m, */
    NULL,  /* mChar_m, */
    NULL,  /* mAddress_m, */
    NULL,  /* mWord_m, */

    NULL,  /* uBoolean_m, */
    NULL,  /* uShortCardinal_m, */
    NULL,  /* uCardinal_m, */
    NULL,  /* uLongCardinal_m, */
    NULL,  /* uShortInteger_m, */
    NULL,  /* uInteger_m, */
    NULL,  /* uLongInteger_m, */
    NULL,  /* uReal_m, */
    NULL,  /* uLongReal_m, */
    NULL,  /* uOctet_m, */
    NULL,  /* uChar_m, */
    NULL,  /* uAddress_m, */
    NULL,  /* uWord_m, */

    NULL,  /* mString_m, */
    NULL,  /* uString_m, */

    NULL,  /* mByteSequence_m, */
    NULL,  /* uByteSequence_m, */
  
    NULL,  /* mStartSequence_m, */
    NULL,  /* uStartSequence_m, */

    NULL,  /* mSequenceElement_m, */
    NULL,  /* uSequenceElement_m, */
    
    NULL,  /* mEndSequence_m, */
    NULL,  /* uEndSequence_m, */

    RPC_mCustom_m,
    RPC_uCustom_m,

    NULL,  /* mByteArray_m, */
    NULL,  /* uByteArray_m, */

    NULL,  /* mBitSet_m, */
    NULL,  /* uBitSet_m, */

};

/* 
 * Export it
 */

static IDCMarshalCtl_Interface_fn RPCMarshalCtlIntf;
static IDCMarshalCtl_Operation_fn RPCMarshalCtlOp;
static IDCMarshalCtl_Type_fn      RPCMarshalCtlType;

static IDCMarshalCtl_op RPCMarshalCtl_ms = {
    RPCMarshalCtlIntf,
    RPCMarshalCtlOp,
    RPCMarshalCtlType,
};

const IDCMarshalCtl_cl RPCMarshalCtl = { &RPCMarshalCtl_ms, NULL };
CL_EXPORT(IDCMarshalCtl, RPCCtl, RPCMarshalCtl);

static bool_t type_ok(Type_Any *rep) {
    switch(rep->type) {
	
    case TypeSystem_Ref__code:
    case TypeSystem_Iref__code:
	/* We can't pass Refs across RPC */
	return True; /* XXX */
	break;
	
    case TypeSystem_Predefined__code:
	if((rep->val == TypeSystem_Predefined_Address) || 
	   (rep->val == TypeSystem_Predefined_Word)) {
	    return True; /* XXX too many brokennesses to stop words */
	}
	break;

    }
   
    return True;
}

static bool_t RPC_mCustom_m(IDC_clp self,
			    IDC_BufferDesc bd,
			    Type_Code tc,
			    addr_t arg,
			    bool_t shouldFree) {
    
    Interface_clp scope;
    Type_Any rep;
    RPC_st *st = self->st;

    switch(tc) {
    case IDCOffer_clp__code:
    {	
	IDCOffer_clp      offer = *(IDCOffer_clp *)arg;
	Type_Code         offer_type;
	IDCOffer_IORList *iorlist;
	bool_t res;
	eprintf("Got offer %p, arg %p\n", offer, arg);

	/* We need to get the IORList from the offer and marshal it */
	iorlist = IDCOffer$GetIORs(offer);

	offer_type = IDCOffer$Type(offer);

	eprintf("Got IOR list %p\n", iorlist);

	res = IDC_mLongCardinal(self, bd, offer_type);
	
	if(!res) {
	    eprintf("RPCMarshal: Out of space marshalling type\n");
	    return False;
	}

	res = StubGen$Marshal(st->stubgen, st->offerCode, 
			      self, bd, iorlist, True);
	
	if(!res) {
	    eprintf("RPCMarshal: Out of space doing custom marshal\n");
	    return False;
	}
	eprintf("RPCMarshal: OK\n");
	return True;
	break;
    }
    
    case Type_Any__code:
    {
	/* We only allow small non-pointer values and IDCOffers to
           pass in Type.Any structures */

	bool_t NOCLOBBER result = False;
	Type_Any *any = (Type_Any *)arg;
	eprintf("RPCMarshal: Got Type_Any %p (%qx, %qx) ... \n", 
		any, any->type, any->val);

	if(!IDC_mLongCardinal(self, bd, any->type))
	    return False;

	if(any->type == IDCOffer_clp__code) {

	    /* Re use the bit of code above here ... */
	    return RPC_mCustom_m(self, bd, 
				 IDCOffer_clp__code, 
				 &any->val, shouldFree);
	} else {
	    
	    if(TypeSystem$IsLarge(Pvs(types), any->type))
		return False;

	    scope = TypeSystem$Info(Pvs(types), any->type, &rep);

	    if(!type_ok(&rep))
		return False;
	    
	    return IDC_mLongCardinal(self, bd, any->val);

	}
	    
	return result;
	break;
    }

    default:
	ntsc_dbgstop();
	break;
    }
    return False;

}
	     
static bool_t RPC_uCustom_m(IDC_clp self,
			    IDC_BufferDesc bd,
			    Type_Code tc,
			    addr_t arg,
			    IDC_AddrSeq *memlist) {
    
    RPC_st *st = self->st;
    Interface_clp scope;
    Type_Any rep;
    
    switch(tc) {
    case IDCOffer_clp__code:
    {	
	IDCOffer_clp *offerp = (IDCOffer_clp *)arg;
	IDCOffer_IORList *iorlist = Heap$Malloc(Pvs(heap), sizeof(*iorlist));
	Type_Code offer_type;

	eprintf("RPC Unmarshal: Got offer\n");
	
	if(!(IDC_uLongCardinal(self, bd, &offer_type) &&
	     StubGen$Unmarshal(st->stubgen, st->offerCode,
			       self, bd, iorlist, memlist))) {
	    FREE(iorlist);
	    return False;
	}

	eprintf("RPCUnmarshal: Got iorlist %p\n", iorlist);

	*offerp = IDCOfferMod$NewOffer(st->om,
				       iorlist,
				       offer_type);

	eprintf("Got new offer: %p\n", *offerp);
	return (*offerp != NULL);
	break;
    }
    
    case Type_Any__code:
    {
	Type_Any *any = arg;
	eprintf("RPCUnmarshal: Got Type_Any ... \n");

	if(!IDC_uLongCardinal(self, bd, &any->type)) {
	    eprintf("Failed to unmarshal type code\n");
	    return False;
	}

	eprintf("Got type code %qx\n", any->type);

	if(any->type == IDCOffer_clp__code) {
	    
	    return(RPC_uCustom_m(self, bd, IDCOffer_clp__code, 
			       &any->val, memlist));
	} else {
	    
	    if(TypeSystem$IsLarge(Pvs(types), any->type)) 
		return False;

	    scope = TypeSystem$Info(Pvs(types), any->type, &rep);

	    if(!type_ok(&rep))
		return False;

	    return (IDC_uLongCardinal(self, bd, &any->val));
	}
	break;
    }

    default:
	ntsc_dbgstop();
	break;
    }
    return False;

}
	     
	      
	    

	    


static bool_t RPCMarshalCtlIntf(IDCMarshalCtl_clp self,
				Interface_clp intf) {
    
    
    /* Ought to check for LOCAL interfaces here, but they're all
       local, more or less, so we can't */

#if 0
    Type_Code code;
    Interface_Needs *needs;
    bool_t local;

    needs = Interface$Info(intf);

    FREE(needs);
    FREE(name);

    return (!local);
#else
    return True;
#endif

}

static bool_t RPCMarshalCtlOp(IDCMarshalCtl_clp self,
			      Operation_clp op,
			      bool_t * resultsFirst) {
    
    *resultsFirst = False;
    return True;
}

static bool_t RPCMarshalCtlType(IDCMarshalCtl_clp self,
				Type_Code  tc,
				IDCMarshalCtl_Code *code) {

    Interface_clp scope;
    Type_Any rep;

    scope = TypeSystem$Info(Pvs(types), tc, &rep);

    switch(tc) {
    case Type_Any__code:
	/* A Type.Any could contain a REF or IREF, or a large type
           that needs to be transferred - catch it */
	*code = IDCMarshalCtl_Code_Custom;
	return True;
	break;

    case IDCOffer_clp__code:
	/* We need to marshal this as an IORList */
	*code = IDCMarshalCtl_Code_Custom;
	return True;
	break;

    default:
	if(!type_ok(&rep)) {
	    return False;
	}
	break;
    }
    
    *code = IDCMarshalCtl_Code_Native;
    return True;

}



/*
 * End RPCMarshal.c
 */


