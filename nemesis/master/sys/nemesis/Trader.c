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
 **     sys/nemesis/Trader.c
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 **	Initialisation for the systemwide Trader
 ** 
 ** ENVIRONMENT: 
 **
 **	User space (System domain, these days)
 **
 ** ID : $Id: Trader.c 1.2 Wed, 02 Jun 1999 13:55:50 +0100 dr10009 $
 ** 
 **
 */

#include <nemesis.h>
#include <stdio.h>
#include <mutex.h>
#include <exceptions.h>
#include <ntsc.h>

#include <IDCMacros.h>

#include <TradedContextMod.ih>
#include <ContextMod.ih>
#include <TradedContext.ih>
#include <Security.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif


/* XXX; make sure that NUM_TRADED_CONTEXTS is the same as the number of
   elements in traded_contexts */

#define NUM_TRADED_CONTEXTS 4

const char * const traded_contexts[] = {
    "svc",
    "dev",
    "modules",
    "mounts"
};

/*---------------------------------------------------- Export Trader ----*/

void
StartTrader (void)
{
    TradedContextMod_clp tcmod;
    Security_Tag systag;
    ContextMod_clp  cmod;
    IDCOffer_clp trader_offer;
    TradedContext_clp	trader;
    Security_CertSeq *null_seq;
    Context_clp sys;
    int i;

    TRC(printf("Trader: starting:\n"));

    cmod = NAME_FIND ("modules>ContextMod", ContextMod_clp);
    tcmod = NAME_FIND ("modules>TradedContextMod", TradedContextMod_clp);
    systag = NAME_FIND ("sys>SystemTag", Security_Tag);
    null_seq=SEQ_CLEAR(SEQ_NEW(Security_CertSeq,0,Pvs(heap)));
    
    /* Create the trader context */
    TRC(printf("Trader: creating main trader context\n"));
    trader_offer = TradedContextMod$New(tcmod, cmod, Pvs(heap), Pvs(types),
					Pvs(entry), systag);

    TRC(printf("Trader: binding to main trader context\n"));
    trader = IDC_BIND(trader_offer, TradedContext_clp);

    for (i=0; i<NUM_TRADED_CONTEXTS; i++) {
	IDCOffer_clp tmp;
	TRC(printf("Trader: create >%s\n", traded_contexts[i]));
	tmp = TradedContextMod$New(tcmod, cmod, Pvs(heap), Pvs(types),
				   Pvs(entry), systag);
	TradedContext$AddTradedContext (trader, traded_contexts[i],
					tmp, systag, null_seq);
    }
	

    /* Export an offer for the trader at "sys>TraderOffer" in root */
    TRC(printf ("Trader_Init: exporting... "));

    sys=NAME_FIND("sys>StdRoot>sys",Context_clp);

    CX_ADD_IN_CX(sys, "TraderOffer", trader_offer, IDCOffer_clp);

    SEQ_FREE(null_seq);

    TRC(printf("StartTrader: done\n"));
}
