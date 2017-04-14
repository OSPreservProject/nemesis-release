#ifndef CRENDDISPLAY_ST

#define CRENDDISPLAY_ST
#include <IO.ih>
#include <WS.ih>
#include <CRendDisplay.ih>

typedef struct WS_Event WSEvent;

typedef struct {
    CRendDisplay_cl      cl;
    WS_clp               ws;			/* Connection to WS */
    IO_clp               evio;		        /* Event channel */
    WSEvent             *ev;			/* Keep an event */
    uint32_t             width, height, depth;
    uint32_t             xgrid, ygrid;
    uint32_t             stride;
    IDCClientBinding_clp binding;
} CRendDisplay_st;
#endif
