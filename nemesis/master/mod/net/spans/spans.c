/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	
**
** FUNCTIONAL DESCRIPTION:
**
**	
**
** ENVIRONMENT: 
**
**
**
** ID : $Id: spans.c 1.1 Thu, 29 Apr 1999 09:53:48 +0100 dr10009 $
**
**
*/

#include <nemesis.h>
#include <mutex.h>
#include <exceptions.h>
#include <ecs.h>
#include <IDCMacros.h>
#include <time.h>
#include <netorder.h>
#include <environment.h>
#include <links.h>

#include <stdio.h>
#include <AALPod.ih>
#include <LongCardTblMod.ih>
#include <LongCardTbl.ih>
#include <RTC.ih>
#include <ntsc.h>


#define __Spans_STATE client_st
typedef struct _conn_st conn_st;
typedef struct _local_sap local_sap;
typedef struct _client_st client_st;
typedef struct _spans_st spans_st;

typedef struct _callback_channel callback_channel;

#include <Spans.ih>
#include <IOTransport.ih>

#include <IOMacros.h>

#include "fore_xdr.h"
#include "fore_msg.h"


#ifdef VERBOSE
#define DEBUG
#define TRC2(x) x
#else
#define TRC2(x)
#endif

#ifdef DEBUG
#define fprintf(stderr, args...) eprintf(args)
#define TRC(x) x
#define ASSERTA(cond,fail) {						\
    if (!(cond)) {							\
	eprintf("%s assertion %s failed in function %s (line %d)\n",	\
		__FILE__, #cond, __FUNCTION__, __LINE__);		\
	{fail};								\
	while(1);							\
    }									\
}
#else
#define TRC(x)
#define ASSERTA(cond,fail)
#endif

#define ASSERT(cond) ASSERTA(cond, ntsc_dbgstop();)

#define FIELD(msg, name) &msg->value.Msg_contents_u.name
#define SEND_MSG(st, msg, type) send_signal_msg(st, msg, sizeof(type##_args))

#define NUM_BUFS 16  // XXX

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);


static	Spans_CallbackChannel_fn     Spans_CallbackChannel_m;
static	Spans_Bind_fn 		     Spans_Bind_m;
static	Spans_Unbind_fn 	     Spans_Unbind_m;
static	Spans_Listen_fn 	     Spans_Listen_m;
static	Spans_Connect_fn 	     Spans_Connect_m;
static	Spans_Accept_fn 	     Spans_Accept_m;
static	Spans_Reject_fn 	     Spans_Reject_m;
static	Spans_Close_fn 		     Spans_Close_m;
static	Spans_Add_fn 		     Spans_Add_m;
static	Spans_Remove_fn 	     Spans_Remove_m;

static	Spans_op	spans_ms = {
  Spans_CallbackChannel_m,
  Spans_Bind_m,
  Spans_Unbind_m,
  Spans_Listen_m,
  Spans_Connect_m,
  Spans_Accept_m,
  Spans_Reject_m,
  Spans_Close_m,
  Spans_Add_m,
  Spans_Remove_m
};

/* Callbacks called when clients bind/die */
static  IDCCallback_Request_fn          IDCCallback_Request_m;
static  IDCCallback_Bound_fn            IDCCallback_Bound_m;
static  IDCCallback_Closed_fn           IDCCallback_Closed_m;

IDCCallback_op  callback_ms = {
  IDCCallback_Request_m,
  IDCCallback_Bound_m,
  IDCCallback_Closed_m
};

static IDCCallback_Request_fn Spans_IDCRequest_m;
static IDCCallback_Bound_fn Spans_IDCBound_m;
static IDCCallback_Closed_fn Spans_IDCClosed_m;

static IDCCallback_op Spans_callback_ms = { 
    Spans_IDCRequest_m, 
    Spans_IDCBound_m, 
    Spans_IDCClosed_m 
};



struct _callback_channel {
    callback_channel *next;  
    callback_channel *prev;  

    IDCCallback_cl iocb_cl;

    client_st *client;

    bool_t eventMissed;
    Spans_Handle handle;
    IO_clp io;
    IDCService_clp service;
};

struct _conn_st {
    conn_st   *next;
    conn_st   *prev;
    Atm_connection conn;   /* network order */
    Atm_conn_resource qos; /* network order */
    Atm_conn_resource min; /* network order */
    Vpvc vpvc;             /* network order */
    Atm_sig_state state;   /* host order */

    local_sap  *lsap;
    bool_t outgoing;
    Spans_Handle handle;
};

struct _local_sap {
    local_sap *next;    
    local_sap *prev;    

    client_st *client;

    bool_t listening;
    bool_t closing;
    Atm_sap id;
    conn_st  conns;
    callback_channel *channel;
    
};

struct _client_st {
    client_st *next;  
    client_st *prev;  

    Spans_cl  cl;
    spans_st  *st;

    local_sap saps;

    callback_channel channels;
    bool_t local;
    bool_t dead;
};

struct _spans_st {

    mutex_t    mu;

    IO_clp rxio, txio;

    IDCCallback_cl  callback_cl;

    LongCardTbl_clp sapTbl;

    LongCardTbl_clp connTbl;

    LongCardTbl_clp vpvcTblTx, vpvcTblRx;
    word_t nextVci;
    word_t nextPort;
    
    Spans_Handle nextHandle;

    Atm_address myAddress;
    Atm_address peerAddress;

    bool_t linkEstablished;
    int32_t peerEpoch;
    int32_t hostEpoch;

    addr_t txBufs[NUM_BUFS];
    int bufsLeft;

    word_t bufsize;

    client_st  clients;

};

/* UTILITY FUNCTIONS */

const string_t atm_results[] = {
    "success",
    "failure",
    "no_vpvc",
    "no_resources",
    "bad_destination"
};
	
/* Obtain a buffer with which to send a signalling message */

Atm_sig_msg *get_tx_buffer(spans_st *st) {

    Atm_sig_msg *msg;
    word_t value;
    uint32_t nrecs;

#ifdef TEST_MODE
    return Heap$Malloc(Pvs(heap), st->bufsize);
#else
    if(st->bufsLeft) {
	msg = st->txBufs[--st->bufsLeft];
    } else {
	IO_Rec rec;
	IO$GetPkt(st->txio, 1, &rec, FOREVER, &nrecs, &value);
	msg = rec.base;
    }
    msg->release = htonl(FORE_ATM_VERSION);
#endif
    return msg;
}

/* Send a signalling message - the buffer should have been obtained
   from get_tx_buffer() */

void send_signal_msg(spans_st *st, Atm_sig_msg *msg, int argsize) {

    IO_Rec rec;
    int ncells;
    uint8_t *buf;
    int datalen;
    buf = rec.base = msg;
    datalen = sizeof(Version) + sizeof(Message_type) + argsize;

    /* Allow for AAL5 trailer */
    rec.len = datalen + 8;

    /* Pad to next cell */
    ncells = (rec.len + 47) / 48;
    rec.len = ncells * 48;

    /* Put length field in trailer */
    buf[rec.len - 5] = datalen & 0xff;
    buf[rec.len - 6] = datalen >> 8;

#ifdef TEST_MODE
    FREE(msg);
#else
    IO$PutPkt(st->txio, 1, &rec, 0, FOREVER);
#endif

}

/* This is a very simple VPI/VCI allocation system - ought to be more
   sensible, and integrated with device driver controller */

bool_t vpvc_alloc(spans_st *st, client_st *client,
		   int *vpi, int *vci, bool_t tx, bool_t valid) {
    
    bool_t ok;
    addr_t tmp;
    LongCardTbl_clp tbl = tx? st->vpvcTblTx : st->vpvcTblRx;
    
    if(valid) {
	
	TRC(fprintf(stderr, "Spans: checking %s vpvc %u:%u\n", 
		    tx ? "tx":"rx", *vpi, *vci));
	
	ok = !LongCardTbl$Get(tbl, MKVPVC(*vpi, *vci), &tmp);
	
	if(ok) {
	    LongCardTbl$Put(tbl, MKVPVC(*vpi, *vci), client);
	}
	
    } else {

	TRC(fprintf(stderr, "Spans: finding free %s vpvc\n", 
		    tx ? "tx":"rx"));
	
	while(LongCardTbl$Get(tbl, MKVPVC(0, st->nextVci), &tmp)) {
	    
	    st->nextVci++;
	    
	}
    
	*vpi = 0;
	*vci = st->nextVci++;

	LongCardTbl$Put(tbl, MKVPVC(*vpi, *vci), client);
	ok = True;
    }

    if(ok) {

	TRC(fprintf(stderr, "Spans: allocated vpvc %u:%u\n", *vpi, *vci));

    } else {

	fprintf(stderr, "Spans: failed to alloc vpvc!\n");
    }

    return ok;

}
	
void vpvc_free(spans_st *st, Vpvc vpvc, bool_t tx) {

    addr_t tmp;
    LongCardTbl_clp tbl = tx? st->vpvcTblTx : st->vpvcTblRx;

    if(vpvc != 0) {
	/* XXX Ought to do tell device driver to drop connection */
	if(!LongCardTbl$Delete(tbl, vpvc, &tmp)) {
	    
	    fprintf(stderr, "Spans: VpVc %u:%u does not exist\n", 
		    VP_ID(vpvc), VC_ID(vpvc));
	}
    } else {
	TRC(fprintf(stderr, "Spans: Not freeing dummy vpvc\n"));
    }
    
}
    

typedef char addrbuf[32];
typedef char connbuf[96];

void sprintf_address(char *buf, Atm_address *addr) {

    sprintf(buf, "(%08x:%04x:%04x)", 
	    SWITCH_ID(*addr), PORT_ID(*addr), PATH_ID(*addr));
	    
}

void sprintf_connection(char *buf, Atm_connection *conn) {

    addrbuf addr1, addr2;

    sprintf_address(addr1, &conn->saddr);
    sprintf_address(addr2, &conn->daddr);

    sprintf(buf, "%s:%04x -> %s:%04x", 
	    addr1, conn->ssap, addr2, conn->dsap);
    
}

local_sap *get_client_sap(Atm_sap sap, client_st *client) {

    local_sap *lsap;

    lsap  = client->saps.next;
    
    while(lsap != &client->saps) {
	if(sap == lsap->id) {
	    break;
	}
	lsap = lsap->next;
    }
    
    if(lsap == &client->saps) {
	fprintf(stderr, 
		"Spans: Client %p using invalid sap %u\n", client, sap);
	RAISE_Spans$Failure(Spans_Result_BadSAP);
    }
    
    return lsap;
}

conn_st *find_connection(spans_st *st, Atm_connection *conn, bool_t outgoing, 
			 local_sap **lsap) {

    Atm_sap sap;
    
    conn_st *c;

    *lsap = NULL;

    /* For now, we only deal with a single host address */
    
    if(outgoing) {
	if(!EQ_ADDR(conn->saddr, st->myAddress)) {
	    fprintf(stderr, "Spans: Bad source address\n");
	    return NULL;
	}
	sap = conn->ssap;
    } else {
	if(!EQ_ADDR(conn->daddr, st->myAddress)) {
	    fprintf(stderr, "Spans: Bad dest address\n");
	    return NULL;
	}
	sap = conn->dsap;
    }

    if(!LongCardTbl$Get(st->sapTbl, sap, (addr_t *)lsap)) {
	fprintf(stderr, "Spans: bad local sap %u\n", sap);
	*lsap = NULL;
	return NULL;
    }
    
    c = (*lsap)->conns.next;
    while(c != &(*lsap)->conns) {

	if(!memcmp(conn, &c->conn, sizeof(Atm_connection))) {

	    return c;

	}
	c = c->next;
    }

    return NULL;
}

void notify_closed(conn_st *c) {
    
    IO_Rec rec;
    word_t value;
    uint32_t nrecs;
    
    if(IO$GetPkt(c->lsap->channel->io, 1, &rec, 0, &nrecs, &value)) {
	
	Spans_Event *ev = rec.base;
	
	ev->tag = Spans_EventType_Close;
	ev->u.Close.connection = c->handle;
	
	IO$PutPkt(c->lsap->channel->io, 1, &rec, 0, 0);
    } else {
	fprintf(stderr, "Spans: Couldn't get buffer for callback\n");
    }

}

void destroy_client(spans_st *st, client_st *client) {

    ASSERT(LINK_EMPTY(&client->saps));

    LINK_REMOVE(client);

    while(!LINK_EMPTY(&client->channels)) {
	callback_channel *ch = client->channels.next;
	
	if(ch->io) {
	    IO$Dispose(ch->io);
	}

	IDCService$Destroy(ch->service);
	LINK_REMOVE(ch);
    }

    FREE(client);
}

void destroy_sap(spans_st *st, local_sap *lsap) {

    client_st *client = lsap->client;
    addr_t tmp;

    ASSERT(LINK_EMPTY(&lsap->conns));
    
    if(!LongCardTbl$Delete(st->sapTbl, lsap->id, &tmp)) {
	fprintf(stderr, "Spans: Destroying invalid SAP!\n");
    }

    LINK_REMOVE(lsap);
    
    FREE(lsap);

    if(client->dead && LINK_EMPTY(&client->saps)) {

	/* Client has gone away and this is its last SAP - 
	   destroy the client */

	destroy_client(st, client);

    }
}

void destroy_connection(spans_st *st, conn_st *c) {

    addr_t tmp;
    local_sap *lsap = c->lsap;

    TRC(fprintf(stderr, "Spans: Destroying connection %p\n", c));
    LongCardTbl$Delete(st->connTbl, c->handle, &tmp);

    LINK_REMOVE(c);
    
    FREE(c);
    
    if(lsap->closing && LINK_EMPTY(&lsap->conns)) {

	/* This SAP is closing, and this is the last connection on it
           - destroy it */

	destroy_sap(st, lsap);

    }
    
}

/* MESSAGE SPECIFIC FUNCTIONS */

void recv_status_request(spans_st *st, Status_req_args *req) {
    
    /* We'll ignore this - the spec doesn't say what we're meant to
       use as our address in this case ... */

    fprintf(stderr, "Spans: Received status_request!\n");

}

void send_status_request(spans_st *st) {

    Atm_sig_msg *msg;

    TRC2(fprintf(stderr, "Spans: sending status request\n"));
    msg = get_tx_buffer(st);

    msg->SPANS_TYPE = htonl(status_request);
    msg->STATUS_R_EPOCH = htonl(st->hostEpoch);
    
    SEND_MSG(st, msg, Status_req);

    TRC2(fprintf(stderr, "Spans: sent status request\n"));

}

void send_status_response(spans_st *st) {

    Atm_sig_msg *msg;
    
    TRC2(fprintf(stderr, "Spans: sending status response\n"));
    msg = get_tx_buffer(st);

    msg->SPANS_TYPE = htonl(status_response);
    msg->STATUS_S_EPOCH = htonl(st->hostEpoch);
    ASS_ADDR(msg->STATUS_S_HOST, st->myAddress);

    SEND_MSG(st, msg, Status_rsp);

    TRC2(fprintf(stderr, "Spans: sent status response\n"));

}

void recv_status_indication(spans_st *st, Status_ind_args *ind) {

    unsigned int switchid, portid;

    int32_t switchEpoch = ntohl(ind->switch_epoch);

    if(!st->linkEstablished || st->peerEpoch != switchEpoch) {

	/* Initialise link */
	
	if(!st->linkEstablished) {

	    TRC(fprintf(stderr, "Spans: Link to switch established\n"));
	    
	} else {
	    
	    TRC(fprintf(stderr, "Spans: Switch reset!\n"));

	    st->linkEstablished = False;

	    /* Need to reset all current links here */

	}

	st->peerEpoch = switchEpoch;

	ASS_ADDR(st->myAddress, ind->host_addr);

	GET_SWITCH(switchid, st->myAddress);
	GET_PORT(portid, st->myAddress);

	TRC(fprintf(stderr, "Spans: my address: %08x portid %08x\n",
		    switchid,portid));

	ASS_ADDR(st->peerAddress,ind->switch_addr);
	GET_SWITCH(switchid, st->peerAddress);
	GET_PORT(portid, st->peerAddress );	
	TRC(fprintf(stderr, "Spans: switch address: %08x portid %08x\n",
		    switchid,portid));
	
	st->linkEstablished = True;

    }
    send_status_response(st);
}

void send_close_request(spans_st *st, Atm_connection *conn, bool_t rclose) {

    Atm_sig_msg *msg;
    Close_req_args *req;

    TRC(fprintf(stderr, "Spans: sending %sclose_request\n", rclose?"r":""));

    msg = get_tx_buffer(st);
    req = FIELD(msg, close_req);

    msg->SPANS_TYPE = rclose?htonl(rclose_request):htonl(close_request);
    req->conn = *conn;
    
    SEND_MSG(st, msg, Close_req);
}

void send_close_response(spans_st *st, Atm_connection *conn, 
			 Atm_sig_result result, bool_t rclose) {
    Atm_sig_msg *msg;
    Close_rsp_args *rsp;

    TRC(fprintf(stderr, "Spans: sending %sclose_response\n", rclose?"r":""));

    msg = get_tx_buffer(st);
    rsp = FIELD(msg, close_rsp);

    msg->SPANS_TYPE = rclose?htonl(rclose_response):htonl(close_response);
    rsp->conn = *conn;
    rsp->result = htonl(result);
    
    SEND_MSG(st, msg, Close_rsp);
}

    
void recv_close_indication(spans_st *st, Close_req_args *req, bool_t rclose) {

    local_sap *lsap;
    conn_st *c;
    bool_t outgoing = rclose;

    TRC({
	connbuf buf;
	
	fprintf(stderr, "Spans: received %sclose_indication\n", rclose?"r":"");
	sprintf_connection(buf, &req->conn);
	fprintf(stderr, "Spans: %s\n", buf);
    });

    /* Try to find connection */
    
    c = find_connection(st, &req->conn, outgoing, &lsap);

    if(!lsap) {
	fprintf(stderr, "Spans: Close indication for unknown SAP\n");
    } else /* We found the SAP */ {

	if(!c) {
	    fprintf(stderr, "Spans: Close indication for bogus connection\n");
	} else {
	    
	    /* Callback user */

	    notify_closed(c);
	    /* Get rid of connection */

	    vpvc_free(st, ntohl(c->vpvc), outgoing);
	    destroy_connection(st, c);
	}
    }
    
    /* XXX Send a successful close response whatever happened, to keep
       switch happy. When we have more control over the ATM device
       driver, might want more cunningness here */
    
    send_close_response(st, &req->conn, success, rclose);
    return;
    
}

void recv_close_conf(spans_st *st, Close_rsp_args *cnf, bool_t rclose) {
    
    conn_st *c;
    local_sap *lsap;
    bool_t outgoing = !rclose;

    Atm_sig_result result = ntohl(cnf->result);

    TRC({
	connbuf buf;
	
	fprintf(stderr, "Spans: received %sclose_conf\n", rclose?"r":"");
	sprintf_connection(buf, &cnf->conn);
	fprintf(stderr, "Spans: %s\n", buf);
    });

    c = find_connection(st, &cnf->conn, outgoing, &lsap);

    if(c) {

	
	/* We found the appropriate connection */

	if(result == success) {
	    
	    if(c->state != sig_state_close_pending) {
		
		/* We weren't expecting this, but mark the connection
                   as closed anyway */
		
		fprintf(stderr, 
			"Spans: Received close conf for open connection!\n");
		
	    }
	    
	    notify_closed(c);
	    vpvc_free(st, htonl(c->vpvc), outgoing);
	    destroy_connection(st, c);
	} else {

	    fprintf(stderr, "Spans: Received close failure!\n");

	}
    } else {
	
	fprintf(stderr, "Spans: received %sclose_conf for bogus connection\n",
		rclose ? "r":"");
    }
    
    
}

void send_open_response(spans_st *st, Atm_connection *conn, 
			Atm_sig_result result, Atm_conn_resource *qos, 
			Vpvc *vpvc) {

    Atm_sig_msg *msg;
    Open_rsp_args *rsp;

    TRC(fprintf(stderr, "Spans: sending open response '%s'\n",
		atm_results[result]));
    msg = get_tx_buffer(st);
    rsp = FIELD(msg, open_rsp);

    msg->SPANS_TYPE = htonl(open_response);
    rsp->conn = *conn;
    rsp->result = htonl(result);
           
    if(result == success) {
	
	rsp->qos = *qos;
	rsp->vpvc = *vpvc;

	TRC({
	    char connbuf[256];

	    sprintf_connection(connbuf, conn);
	
	    fprintf(stderr, "%s, on (%04x:%04x)\n", connbuf, 
		    VP_ID(ntohl(rsp->vpvc)), VC_ID(ntohl(rsp->vpvc)));
	});

    } else {

	TRC(fprintf(stderr, "Open request was not successful\n"));
    }

    SEND_MSG(st, msg, Open_rsp);

    TRC(fprintf(stderr, "Spans: sent open response\n"));

}
    
void recv_open_indication(spans_st *st, Open_req_args *ind) {

    /* Someone wants to open a connection to us - first do some simple
       checks */
    
    Aal_type aal = ntohl(ind->aal);
    Atm_sap sap = ind->conn.dsap;
    local_sap *lsap = NULL;
    bool_t ok;
    conn_st *c;
    int vci,vpi;
    IO_Rec rec;
    word_t value;
    uint32_t nrecs;

    TRC({
	char connbuf[256];
	sprintf_connection(connbuf, &ind->conn);
	fprintf(stderr, "Spans: Received open indication: %s\n", connbuf);
    });

    if(aal != aal_type_5) {
	/* XXX We only do AAL5 currently ... */
	
	fprintf(stderr, "Spans: Request for unsupported AAL type %d\n", aal);

	send_open_response(st, &ind->conn, failure, NULL, NULL);

	return;
    }
    
    /* Check this is our address */

    if(!EQ_ADDR(st->myAddress, ind->conn.daddr)) {
	
	fprintf(stderr, "Spans: Not my address in open destination!\n");

	send_open_response(st, &ind->conn, failure, NULL, NULL);

	return;
    }

    c = find_connection (st, &ind->conn, False, &lsap);

    /* check the localSap exists */
    if(!lsap || lsap->closing) {
	/* This is not a valid sap */

	fprintf(stderr, "Spans: Connection for invalid address/sap\n");
	send_open_response(st, &ind->conn, bad_destination, NULL, NULL);
	return;
    }

    /* See if connection already exists */
    if(c) {
	switch(c->state) {
		
	case sig_state_open:
	    
	    /* We need to send another response */
	    
	    fprintf(stderr, "Spans: resending open_response\n");
	    send_open_response(st, &ind->conn, success, 
			       &c->qos, &c->vpvc);
	    break;
	    
	case sig_state_close_pending:
	    
	    /* The client has closed it - refuse the connection */
	    
	    fprintf(stderr, "Spans: connection closing, sending refusal\n");
	    
	    send_open_response(st, &ind->conn, failure, NULL, NULL);
	    break;
	    
	case sig_state_open_pending:
	    
	    /* The client is being slow at answering ... */
	    
	    fprintf(stderr, "Spans: repeat request for pending connection!\n");
	    
	    break;
	    
	default:
	    
	    ntsc_dbgstop(); /* Shouldn't be any other value */
	    
	}

	return;
    }  

    /* This is a request for a new connection */ 
	
    
    TRC(fprintf(stderr, "Spans: Request for new connection\n"));
    
    if(!(lsap->listening && lsap->channel->io)) {
	
	/* The client is not accepting connections on this channel */
	
	fprintf(stderr, "Spans: Sap %u is not listening or has no channel\n", 
		sap);
	
	send_open_response(st, &ind->conn, bad_destination, NULL, NULL);
	
	return;
    }

	    
    /* The client is accepting connections on this sap */
	    
    TRC(fprintf(stderr, "Spans: Client is listening\n"));
    
    vci = VC_ID(ntohl(ind->pref.vpvc));
    vpi = VP_ID(ntohl(ind->pref.vpvc));
    
    ok = vpvc_alloc(st, lsap->client, 
		    &vpi, &vci, False, ntohl(ind->pref.valid));
    
    if(!ok) {
	
	fprintf(stderr, "Spans: Couldn't allocate vp/vc\n");
	send_open_response(st, &ind->conn, no_vpvc, NULL, NULL);
	return;
    } 

    /* Allocated vpvc */ 
		
		    
    /* Create new connection */
    
    TRC(fprintf(stderr, "Spans: Creating connection\n"));
    
    c = Heap$Calloc(Pvs(heap), sizeof(*c), 1);
    c->conn = ind->conn;
    c->qos = ind->qos;
    c->handle = st->nextHandle++;
    
    LongCardTbl$Put(st->connTbl, c->handle, c);
    
    c->vpvc = htonl(MKVPVC(vpi, vci));
    c->lsap = lsap;
    
    c->state = sig_state_open_pending;
    LINK_ADD_TO_HEAD(&lsap->conns, c);
    
    /* Callback to the user */
    
    if(IO$GetPkt(lsap->channel->io, 1, 
			&rec, 0, &nrecs, &value)) {
	
	/* Check thing is large enough here, and that
	   memory is in right place XXX */
	
	Spans_Event *ev = rec.base;
	Spans_ConnectRequestData *data = &ev->u.ConnectRequest;
	
	TRC(fprintf(stderr, "Spans: Calling back user\n"));
	
	ev->tag = Spans_EventType_ConnectRequest;
	data->connection = c->handle;
	ASS_ADDR(data->lsap.addr, c->conn.daddr);
	data->lsap.port = c->conn.dsap;
	ASS_ADDR(data->rsap.addr, c->conn.saddr);
	data->rsap.port = c->conn.ssap;
	
	data->qos.peakBandwidth = ntohl(ind->qos.peak_bandwidth);
	data->qos.meanBandwidth = ntohl(ind->qos.mean_bandwidth);
	data->qos.meanBurst = ntohl(ind->qos.mean_burst);
	
	data->min.peakBandwidth = ntohl(ind->min.peak_bandwidth);
	data->min.meanBandwidth = ntohl(ind->min.mean_bandwidth);
	data->min.meanBurst = ntohl(ind->min.mean_burst);
	
	data->vpi = vpi;
	data->vci = vci;
	data->multi = False;
	
	IO$PutPkt(lsap->channel->io, 1, &rec, 0, 0);
    } else /* Can't get buffer from channel */ {
	
	TRC(fprintf(stderr, "Spans: Can't get buffer\n"));
	
    }
}

void send_open_request(spans_st *st, Atm_connection *conn, Aal_type type,
		       Atm_conn_resource *qos, Atm_conn_resource *min) {

    Atm_sig_msg *msg;
    Open_req_args *req;

    TRC(fprintf(stderr, "Spans: sending open_request\n"));

    msg = get_tx_buffer(st);
    req = FIELD(msg, open_req);

    msg->SPANS_TYPE = htonl(open_request);
    req->conn = *conn;
    req->aal = htonl(type);
    req->qos = *qos;
    req->min = *min;
    req->pref.valid = htonl(False);

    SEND_MSG(st, msg, Open_req);
}

void recv_open_conf(spans_st *st, Open_rsp_args *cnf) {

    Atm_sig_result result = ntohl(cnf->result);

    conn_st *c;
    local_sap *lsap;
    bool_t ok;
    bool_t destroy_on_failure = True;

    TRC(fprintf(stderr, "Spans: received open_confirmation\n"));
    
    TRC({
	char connbuf[256];
	sprintf_connection(connbuf, &cnf->conn);
	fprintf(stderr, "%s, on (%04x:%04x)\n", connbuf, 
		VP_ID(ntohl(cnf->vpvc)), VC_ID(ntohl(cnf->vpvc)));
    })

    c = find_connection(st, &cnf->conn, True, &lsap);

    if(c && c->state == sig_state_open_pending) {
	
	IO_Rec rec;
	word_t value;
	uint32_t nrecs;
	int vci, vpi;

	/* We found the connection */

	if(result == success) {

	    TRC(fprintf(stderr, "Spans: Checking vpvc returned by switch\n"));
	    vci = VC_ID(ntohl(cnf->vpvc));
	    vpi = VP_ID(ntohl(cnf->vpvc));

	    ok = vpvc_alloc(st, lsap->client, &vpi, &vci, True, True);

	    if(ok) {
		Vpvc vpvc = MKVPVC(vpi, vci);
		c->vpvc = htonl(vpvc);
	    } else {

		/* Oh dear, we're unable to allocate the vci for some
                   reason! */

		fprintf(stderr, 
			"Spans: Can't alloc vpvc for opened connection\n");

		result  = no_vpvc;

		c->state = sig_state_close_pending;
		c->vpvc = 0;
		send_close_request(st, &cnf->conn, False);
		destroy_on_failure = False;
	    }
	}

	/* Call the user back */
	
	if(IO$GetPkt(lsap->channel->io, 1, &rec, 0, &nrecs, &value)) {

	    /* Do memory checking ... */

	    Spans_Event *ev = rec.base;
	    Spans_ConnectResponseData *data = &ev->u.ConnectResponse;
	    
	    TRC(fprintf(stderr, "Spans: Calling back user\n"));
	    
	    ev->tag = Spans_EventType_ConnectResponse;
	    data->connection = c->handle;
	    data->result = result;
	    data->qos.peakBandwidth = ntohl(cnf->qos.peak_bandwidth);
	    data->qos.meanBandwidth = ntohl(cnf->qos.mean_bandwidth);
	    data->qos.meanBurst = ntohl(cnf->qos.mean_burst);
	    data->vpi = vpi;
	    data->vci = vci;

	    IO$PutPkt(lsap->channel->io, 1, &rec, 0, 0);
	}
	
	if(result == success) {
	    c->state = sig_state_open;
	} else if(destroy_on_failure) {
	    fprintf(stderr, "Spans: Request failed: '%s'\n", 
		    atm_results[result]);
	    
	    destroy_connection(st, c);
	}
    } else /* Connection doesn't exist or is in bad state */ {
	      
	if(!c) {
	    fprintf(stderr, "Spans: Received open conf for bogus connection!\n");
	} else {
	    switch(c->state) {
		
	    case sig_state_open:
		TRC(fprintf(stderr, "Spans: Received duplicate open conf - ignoring\n"));
		break;

	    case sig_state_close_pending:
		fprintf(stderr, "Spans: Received open conf for closing connection!\n");
		send_close_request(st, &c->conn, False);
		break;
		
	    default:
		ntsc_dbgstop(); /* XXX Something's broken ... */
	    }
	}
    }
}


void recv_query_request(spans_st *st, Query_req_args *req) {
    
    Atm_sig_msg *msg;
    Query_rsp_args *rsp;
    Atm_sig_state state = sig_state_closed;
    local_sap *lsap;
    conn_st *c;
    bool_t outgoing;
    
    TRC({
	char connbuf[256];
	
	fprintf(stderr, "Spans: received query_request\n");
	sprintf_connection(connbuf, &req->conn);
	fprintf(stderr, "Spans: %s, action %d\n", connbuf, ntohl(req->type));
    });

    /* Check this is our address */

    if(EQ_ADDR(st->myAddress, req->conn.daddr)) {
	    
	TRC(fprintf(stderr, "Spans: I am destination\n"));
	outgoing = False;
	
    } else if(EQ_ADDR(st->myAddress, req->conn.saddr)) { 
	    
	TRC(fprintf(stderr, "Spans: I am source\n"));
	    
	outgoing  = True;
	
    } else {
	
	/* Don't reply to this - it isn't for us */
	fprintf(stderr, "Spans: Query request for unknown address\n");
	return;
    }
    
    c = find_connection(st,  &req->conn, outgoing, &lsap);
    
    if(!lsap) {
	fprintf(stderr, "Spans: Query request for unknown SAP\n");
    } else if(!c) {
	
	TRC(fprintf(stderr, 
		    "Spans: Query request for unknown connection\n"));
	
    } else {

	/* We found the connection */
	
	state = c->state;
	
    }
    
    msg = get_tx_buffer(st);
    msg->SPANS_TYPE = htonl(query_response);

    rsp = FIELD(msg, query_rsp);
    rsp->conn = req->conn;
    rsp->type = req->type;
    rsp->state = htonl(state);
    rsp->data = 0; /* mystic debugging field ... */

    SEND_MSG(st, msg, Query_rsp);

}

void SpansThread(spans_st *st) {

    while(1) {
	
	word_t value;
	IO_Rec rec;
	Atm_sig_msg *recv_msg;
	uint32_t nrecs;
	bool_t gotRecs;

	if(st->linkEstablished) {
	    TRC2(fprintf(stderr, "Spans: waiting for packet\n"));
	    gotRecs = IO$GetPkt(st->rxio, 1, &rec, FOREVER, &nrecs, &value);
	} else {
	    TRC2(fprintf(stderr, "Spans: waiting for packet or timeout\n"));
	    gotRecs = IO$GetPkt(st->rxio, 1, &rec, 
				NOW() + SECONDS(5), &nrecs, &value);
	}

	MU_LOCK(&st->mu);
	
	if(gotRecs) {
	    
	    TRC2(fprintf(stderr, "Spans: Received packet %p\n", rec.base));
	    
	    /* We don't bother checking the AAL5 trailer information -
               the length is implied by the packet type */

	    recv_msg = rec.base;
	    
	    if(recv_msg->release == htonl(FORE_ATM_VERSION)) {

		switch(ntohl(recv_msg->SPANS_TYPE)) {
		    
		case status_request:
		    recv_status_request(st, FIELD(recv_msg, status_req));
		    break;
		    
		case status_indication:
		    recv_status_indication(st, FIELD(recv_msg, status_ind));
		    break;
		    
		case status_response:
		    TRC(fprintf(stderr, "Spans: Received status_response!\n"));
		    break;

		case open_request:
		case open_indication:
		    recv_open_indication(st, FIELD(recv_msg, open_ind));
		    break;

		case open_response:
		case open_confirmation:
		    recv_open_conf(st, FIELD(recv_msg, open_cnf));
		    break;
		    
		case close_request:
		case close_indication:
		    recv_close_indication(st, FIELD(recv_msg, close_req), 
					  False);
		    break;

		case close_response:
		case close_confirmation:
		    recv_close_conf(st, FIELD(recv_msg, close_cnf), False);
		    break;

		case rclose_request:
		case rclose_indication:
		    recv_close_indication(st, FIELD(recv_msg, close_req),
					  True);
		    break;

		case rclose_response:
		case rclose_confirmation:
		    recv_close_conf(st, FIELD(recv_msg, close_cnf), True);
		    break;

		case query_request:
		    recv_query_request(st, FIELD(recv_msg, query_req));
		    break;

		default:
		    /* This is something we don't process yet ... */
		    fprintf(stderr, "Spans: Received UNKNOWN msg type %d\n", 
			    ntohl(recv_msg->SPANS_TYPE));
		    break;
		    
		}
	    } else {
		TRC(fprintf(stderr, 
			    "Spans: Received packet with "
			    "incorrect version 0x%x\n", 
			    ntohl(recv_msg->release)));
	    }

	    /* Release packet */

	    rec.len = st->bufsize;
	    IO$PutPkt(st->rxio, 1, &rec, 0, FOREVER);
	} else {
	    
	    TRC(fprintf(stderr, "Spans: rx timed out\n"));

	    if(!st->linkEstablished) {
		
		send_status_request(st);

	    }
	    
	}
	
	MU_RELEASE(&st->mu);
		
	    
    }

}

client_st *new_client(spans_st *st) {

    client_st *client = Heap$Calloc(Pvs(heap), sizeof(client_st), 1);   

    if(!client) {
	TRC(fprintf(stderr, "Spans: Can't allocate memory for client!\n"));
	return NULL;
    }
    
    LINK_INITIALISE(&client->saps);
    LINK_INITIALISE(&client->channels);
    
    client->st = st;
    CL_INIT(client->cl, &spans_ms, client);


    LINK_ADD_TO_HEAD(&st->clients, client);

    return client;
}

static void StatsThread(spans_st *st) {

    while(1) {

	int txcons = 0, rxcons = 0, txvcs, rxvcs;
	LongCardTblIter_clp iter;
	Spans_Handle h;
	conn_st *conn;

	PAUSE(SECONDS(20));

	MU_LOCK(&st->mu);
	
	txvcs = LongCardTbl$Size(st->vpvcTblTx);
	rxvcs = LongCardTbl$Size(st->vpvcTblRx);

	iter = LongCardTbl$Iterate(st->connTbl);
	
	while(LongCardTblIter$Next(iter, &h, (addr_t *)&conn)) {
	    if(conn->outgoing) {
		txcons++;
	    } else {
		rxcons++;
	    }
	}
	
	LongCardTblIter$Dispose(iter);
	
	fprintf(stderr, "Spans: TX vcs: %u, RX vcs: %u, TX conns: %u, RX conns: %u\n",
		txvcs, rxvcs, txcons, rxcons);

	if(txcons != rxcons) {

	    client_st *client = st->clients.next;

	    while(client != &st->clients) {

		local_sap *lsap = client->saps.next;
		
		while(lsap != &client->saps) {

		    conn_st *conn = lsap->conns.next;

		    while(conn != &lsap->conns) {

			connbuf buf;

			sprintf_connection(buf, &conn->conn);

			fprintf(stderr, "%04u(%s): Client %p, state %u, conn %s\n",
				(uint32_t) conn->handle, conn->outgoing?"tx":"rx", 
				client, conn->state, buf);

			conn = conn->next;
		    }

		    lsap = lsap->next;
		}

		client = client->next;
	    }
	}
	MU_RELEASE(&st->mu);

    }
}

MAIN_RUNES(Spans);

void Main(Closure_clp self) {

    AALPod_clp aal;
    addr_t bufs;
    int i;
    LongCardTblMod_clp tblmod = NAME_FIND("modules>LongCardTblMod",
					  LongCardTblMod_clp);
    RTC_clp rtc = NAME_FIND("modules>RTC", RTC_clp);

    spans_st *st = Heap$Calloc(Pvs(heap), sizeof(*st), 1);
    client_st *main_client;

    uint32_t spans_vci = 15;

    if(!st) {
	RAISE_Heap$NoMemory();
    }
    TRC(fprintf(stderr, "Spans: Allocated state at %p\n", st));
    

    st->hostEpoch = RTC$GetCMOSTime(rtc);
    
    MU_INIT(&st->mu);

#ifndef TEST_MODE
    aal = IDC_OPEN("dev>atm0", AALPod_clp);

    st->txio = IO_BIND((IOOffer_clp)AALPod$Open(aal, AALPod_Dir_TX, AALPod_Master_Client, 
				   AALPod_Mode_AAL5_Raw, 0, spans_vci, 64));

    st->rxio = IO_BIND((IOOffer_clp)AALPod$Open(aal, AALPod_Dir_RX, AALPod_Master_Client, 
				   AALPod_Mode_AAL5_Raw, 0, spans_vci, 64));

    /* Ought to allocate memory properly - wait for great IO restructuring */

    st->bufsize = (sizeof(Atm_sig_msg) + 56) ;

    bufs = Heap$Malloc(Pvs(heap), NUM_BUFS * st->bufsize);
    
    for(i = 0; i < NUM_BUFS; i++) {
	IO_Rec rec;
	
	rec.base = bufs + i * st->bufsize;
	rec.len = st->bufsize;

	IO$PutPkt(st->rxio, 1, &rec, 0, FOREVER);

    }

    bufs = Heap$Malloc(Pvs(heap), NUM_BUFS * st->bufsize);

    for(i = 0; i < NUM_BUFS; i++) {
	st->txBufs[i] = bufs + i * st->bufsize;
    }

#endif

    st->bufsLeft = NUM_BUFS;

    st->sapTbl = LongCardTblMod$New(tblmod, Pvs(heap));

    st->vpvcTblTx = LongCardTblMod$New(tblmod, Pvs(heap));
    st->vpvcTblRx = LongCardTblMod$New(tblmod, Pvs(heap));

    st->connTbl = LongCardTblMod$New(tblmod, Pvs(heap));
    st->nextVci = 256;
    st->nextPort = 1024;
    st->nextHandle = 1;

    LINK_INITIALISE(&st->clients);

#ifndef TEST_MODE
    send_status_request(st);

    Threads$Fork(Pvs(thds), SpansThread, st, 0);

    while(!st->linkEstablished) {

	PAUSE(SECONDS(1));

    }
#endif

    Threads$Fork(Pvs(thds), StatsThread, st, 0);
    CL_INIT(st->callback_cl, &callback_ms, st);

    TRC(fprintf(stderr, "Spans: Creating main client\n"));
    
    main_client = new_client(st);

    main_client->local = True;
	  
    {						
	IDCTransport_clp	shmt;			
	Type_Any       	any;		
	IDCOffer_clp   	offer;			
	IDCService_clp 	service;		
	
	shmt = NAME_FIND ("modules>ShmTransport", IDCTransport_clp); 
	ANY_INIT(&any,Spans_clp, &main_client->cl);		
	offer = IDCTransport$Offer (shmt, &any, &st->callback_cl, 
				    Pvs(heap), Pvs(gkpr), 
				    Pvs(entry), &service); 
	
	/* XXX Ought to be svc>net>spans, but this is read-only in the
           trader */
	CX_ADD("svc>spans", offer, IDCOffer_clp);
	
    }

    TRC(fprintf(stderr, "Spans: Exported spans service\n"));

}


/*---------------------------------------------------- Entry Points ----*/

static IOOffer_clp Spans_CallbackChannel_m (
	Spans_cl	*self,
	uint32_t	numSlots	/* IN */
   /* RETURNS */,
	Spans_Handle	*handle )
{
    client_st *client = self->st;
    spans_st *st = client->st;
    
    IOTransport_clp iotr = NAME_FIND("modules>IOTransport", IOTransport_clp);

    callback_channel *ch = Heap$Malloc(Pvs(heap), sizeof(*ch));

    IOOffer_clp NOCLOBBER offer = NULL;

    if(!ch) RAISE_Heap$NoMemory();
	  
    USING(MUTEX, &st->mu,


	  IOData_Shm *shm = SEQ_NEW(IOData_Shm, 1, Pvs(heap));
	  IOData_Area *area = &SEQ_ELEM(shm, 0);

	  TRC(fprintf(stderr, "Spans: Creating callback channel\n"));

	  
	  ch->handle = st->nextHandle++;
	  ch->client = client;
	  ch->io = NULL;
	  CL_INIT(ch->iocb_cl, &Spans_callback_ms, ch);
	  
	  area->buf.base = NULL;
	  area->buf.len = sizeof(Spans_Event) * numSlots;
	  area->param.attrs = 0;
	  area->param.awidth = PAGE_WIDTH;
	  area->param.pwidth = PAGE_WIDTH;

	  offer = IOTransport$Offer(iotr,  
				    Pvs(heap), numSlots, IO_Mode_Rx, IO_Kind_Slave, shm, Pvs(gkpr),
				    &ch->iocb_cl, NULL, 0, &ch->service);
	  
	  TRC(fprintf(stderr, "Spans: offer %p, channel %p\n", offer, ch));
	  
	  LINK_ADD_TO_HEAD(&client->channels, ch);
	  
	  *handle =  ch->handle;
    
	);
        
    return offer;

}

static const Spans_Address NullAddress = { {0} };

static Spans_BindResult Spans_Bind_m (
	Spans_cl	*self,
	Spans_SAP	*sap	/* INOUT */,
	Spans_Handle	         channel	/* IN */ )
{
    client_st *client = self->st;
    spans_st *st = client->st;
    
    callback_channel *ch;
    local_sap * NOCLOBBER lsap = NULL;
    bool_t NOCLOBBER ok = False;

    TRC(fprintf(stderr, "Spans: Client %p binding to sap %qx:%u\n", 
		client, *(uint64_t *)&sap->addr, sap->port));

        
    
    USING(MUTEX, &st->mu,

	  bool_t search = False;
	  addr_t tmp;

	  if(!memcmp(&sap->addr, &NullAddress, sizeof(Spans_Address))) {
	      TRC(fprintf(stderr, "Spans: Setting address to my address\n"));
	      /* Client has specified default address */
	      ASS_ADDR(sap->addr, st->myAddress);
	  }
	  
	  /* For now we don't deal with group addresses */
	  
	  if(!EQ_ADDR(sap->addr, st->myAddress)) {
	      fprintf(stderr, "Spans: Bad address %qx\n", *(uint64_t *) &sap->addr);
	      RAISE_Spans$Failure(Spans_Result_BadAddress);
	  }
	  
	  if(!sap->port) {
	      sap->port = st->nextPort++;
	      search = True;
	      TRC(fprintf(stderr, "Spans: Doing autoalloc, starting at %u\n", 
			  sap->port));
	  }
	  
	  TRC(fprintf(stderr, "Spans: searching for channel %p\n", 
		      (addr_t) channel));
	  
	  ch = client->channels.next;
	  
	  while(ch != &client->channels) {
	      if(channel == ch->handle) {
		  break;
	      }
	      ch = ch->next;
	  }
	  
	  if(ch == &client->channels) {
	      fprintf(stderr, "Spans: Couldn't find channel %p\n", 
		      (addr_t) (word_t) channel);
	      RAISE_Spans$Failure(Spans_Result_BadChannel);
	  }
	  
	  lsap = Heap$Calloc(Pvs(heap), sizeof(local_sap), 1);
	  
	  lsap->channel = ch;
	  lsap->client = client;
	  lsap->listening = False;
	  
	  LINK_INITIALISE(&lsap->conns);
	  
	  if(!lsap) {
	      fprintf(stderr, "Spans: No memory for local sap\n");
	      RAISE_Spans$Failure(Spans_Result_GeneralFailure);
	  }
	  
	  do {
	      TRC(fprintf(stderr, "Spans: Checking sap %u\n", sap->port));
	      ok = !LongCardTbl$Get(st->sapTbl, sap->port, &tmp);
	      
	      if(ok) {
		  LINK_ADD_TO_HEAD(&client->saps, lsap);
		  
		  TRC(fprintf(stderr, "Spans: Adding sap %u to table\n", sap->port));
		  LongCardTbl$Put(st->sapTbl, sap->port, lsap);
	      } else if (search) {
		  sap->port = st->nextPort++;
	      }
	  } while(search && !ok);
	  
	  lsap->id = sap->port;

	);

        
    if(!ok) {
	FREE(lsap);
	return Spans_BindResult_Busy;
    } else {
	return Spans_BindResult_Success;
    }

}

static void Spans_Unbind_m (
	Spans_cl	*self,
	const Spans_SAP	*sap	/* IN */ )
{
    
    client_st *client = self->st;
    spans_st *st = client->st;

    USING(MUTEX, &st->mu,
	  
	  local_sap *lsap = get_client_sap(sap->port, client);

	  if(!LINK_EMPTY(&lsap->conns)) {
	      RAISE_Spans$Failure(Spans_Result_BusySAP);
	  }

	  destroy_sap(st, lsap);
	);
}

static void Spans_Listen_m (
	Spans_cl	*self,
	const Spans_SAP	*sap	/* IN */,
	bool_t	listen	/* IN */ )
{
    client_st *client = self->st;
    spans_st *st = client->st;
	
    local_sap *lsap;

        

    USING(MUTEX, &st->mu,
	  /* For now we don't deal with group addresses */

	  if(!EQ_ADDR(sap->addr, st->myAddress)) {
	      fprintf(stderr, "Spans: Trying to listen on bad address %qx\n", 
		      *(uint64_t *)&sap->addr);
	      RAISE_Spans$Failure(Spans_Result_BadAddress);
	  }
	  
	  lsap = get_client_sap(sap->port, client);
	  
	  lsap->listening = listen;
	);
    
        
}

static Spans_Handle Spans_Connect_m (
	Spans_cl	*self,
	const Spans_SAP	*src	/* IN */,
	const Spans_SAP	*dest	/* IN */,
	const Spans_QoS	*qos	/* IN */,
	const Spans_QoS	*min	/* IN */,
	bool_t	multi	/* IN */ )
{
    client_st *client = self->st;
    spans_st *st = client->st;
    
    local_sap *lsap;
    conn_st * NOCLOBBER conn = NULL;


    TRC(fprintf(stderr, "Spans$Connect(%p, %p, %p, %p, %p, %u)\n",
		self, src, dest, qos, min, multi));

    TRC({
	addrbuf srcbuf;
	addrbuf destbuf;

	sprintf_address(srcbuf, &src->addr);
	sprintf_address(destbuf, &dest->addr);
	fprintf(stderr, "Spans: Connect(%s:%04u -> %s:%04u)\n",
		srcbuf, src->port, destbuf, dest->port);
    })


    if(EQ_ADDR(src->addr, dest->addr) && (src->port == dest->port)) {

	fprintf(stderr, 
		"Spans: Trying to create looped connection\n");
	RAISE_Spans$Failure(Spans_Result_BadSAP);
    }

    USING(MUTEX, &st->mu,
	  
	  Atm_connection atmconn;
	  /* For now we don't deal with group addresses */
	  
	  if(!EQ_ADDR(src->addr, st->myAddress)) {
	      fprintf(stderr, 
		      "Spans: Trying to connect with bad source address \n");
	      
	      RAISE_Spans$Failure(Spans_Result_BadAddress);
	  }
	  
	  if(multi) {
	      RAISE_Spans$Failure(Spans_Result_GeneralFailure);
	  }

	  ASS_ADDR(atmconn.saddr, src->addr);
	  ASS_ADDR(atmconn.daddr, dest->addr);
	  atmconn.ssap = src->port;
	  atmconn.dsap = dest->port;
	  
	  conn = find_connection(st, &atmconn, True, &lsap);

	  if(!lsap || lsap->client != client) {
	      RAISE_Spans$Failure(Spans_Result_BadSAP);
	  }

	  if(conn) {
	      RAISE_Spans$Failure(Spans_Result_ConnectionExists);
	  }

	  conn = Heap$Calloc(Pvs(heap), sizeof (*conn), 1);
	  
	  conn->lsap = lsap;
	  conn->handle = st->nextHandle++;
	  conn->conn = atmconn;

	  /* Sanity check the min and qos structs */

	  if(min->peakBandwidth > qos->peakBandwidth) {
	      conn->min.peak_bandwidth = htonl(qos->peakBandwidth);
	  } else {
	      conn->min.peak_bandwidth = htonl(min->peakBandwidth);
	  }

	  if(min->meanBandwidth > qos->meanBandwidth) {
	      conn->min.mean_bandwidth = htonl(qos->meanBandwidth);
	  } else {
	      conn->min.mean_bandwidth = htonl(min->meanBandwidth);
	  }

	  if(min->meanBurst > qos->meanBurst) {
	      conn->min.mean_burst = htonl(qos->meanBurst);
	  } else {
	      conn->min.mean_burst = htonl(min->meanBurst);
	  }

	  conn->qos.peak_bandwidth = htonl(qos->peakBandwidth);
	  conn->qos.mean_bandwidth = htonl(qos->meanBandwidth);
	  conn->qos.mean_burst = htonl(qos->meanBurst);
	  
	  conn->state = sig_state_open_pending;
	  conn->outgoing = True;
	  
	  LINK_ADD_TO_HEAD(&lsap->conns, conn);
	  
	  send_open_request(st, &conn->conn, aal_type_5, 
			    &conn->qos, &conn->min);

	  /* Need to add to pending queue */
	  
	  LongCardTbl$Put(st->connTbl, conn->handle, conn);
	);
    
    return conn->handle;

}
    

    
			
static void Spans_Accept_m (
	Spans_cl	*self,
	Spans_Handle	connection	/* IN */,
	const Spans_QoS	*qos	/* IN */ )
{
    client_st *client = self->st;
    spans_st *st = client->st;

    conn_st *c;

    TRC(fprintf(stderr, "Spans$Accept(%p)\n", (addr_t)connection));

    
    USING(MUTEX, &st->mu,

	  if(!LongCardTbl$Get(st->connTbl, connection, (addr_t) &c)) {
	      
	      /* Connection doesn't exist */
	      
	      TRC(fprintf(stderr, "Spans: Conenction %p doesn't exist\n",
			  (addr_t) connection));
	      RAISE_Spans$Failure(Spans_Result_BadConnection);
	      
	  }
	  
	  if(c->lsap->client != client) {
	      
	      /* Connection belongs to someone else */
	      
	      TRC(fprintf(stderr, 
			  "Spans: Connection %p belongs to client %p\n",
			  (addr_t) connection, c->lsap->client));

	      RAISE_Spans$Failure(Spans_Result_BadConnection);
	      
	  }
	  
	  if(c->outgoing || c->state != sig_state_open_pending) {
	      
	      /* Connection is not waiting to be accepted */
	      
	      TRC(fprintf(stderr, "Spans: Connection %p is in state %u\n",
			  (addr_t) connection, c->state));
	      RAISE_Spans$Failure(Spans_Result_BadConnection);
	      
	  }
	  
	  c->qos.mean_bandwidth = htonl(qos->meanBandwidth);
	  c->qos.peak_bandwidth = htonl(qos->peakBandwidth);
	  c->qos.mean_burst = htonl(qos->meanBurst);
	  
	  c->state = sig_state_open;
	  
	  send_open_response(st, &c->conn, success, &c->qos, &c->vpvc);
    	);
        
}

static void Spans_Reject_m (
	Spans_cl	*self,
	Spans_Handle	connection	/* IN */ )
{
    client_st *client = self->st;
    spans_st *st = client->st;
	
    conn_st *c;
    
    
    USING(MUTEX, &st->mu,

	  if(!LongCardTbl$Get(st->connTbl, connection, (addr_t) &c)) {
	      
	      /* Connection doesn't exist */
	      
	      RAISE_Spans$Failure(Spans_Result_BadConnection);
	      
	  }
	  
	  if(c->lsap->client != client) {
	      
	      /* Connection belongs to someone else */
	      
	      RAISE_Spans$Failure(Spans_Result_BadConnection);
	      
	  }
	  
	  if(c->outgoing || c->state != sig_state_open_pending) {
	      
	      /* Connection is not waiting to be accepted */
	      
	      RAISE_Spans$Failure(Spans_Result_BadConnection);
	      
	  }
	  
	  /* Get rid of connection */
	  
	  vpvc_free(st, ntohl(c->vpvc), False);
	  destroy_connection(st, c);
	  
	  send_open_response(st, &c->conn, bad_destination, NULL, NULL);
	);
        
}

static void Spans_Close_m (
	Spans_cl	*self,
	Spans_Handle	connection	/* IN */ )
{
    client_st *client = self->st;
    spans_st *st = client->st;

    conn_st *c;

    TRC(fprintf(stderr, "Spans$Close(%p)\n", (addr_t)connection));

    USING(MUTEX, &st->mu,
	  
	  if(!LongCardTbl$Get(st->connTbl, connection, (addr_t) &c)) {
	      
	      /* Connection doesn't exist */
	      
	      RAISE_Spans$Failure(Spans_Result_BadConnection);
	      
	  }
	  
	  if(c->lsap->client != client) {
	      
	      /* Connection belongs to someone else */
	      
	      RAISE_Spans$Failure(Spans_Result_BadConnection);
	      
	  }

	  if(c->state != sig_state_close_pending) {

	      connbuf buf;
	      
	      sprintf_connection(buf, &c->conn);

	      TRC(fprintf(stderr, "Spans: Closing connection %s\n", buf));

	      c->state = sig_state_close_pending;

	      send_close_request(st, &c->conn, !c->outgoing);

	  } else {
	      TRC(fprintf(stderr, "Spans: Connection is already closing\n"));
	  }
	  
	);
    
}

static void Spans_Add_m (
	Spans_cl	*self,
	Spans_Handle	connection	/* IN */,
	const Spans_SAP	*rsap	/* IN */ )
{
    client_st *client = self->st;
    spans_st *st = client->st;
    
    UNIMPLEMENTED;
}

static void Spans_Remove_m (
	Spans_cl	*self,
	Spans_Handle	connection	/* IN */,
	const Spans_SAP	*rsap	/* IN */ )
{
    client_st *client = self->st;
    spans_st *st = client->st;

    UNIMPLEMENTED;
}

/* ------------------------------------------------------------ */
/* Per-client callbacks */


/* Called when client binds, to see if we admit them */
static bool_t IDCCallback_Request_m(
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        Domain_ID       dom     /* IN */,
        ProtectionDomain_ID    pdom    /* IN */,
        const Binder_Cookies    *clt_cks        /* IN */,
        Binder_Cookies  *svr_cks        /* OUT */ )
{
    spans_st *st = self->st;
    TRC(printf("Spans callback: bind request from %qx\n", dom));

    return True; /* we accept the bind */
}


/* ------------------------------------------------------------ */
/* Domain-specific IREF Spans generation.
 * Called on bind for all clients to get their own private Spans
 * closure. Allocates and inits a new client state record (client_st) */

static bool_t IDCCallback_Bound_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        IDCServerBinding_clp    binding /* IN */,
        Domain_ID       dom     /* IN */,
        ProtectionDomain_ID    pdom    /* IN */,
	const Binder_Cookies	*clt_cks /* IN */,
	Type_Any	*server /* INOUT */)
{

    spans_st *st = self->st;
    client_st *client;
    

    TRC(fprintf(stderr, "Spans: Creating new client\n"));

    MU_LOCK(&st->mu);
    
    client = new_client(st);
    
    MU_RELEASE(&st->mu);
        
    if(!client) {
	fprintf(stderr, "Spans: Out of memory\n");
	return False;
    }

    server->val = (word_t) &client->cl;    
    TRC(fprintf(stderr, "Spans: New client accepted\n"));

    return True;
}


static void IDCCallback_Closed_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        IDCServerBinding_clp    binding /* IN */,
	const Type_Any *server /* IN */)
{

    spans_st *st = self->st;
    Spans_clp client_clp = (Spans_clp) (word_t) server->val;
    client_st *client = client_clp->st;


    local_sap *lsap, *next_lsap;

    fprintf(stderr, "Spans: Client %p disconnected\n", client);

    MU_LOCK(&st->mu);

    lsap = client->saps.next;


    while(lsap != &client->saps) {
	
	conn_st *c = lsap->conns.next;
	
	lsap->closing = True;
	
	while(c != &lsap->conns) {
	    
	    TRC(fprintf(stderr, "Spans: Closing connection %s\n", buf));
	    
	    c->state = sig_state_close_pending;
	    
	    send_close_request(st, &c->conn, !c->outgoing);

	    c = c->next;
	}
	
	next_lsap = lsap->next;

	if(LINK_EMPTY(&lsap->conns)) {
	    destroy_sap(st, lsap);
	}

	lsap = next_lsap;

    }

    client->dead = True;
    
    if(LINK_EMPTY(&client->saps)) {
	
	destroy_client(st, client);
    }

    MU_RELEASE(&st->mu);
    
}

static bool_t Spans_IDCRequest_m (
    IDCCallback_clp self,
    IDCOffer_clp offer,
    Domain_ID id,
    ProtectionDomain_ID pdom,
    const Binder_Cookies *clt_cks,
    Binder_Cookies *svr_cks) {

    return True;

}

static bool_t Spans_IDCBound_m (
    IDCCallback_clp self,
    IDCOffer_clp offer,
    IDCServerBinding_clp binding,
    Domain_ID client,
    ProtectionDomain_ID pdom,
    const Binder_Cookies *clt_cks,
    Type_Any *any) {

    callback_channel *ch = self->st;
    IO_clp io = NARROW(any, IO_clp);
    TRC(fprintf(stderr, "Spans: Client bound to IO %p on channel %p\n", 
		io, ch));

    ch->io = io;
    
    return True;
}


static void Spans_IDCClosed_m(
    IDCCallback_clp self,
    IDCOffer_clp offer,
    IDCServerBinding_clp binding,
    const Type_Any *any)
{
}

/*
 * End 
 */

