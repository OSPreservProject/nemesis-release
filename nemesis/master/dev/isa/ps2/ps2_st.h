/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      PS2 keyboard and mouse driver.
** 
** FUNCTIONAL DESCRIPTION:
** 
**      State record information.
** 
** ENVIRONMENT: 
**
**      User space.
**
*/

#ifndef ps2_st_h
#define ps2_st_h

#include <irq.h>

typedef struct _ps2_st {
    irq_stub_st     stub;        /* generic stub state */

    Event_Count     event;
    
    uint8_t         kc_mode;

    bool_t          shft_down;
    bool_t          ctrl_down;

    bool_t          capslock;

    IDCCallback_cl  iocb_cl; 

    IO_clp          kbd_io;
    IO_clp          mouse_io;

    IOOffer_clp     keyboard_offer;
    IOOffer_clp     mouse_offer;
    int const      *keysymtab; /* xkeysym mapping table */
    int const      *e0keysymtab; /* escaped keysym table */

    bool_t          have_keyb;
    bool_t          have_mouse;

} ps2_st;

#endif /* ps2_st_h */
