#include <nemesis.h>
#include <mutex.h>
#include <time.h>
#include <contexts.h>
#include <exceptions.h>
#include <ecs.h>
#include <IDCMacros.h>
#include <IDCMarshal.h>
#include <string.h>
#include <stdio.h>

#include <Bouncer.ih>
#include <LongCardTblMod.ih>
#include <LongCardTbl.ih>
#include <IDCStubs.ih>
#include <IDCClientStubs.ih>
#include <IDCServerStubs.ih>
#include <RPCMod.ih>
#include <IDC.ih>
#include <IDCTransport.ih>
#include <RPCTransport.ih>
#include <IORConverter.ih>
#include <IDCOfferMod.ih>
#include <IDCMarshalCtl.ih>
#include <StubGen.ih>
#include <Netif.ih>
#include <FlowMan.ih>
#include <UDPMod.ih>
#include <UDP.ih>
#include <ActivationF.ih>
#include <TimeNotify.ih>

#define NUM_BUFS         16
#define NUM_BLOCKREQS    16

typedef struct _Server_st Server_st;
typedef struct _RPC_st RPC_st;

/* Structure stored in blockTbl */

typedef struct _BlockedRequest {
    Event_Count ec;
    Event_Val   waitval;
    Time_T timeout;
    bool_t completed;
    IO_Rec rxrecs[3];
    IO_Rec txrecs[3];
} BlockedRequest;

/* communication state */

typedef struct _RPCComm_st {

    mutex_t            txPutMu;
    IO_clp             txio;
    IO_Rec             txbufs[NUM_BUFS][3];
    Event_Sequencer    get_txbuf_sq;
    Event_Count        txbuf_ec;
    Event_Sequencer    put_txbuf_sq;


    Event_Count        busy_txbuf_ec;

    word_t             numtxbufs;

    mutex_t            rxGetMu;
    mutex_t            rxPutMu;
    IO_clp             rxio;

    UDP_clp            udp;
    Heap_clp           udptxheap;
    Heap_clp           udprxheap;
    uint32_t           header;
    uint32_t           mtu;
    uint32_t           trailer;
    FlowMan_SAP        localSap;
    FlowMan_SAP        remoteSap;

    addr_t             txbase;
    addr_t             rxbase;
    
} RPCComm_st;
    
typedef struct _RPCServerMUX_st {
    RPC_st            *rpc;
    RPCComm_st         comm;

    LongCardTbl_clp    offerTbl;
    mutex_t            offerTblMu;
    uint64_t           nextOffer;

    LongCardTbl_clp    bindingTbl;
    mutex_t            bindingTblMu;

    
    IDCServerStubs_cl  serverstubs;
} RPCServerMUX_st;

typedef struct _RPCClientMUX_st {
    RPC_st            *rpc;
    RPCComm_st         comm;
    LongCardTbl_clp    blockTbl;
    mutex_t            blockTblMu;
    TimeNotify_cl      notify_cl;
} RPCClientMUX_st;

struct _RPC_st {
    RPCTransport_cl    transport_cl;
    Bouncer_cl         bouncer_cl;
    IORConverter_cl    iorconverter_cl;
    IDC_cl             idc_cl;
    Server_st         *bouncerServer;

    LongCardTbl_clp    bouncerTbl;
    mutex_t            bouncerTblMu;
    
    /* For now, just one server and one client stack */

    RPCServerMUX_st   *serverMUX;
    RPCClientMUX_st   *clientMUX;

    LongCardTbl_clp    remoteOfferTbl;
    mutex_t            remoteOfferTblMu;

    string_t           iorconverter_name;
    
    IDCOfferMod_clp    om;
    StubGen_Code      *offerCode;
    StubGen_clp        stubgen;

};

/*
 * Generic closure for client and server states 
 */
typedef struct _generic_cl _generic_cl;
struct _generic_cl {
  addr_t	op;
  addr_t	st;
};

typedef struct _RPCOffer_st {
    IDCOffer_cl       offer_cl;
    IDCOffer_clp      shmoffer;
    IDCService_clp    shmservice;
    RPC_st           *rpc;
    IDCStubs_Info     info;
    IDCCallback_clp   scb;
    Type_Any          server;    
    ProtectionDomain_ID pdom;
    uint64_t          offerId;
    IDCOffer_IORList *iorlist;

} RPCOffer_st;

typedef struct _SurrRPCOffer_st {
    IDCOffer_cl        offer_cl;
    RPC_st            *rpc;
    Type_Code          tc;
    FlowMan_SAP        remoteSap;
    uint64_t           remoteOffer;
    IDCOffer_IORList  *iorlist;
} SurrRPCOffer_st;

typedef struct _Client_st {

    IDCClientStubs_State cs;

    RPCClientMUX_st     *mux;
    mutex_t		txBufMu;	/* Lock for client side		*/
    mutex_t             rxBufMu;


    IO_Rec              txrecs[3];
    IDC_BufferRec 	txbuf;		/* Transmit buffer descriptor	*/

    IO_Rec              rxrecs[3];
    IDC_BufferRec	rxbuf;		/* Receive ditto		*/
    
    Heap_Size		txsize;		/* Size of tx buffer 		*/
    Heap_Size		rxsize;		/* Size of rx buffer		*/
    
    FlowMan_SAP         remoteSap;
    uint64_t            remoteBinding;

    uint64_t            transId;
    BlockedRequest     *blockReq;

    /* General state */
    IDCClientBinding_cl cntrl_cl;	/* client binding		*/
    struct _generic_cl  client_cl;	/* client surrogate		*/
} Client_st;

struct _Server_st {
    
    IDCServerStubs_State  ss;		/* Server state			*/
    
    RPCServerMUX_st      *mux;
    
    IO_Rec                txrecs[3];
    IDC_BufferRec 	  txbuf;        /* Transmit buffer descriptor	*/

    IO_Rec                rxrecs[3];
    IDC_BufferRec	  rxbuf;	/* Receive ditto		*/
    
    Heap_Size		  txsize;	/* Size of tx buffer 		*/
    Heap_Size		  rxsize;	/* Size of rx buffer		*/

    IDCServerStubs_cl     cntrl_cl;
    IDCServerBinding_cl   bind_cl;
    uint64_t              transId;

    FlowMan_SAP           remoteSap;
    bool_t                checkSap;

    RPCOffer_st          *offer;
};

extern const IDCMarshalCtl_cl RPCMarshalCtl;
extern IDC_op                 RPCMarshal_ms;
