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
**      Keyboard and mouse driver
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Drives PC keyboard and PS/2 mouse
** 
** ENVIRONMENT: 
**
**      User-mode device driver
**
*/

#include <nemesis.h>

#include <io.h>

#include <ecs.h>
#include <time.h>
#include <stdio.h>
#include <exceptions.h>

#include <IDCMacros.h>
#include <IOMacros.h>
#include <VPMacros.h>

#include <Kbd.ih>
#include <Mouse.ih>
#include <IO.ih>

#include <Interrupt.ih>

#define CONFIG_PS2_MOUSE

#ifndef SRCIT
#include "pckbd.h"
#define AUX_IRQ 12
#else
#include "itkbd.h"
#endif

#include "ps2_st.h"

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#include <keysym.h>


/* Default 100ms timeout */
#define TIMEOUT 1000
#define TIMEOUT_VAL 0x100

/* There is a single thread of control in this driver to avoid the need
   for locking */


/*
 * IDCCallback methods 
 */
static	IDCCallback_Request_fn 		IO_Request_m;
static	IDCCallback_Bound_fn 		IO_Bound_m;
static	IDCCallback_Closed_fn 		IO_Closed_m;

static	IDCCallback_op	iocb_ms = {
    IO_Request_m,
    IO_Bound_m,
    IO_Closed_m
};

#define HANDLE_MOUSE (word_t)0xf00f
#define HANDLE_KBD   (word_t)0xbead

/* Module stuff */
static Kbd_SetLEDs_fn Kbd_SetLEDs_m;
static Kbd_SetRepeat_fn Kbd_SetRepeat_m;
static Kbd_GetOffer_fn Kbd_GetOffer_m;
static Kbd_op kbd_op = {
    Kbd_SetLEDs_m,
    Kbd_SetRepeat_m,
    Kbd_GetOffer_m,
};

static Closure_Apply_fn Main;
static Closure_op cl_op = { Main };
static const Closure_cl cl = { &cl_op, NULL };
CL_EXPORT(Closure,PS2,cl);

static void ServiceThread(ps2_st *st);
extern const int keysymtab [128];
extern const int e0keysymtab [128];

/* 
** Currently, we need to initialize the keyboard specially on SRCIT.
*/
#ifdef SRCIT
#define INIT_KBD
#endif

#ifdef INIT_KBD
static int kbd_wait_for_input(void)
{
    int     n;
    int     status, data;
    uint32_t timeout=TIMEOUT;

    do {
	PAUSE(MICROSECS(100));
	status = inb(KC_STATUS);
	/*
	 * Wait for input data to become available.  This bit will
	 * then be cleared by the following read of the DATA
	 * register.
	 */

	if (!(status & KC_ST_OUTB))
	    continue;

	data = inb(KC_IN);

	/*
	 * Check to see if a timeout error has occurred.  This means
	 * that transmission was started but did not complete in the
	 * normal time cycle.  PERR is set when a parity error occurred
	 * in the last transmission.
	 */
	if (status & (KC_ST_TIM | KC_ST_PARE)) {
	    continue;
	}
	return (data & 0xff);
    } while (timeout--);

    return (-1);
}

static void kbd_write(int address, int data)
{
    int status;

    do {
	status = inb(KC_STATUS);
    } while (status & KC_ST_INPB);
    outb(data, address);
}

static char *initialize_kbd2(void)
{
    /* Flush any pending input. */

    while (kbd_wait_for_input() != -1)
	;

	/*
	 * Test the keyboard interface.
	 * This seems to be the only way to get it going.
	 * If the test is successful a x55 is placed in the input buffer.
	 */

    kbd_write(KC_CONTROL, KC_COM_SELFTEST);
    if (kbd_wait_for_input() != 0x55)
	return "Keyboard failed self test";

	/*
	 * Perform a keyboard interface test.  This causes the controller
	 * to test the keyboard clock and data lines.  The results of the
	 * test are placed in the input buffer.
	 */

    kbd_write(KC_CONTROL, KC_COM_READ_TEST);
    if (kbd_wait_for_input() != 0x00)
	return "Keyboard interface failed self test";

	/* Enable the keyboard by allowing the keyboard clock to run. */

    kbd_write(KC_CONTROL, KC_COM_ENABLE_KBD);

    /*
	 * Reset keyboard. If the read times out
	 * then the assumption is that no keyboard is
	 * plugged into the machine.
	 * This defaults the keyboard to scan-code set 2.
	 */

    kbd_write(KC_OUT, KBD_COM_RESET);
    if (kbd_wait_for_input() != KBD_RET_ACK)
	return "Keyboard reset failed, no ACK";
    if (kbd_wait_for_input() != KBD_RET_BATOK)
	return "Keyboard reset failed, no BATOK";

	/*
	 * Set keyboard controller mode. During this, the keyboard should be
	 * in the disabled state.
	 */

    kbd_write(KC_OUT, KC_COM_DISABLE_KBD);
    if (kbd_wait_for_input() != KBD_RET_ACK)
	return "Disable keyboard: no ACK";

    kbd_write(KC_CONTROL, KC_COM_WRITE_MODE);
    kbd_write(KC_OUT, KBD_MODE_KBD_INT
	      | KBD_MODE_SYS
	      | KBD_MODE_DISABLE_MOUSE
	      | KBD_MODE_KCC);

    kbd_write(KC_OUT, KC_COM_ENABLE_KBD);
    if (kbd_wait_for_input() != KBD_RET_ACK)
	return "Enable keyboard: no ACK";

	/*
	 * Finally, set the typematic rate to maximum.
	 */

    kbd_write(KC_OUT, KBD_COM_SETREP);
    if (kbd_wait_for_input() != KBD_RET_ACK)
	return "Set rate: no ACK";
    kbd_write(KC_OUT, 0x00);
    if (kbd_wait_for_input() != KBD_RET_ACK)
	return "Set rate: no ACK";

    return NULL;
}

static void initialize_kbd(void)
{
    char *msg;

    msg = initialize_kbd2();
    if (msg)
	printf("ps2: initialize_kbd: %s\n", msg);
}

#endif /* INIT_KBD */



/* Send a command to the keyboard controller */
static bool_t sendcommand(uint8_t command)
{
    uint32_t timeout=TIMEOUT;

    outb(command, KC_CONTROL);
    while((inb(KC_STATUS)&KC_ST_INPB) && --timeout) {
	PAUSE(MICROSECS(100));
    }
    return (timeout>0);
}

/* Send data to the keyboard controller */
static bool_t senddata(uint8_t data)
{
    uint32_t timeout=TIMEOUT;

    outb(data, KC_IN);
    while((inb(KC_STATUS) & KC_ST_INPB) && --timeout) {
	PAUSE(MICROSECS(100));
    }
    return (timeout>0);
}


#ifdef CONFIG_PS2_MOUSE
/*
** PS/2 mice are notoriously slow to answer; hence we must retry
** a number of times -- small-ish when we PAUSE() between tries, 
** large when we just bail at the device.
*/
#define MAX_SLEEP_RETRIES 60
#define MAX_NOSLEEP_RETRIES 1000000


static int poll_aux_status(void)
{
    int retries=0;

    while ((inb(KC_STATUS)&0x03) && retries < MAX_SLEEP_RETRIES) {
        if ((inb(KC_STATUS) & (KC_ST_OUTB | KC_ST_AUXB)) == 
	    (KC_ST_OUTB | KC_ST_AUXB))
            inb(KC_IN);
	PAUSE(MILLISECS(100));
        retries++;
    }
    return !(retries==MAX_SLEEP_RETRIES);
}

static int poll_aux_status_nosleep(void)
{
    int retries = 0;

    while ((inb(KC_STATUS)&0x03) && retries < MAX_NOSLEEP_RETRIES) {
        if ((inb(KC_STATUS) & (KC_ST_OUTB | KC_ST_AUXB)) == 
	    (KC_ST_OUTB | KC_ST_AUXB))
            inb(KC_IN);
        retries++;
    }
    return !(retries == MAX_NOSLEEP_RETRIES);
}

/*
** Write to aux device
*/
static void aux_write_dev(uint8_t data)
{
    poll_aux_status();
    outb(KC_COM_WRITE_AUX,KC_CONTROL);    /* write magic cookie */
    poll_aux_status();
    outb(data,KC_OUT);           /* write data */
}

/*
** Write to device & handle returned ack
*/
static int aux_write_ack(uint8_t data)
{
    poll_aux_status_nosleep();
    outb(KC_COM_WRITE_AUX,KC_CONTROL);
    poll_aux_status_nosleep();
    outb(data,KC_OUT);
    poll_aux_status_nosleep();

    if ((inb(KC_STATUS) & (KC_ST_OUTB | KC_ST_AUXB)) == 
	(KC_ST_OUTB | KC_ST_AUXB)) {
        return (inb(KC_IN));
    }
    return 0;
}

/*
** Write aux device command
*/
static void aux_write_cmd(uint8_t data)
{
    poll_aux_status();
    outb(KC_COM_WRITE_MODE,KC_CONTROL);
    poll_aux_status();
    outb(data,KC_OUT);
}
#endif /* CONFIG_PS2_MOUSE */

/* Read whatever is in the output buffer of the keyboard controller */
static uint32_t readoutput(void)
{
    uint32_t timeout=TIMEOUT;

    while ((!(inb(KC_STATUS)&KC_ST_OUTB)) && --timeout) {
	PAUSE(MICROSECS(100));
    }
    return (timeout>0 ? inb(KC_OUT) : TIMEOUT_VAL);
}

/* Send data to the keyboard, wait for ack, deal with retransmit */
static bool_t sendkbcommand(uint8_t data)
{
    uint32_t result;
    do {
	if (!senddata(data)) return False;
	result=readoutput();
    } while (result==KBD_COM_RESEND);

    return (result==0xfa);
}

static bool_t selftest(void)
{
    TRC(fprintf(stderr,"Keyboard controller self test...\n"));

    sendcommand(KC_COM_SELFTEST);
    PAUSE(MICROSECS(100));
    if (readoutput()!=0x55) {
	fprintf(stderr,"BOGOSITY: keyboard controller failed self test\n");
	return False;
    }
    TRC(fprintf(stderr,"Keyboard controller ok\n"));
    return True;
}

static bool_t checkinterface(uint8_t command)
{
    uint8_t res;

    sendcommand(command);
    res = readoutput();

    return (res==0);
}

static bool_t initialise_keyboard(ps2_st *st)
{
    uint8_t output, timeout;
    uint32_t result;

    sendcommand(KC_COM_ENABLE_KBD);

    /* Empty keyboard queue */
    while ((result=readoutput())!=TIMEOUT_VAL)
	PAUSE(MICROSECS(100));

    /* Reset keyboard */
    TRC(fprintf(stderr,"Resetting keyboard\n"));
    if (!sendkbcommand(KBD_COM_RESET)) {
	TRC(fprintf(stderr,"Failed to reset keyboard\n"));
	return False;
    }
    /* Put DATA and CLOCK high for 500us (a necessary response to the ACK) */
    sendcommand(KC_COM_READ_OUTPUT);
    output=readoutput();
    sendcommand(KC_COM_WRITE_OUTPUT);
    senddata(output | 0xc0);
    PAUSE(MICROSECS(500));
    sendcommand(KC_COM_WRITE_OUTPUT);
    senddata(output);

    /* Read result code from keyboard */
    timeout=100;
    do {
	result=readoutput();
    } while ((result == TIMEOUT_VAL) && timeout--);
    if (result==KBD_RET_BATERR) {
	fprintf(stderr,"Keyboard failed self test\n");
	return False;
    }
    if (result!=KBD_RET_BATOK) {
	fprintf(stderr,"Keyboard not present - result code %x\n",result);
	return False;
    }
    TRC(fprintf(stderr,"Keyboard passed self test\n"));

    if (!sendkbcommand(KBD_COM_STDENB)) {
	fprintf(stderr,"Failed to disable keyboard\n");
	return False;
    }

    /* Enable keyboard interrupt */
    st->kc_mode |= KC_M_EKI | KC_M_SYS | KC_M_DMS | KC_M_KCC;
    sendcommand(KC_COM_WRITE_MODE);
    senddata(st->kc_mode);

    /* Send an enable code - start keyboard scanning */
    if (!sendkbcommand(KBD_COM_ENABLE)) {
	fprintf(stderr,"Failed to enable keyboard\n");
	return False;
    }

    st->keysymtab = keysymtab;
    st->e0keysymtab = e0keysymtab;
    return True;
}

#ifdef CONFIG_PS2_MOUSE
static void initialise_mouse(ps2_st *st)
{
    TRC(fprintf(stderr,"About to initialise mouse...\n"));

    TRC(fprintf(stderr,"Enable aux\n"));
    outb(KC_COM_ENABLE_AUX,KC_CONTROL);     /* Enable Aux */
    TRC(fprintf(stderr,"Enable aux\n"));
    aux_write_ack(MOUSE_COM_SETSAMPLE);
    aux_write_ack(100);                     /* 100 samples/sec */
    aux_write_ack(MOUSE_COM_SETRES);
    aux_write_ack(3);                       /* 8 counts per mm */
    aux_write_ack(MOUSE_COM_SETSCL);        /* 2:1 scaling */
    TRC(fprintf(stderr,"poll aux status nosleep\n"));
    poll_aux_status_nosleep();
    TRC(fprintf(stderr,"KC_COM_DISABLE_AUX, KC_CONTROL\n"));
    outb_p(KC_COM_DISABLE_AUX,KC_CONTROL);  /* Disable Aux device */
    TRC(fprintf(stderr,"poll aux status nosleep\n"));
    poll_aux_status_nosleep();
    TRC(fprintf(stderr,"KC_COM_WRITE_MODE, KC_CONTROL\n"));
    outb_p(KC_COM_WRITE_MODE,KC_CONTROL);
    TRC(fprintf(stderr,"poll aux status nosleep\n"));
    poll_aux_status_nosleep();              /* Disable interrupts */
    TRC(fprintf(stderr,"AUX_INTS_OFF, KC_OUT\n"));
    outb_p(AUX_INTS_OFF, KC_OUT);           /* on the controller */

    TRC(fprintf(stderr,"Done mouse\n"));
}
#endif /* CONFIG_PS2_MOUSE */

static void Kbd_SetLEDs_m(Kbd_clp self, Kbd_LEDs leds)
{
    uint8_t l=0;

    ENTER_KERNEL_CRITICAL_SECTION();
    ntsc_mask_irq(KC_IRQ);

    senddata(KBD_COM_LED);
    if (SET_IN(leds, Kbd_LED_CapsLock)) l|=2;
    readoutput();
    senddata(l);
    readoutput();

    ntsc_unmask_irq(KC_IRQ);
    LEAVE_KERNEL_CRITICAL_SECTION();
}

static void Kbd_SetRepeat_m(Kbd_clp self, uint32_t delay, uint32_t rate)
{
    uint8_t d, r;

    d=3;
    if (delay<750) d=2;
    if (delay<500) d=1;
    if (delay<250) d=0;

    r=0;
    if (rate<=267) r=1;
    if (rate<=240) r=2;
    if (rate<=218) r=3;
    if (rate<=200) r=4;
    if (rate<=185) r=5;
    if (rate<=171) r=6;
    if (rate<=160) r=7;
    if (rate<=150) r=8;
    if (rate<=133) r=9;
    if (rate<=120) r=10;
    if (rate<=109) r=11;
    if (rate<=100) r=12;
    if (rate<= 92) r=13;
    if (rate<= 85) r=14;
    if (rate<= 80) r=15;
    if (rate<= 75) r=16;
    if (rate<= 67) r=17;
    if (rate<= 60) r=18;
    if (rate<= 55) r=19;
    if (rate<= 50) r=20;
    if (rate<= 46) r=21;
    if (rate<= 43) r=22;
    if (rate<= 40) r=23;
    if (rate<= 37) r=24;
    if (rate<= 33) r=25;
    if (rate<= 30) r=26;
    if (rate<= 27) r=27;
    if (rate<= 25) r=28;
    if (rate<= 23) r=29;
    if (rate<= 21) r=30;
    if (rate<= 20) r=31;

    ENTER_KERNEL_CRITICAL_SECTION();
    ntsc_mask_irq(KC_IRQ);
    LEAVE_KERNEL_CRITICAL_SECTION();

    sendkbcommand(KBD_COM_SETREP);
    sendkbcommand(r | (d<<5));

    ENTER_KERNEL_CRITICAL_SECTION();
    ntsc_unmask_irq(KC_IRQ);
    LEAVE_KERNEL_CRITICAL_SECTION();
}

static IOOffer_clp Kbd_GetOffer_m(Kbd_clp self)
{
    ps2_st *st=self->st;

    return st->keyboard_offer;
}


#ifdef CONFIG_PS2_MOUSE
static void BuildEv(Mouse_Event *ev, uint8_t *data)
{
    ev->time = NOW();
    ev->buttons =
    	(data[0] & 0x01 ? SET_ELEM(Mouse_Button_Left) : 0) |
    	(data[0] & 0x04 ? SET_ELEM(Mouse_Button_Middle) : 0) |
	(data[0] & 0x02 ? SET_ELEM(Mouse_Button_Right) : 0);
    ev->dx = (data[0] & 0x10) ? data[1]-256 : data[1];
    ev->dy = (data[0] & 0x20) ? (data[2]-256) : data[2];
}
#endif /* CONFIG_PS2_MOUSE */


/* Callbacks for our mouse and keyboard IOOffers */
static bool_t IO_Request_m (IDCCallback_cl *self, IDCOffer_clp offer,
			    Domain_ID dom, ProtectionDomain_ID pdid,
			    const Binder_Cookies *clt_cks, 
			    Binder_Cookies *svr_cks)
{
    return True;
}

static bool_t IO_Bound_m (IDCCallback_cl *self, IDCOffer_clp offer,
			  IDCServerBinding_clp binding, Domain_ID dom,
			  ProtectionDomain_ID pdid, 
			  const Binder_Cookies *clt_cks, 
			  Type_Any *server)    
{
    ps2_st       *st = self->st;
    IO_cl        *io = NARROW(server, IO_clp);

    if(IO_HANDLE(io) == HANDLE_MOUSE) {
	/* For the mouse, we have a first-come/first-served policy */
	if(st->mouse_io != NULL)
	    return False;
	st->mouse_io = io; 
	return True;
    }

    if(IO_HANDLE(io) == HANDLE_KBD) {
	/* 
	** For the keyboard, we take the latest binding, and oust
	** the previous client. 
	** XXX SMH: this is in order that the current x86
	** default stuff will work (with ttyterm binding to the 
	** keyboard first, but the wssvr binding to it later -- at
	** this point ttyterm is effectively useless and hence we 
	** do no damage by throwing away its binding).
	** However we should sort it out properly. 
	*/
	if(st->kbd_io != NULL)
	    IO$Dispose(st->kbd_io);
	st->kbd_io = io; 
	return True;
    }

    return False; 
}

static void IO_Closed_m (IDCCallback_cl *self, IDCOffer_clp offer,
			 IDCServerBinding_clp binding, 
			 const Type_Any *server)
{
    ps2_st       *st = self->st;
    IO_cl        *io = NARROW(server, IO_clp);

    if(IO_HANDLE(io) == HANDLE_MOUSE)
	st->mouse_io = NULL; 
    else if(IO_HANDLE(io) == HANDLE_KBD) 
	st->kbd_io = NULL; 

    return;
}

static void Main(Closure_clp self)
{
    ps2_st       *st;

    TRC(fprintf(stderr,"PS/2 keyboard/mouse driver: started\n"));

#ifdef INIT_KBD
    initialize_kbd();
#endif

    if (!selftest()) return;

    st = Heap$Malloc(Pvs(heap), sizeof(*st));

    st->have_keyb = checkinterface(KC_COM_CHECK_KBD);
#ifdef CONFIG_PS2_MOUSE
    st->have_mouse = checkinterface(KC_COM_CHECK_AUX);
#else
    st->have_mouse = False;
#endif

    //    TRC(if (have_keyb) fprintf(stderr,"Keyboard interface ok\n"));
#ifdef CONFIG_PS2_MOUSE
    // TRC(if (have_mouse) fprintf(stderr,"Mouse interface ok\n"));
#endif

    /* Fill in initial state */
    st->stub.dcb  = RO(Pvs(vp));
    st->stub.ep   = Events$AllocChannel(Pvs(evs));
    st->event     = EC_NEW();
    Events$Attach(Pvs(evs), st->event, st->stub.ep, Channel_EPType_RX);

    st->kbd_io    = NULL;
    st->mouse_io  = NULL;

    st->shft_down = False;
    st->ctrl_down = False;
    st->capslock  = False;
    
    st->kc_mode   = KC_M_SYS | KC_M_KCC;

    CLP_INIT(&st->iocb_cl, &iocb_ms, st);

    /* Initialise the keyboard */
    if (st->have_keyb) 
	st->have_keyb = initialise_keyboard(st);

#ifdef CONFIG_PS2_MOUSE
    /* Initialise the mouse */
    if (st->have_mouse) initialise_mouse(st);
#endif

    if (!(st->have_keyb || st->have_mouse)) {
	TRC(fprintf(stderr,"No keyboard or PS/2 mouse detected\n"));
	return;
    } 

    { 
	/* Register the interrupt stubs */

	Interrupt_clp icl;
	TRC(fprintf(stderr,"Registering interrupt stubs\n"));

	icl = IDC_OPEN("sys>InterruptOffer", Interrupt_clp);
	if(st->have_keyb) {
	    if(Interrupt$Allocate(icl, KC_IRQ, _generic_stub, st)
	       !=Interrupt_AllocCode_Ok) {
		fprintf(stderr,"keyboard: failed to allocate interrupt\n");
		return;
	    } else { 
		TRC(fprintf(stderr, "ps2: alloc'd irq %d for keyboard.\n", 
			    KC_IRQ));
	    }
	}
#ifdef CONFIG_PS2_MOUSE
	if(st->have_mouse) {
	    if (Interrupt$Allocate(icl, AUX_IRQ, _generic_stub, st)
		!=Interrupt_AllocCode_Ok) {
		fprintf(stderr,"psmouse: failed to allocate interrupt\n");
		return;
	    } else { 
		TRC(fprintf(stderr, "ps2: alloc'd irq %d for mouse.\n", 
			    AUX_IRQ));
	    }
	}
#endif /* CONFIG_PS2_MOUSE */
    }

    /* Create our IOOffers */
    {
	IOTransport_clp iot;
	IDCService_clp service;
	IOData_Shm     *shm;

	/* First find the IOTransport closure */
	iot = NAME_FIND("modules>IOTransport",IOTransport_clp);

	/* Now setup the shm requirements; we use the same for both
	   keyboard and mouse */
	shm = SEQ_NEW(IOData_Shm, 1, Pvs(heap));
	SEQ_ELEM(shm, 0).buf.base     = NO_ADDRESS; /* not yet allocated */
	SEQ_ELEM(shm, 0).buf.len      = 4096; 
	SEQ_ELEM(shm, 0).param.attrs  = 0;
	SEQ_ELEM(shm, 0).param.awidth = PAGE_WIDTH;
	SEQ_ELEM(shm, 0).param.pwidth = PAGE_WIDTH;

	if (st->have_keyb) {
	    Kbd_clp kbd;

	    kbd     = Heap$Malloc(Pvs(heap), sizeof(*kbd));
	    kbd->op = &kbd_op;
	    kbd->st = st;

	    st->keyboard_offer = IOTransport$Offer(iot, Pvs(heap), 15, 
						   IO_Mode_Rx, IO_Kind_Slave, 
						   shm, Pvs(gkpr), 
						   &st->iocb_cl, NULL, 
						   HANDLE_KBD, &service);


	    /* XXX hack - don't know why this works */
	    PAUSE(MILLISECS(200));
	    Kbd$SetRepeat(kbd, 250, 15);
	    TRC(fprintf(stderr,"Exporting kbd offer\n"));
	    IDC_EXPORT("dev>kbd", Kbd_clp, kbd);
	    TRC(fprintf(stderr,"Exported kbd offer\n"));
	}

#ifdef CONFIG_PS2_MOUSE
	if (st->have_mouse) {
	    IOOffer_clp offer; 
	    Type_Any any; 

	    TRC(fprintf(stderr,"Exporting mouse offer\n"));
	    offer = IOTransport$Offer(iot, Pvs(heap), 15, IO_Mode_Rx, 
				      IO_Kind_Slave, shm, Pvs(gkpr), 
				      &st->iocb_cl, NULL, HANDLE_MOUSE, 
				      &service);

	    ANY_INIT(&any, IOOffer_clp, offer); 
	    Context$Add(Pvs(root), "dev>mouse_ps2", &any);

	    TRC(fprintf(stderr,"Calling poll aux status\n"));
	    poll_aux_status();
	    TRC(fprintf(stderr,"KC_COM_ENABLE_AUX on KC_CONTROL\n"));
	    outb(KC_COM_ENABLE_AUX,KC_CONTROL);   /* Enable Aux */
	    TRC(fprintf(stderr,"Enable aux device\n"));
	    aux_write_dev(MOUSE_COM_ENABLE);      /* enable aux device */
	    TRC(fprintf(stderr,"Enable controller interrupts\n"));
	    aux_write_cmd(AUX_INTS_ON);           /* enable controller ints */
	    poll_aux_status();
	    TRC(fprintf(stderr,"Exported mouse offer\n"));
	}
#endif /* CONFIG_PS2_MOUSE */
    }

    /* Unmask our irqs */
    ENTER_KERNEL_CRITICAL_SECTION();
    if(st->have_keyb) ntsc_unmask_irq(KC_IRQ);
#ifdef CONFIG_PS2_MOUSE
    if(st->have_mouse) ntsc_unmask_irq(AUX_IRQ);
#endif
    LEAVE_KERNEL_CRITICAL_SECTION();

    /* Fork a thread to service the device */
    Threads$Fork(Pvs(thds),ServiceThread,st,0);
    return;
}

static void ServiceThread(ps2_st *st)
{
    Event_Val     cur  = 0;
    uint8_t       stat, mc, byte, esc = 0;
    Kbd_Event    *kev;
    IO_Rec        rec;
    uint32_t      nrecs, the_key;
    word_t        value; 

#ifdef CONFIG_PS2_MOUSE
    Mouse_Event  *ev, temp;
    uint8_t       mbuf[8];
    uint32_t      mp = 0;
#endif


    /* And now wait for interrupts... forever */
    while(1) {
	TRC(fprintf(stderr,"ps2: awaiting interrupt\n"));
	EC_AWAIT(st->event, cur);
	cur++;
	TRC(fprintf(stderr,"ps2: interrupt\n"));
	while ( ((stat=inb(KC_STATUS))&KC_ST_OUTB) == KC_ST_OUTB) {
	    if (stat & KC_ST_AUXB) {
		/* It's a mouse byte */
		
		mc = inb(KC_OUT);

		TRC(fprintf(stderr,"ps2: mouse byte %02x\n",mc));
		
#ifdef CONFIG_PS2_MOUSE
		/* Header */
		if (!mp && ((mc & 0xC0) != 0x00))
		    continue;

		mbuf[mp++]= mc;

		if (mp==3) {
		    /* We have a complete mouse packet; send it if poss */

		    TRC(fprintf(stderr,"complete mouse packet received\n"));

		    if (st->mouse_io && IO$GetPkt(st->mouse_io, 1, &rec, 0,
						  &nrecs, &value)) {
			ev=(void *)rec.base;
			BuildEv(ev, mbuf);
			IO$PutPkt(st->mouse_io, 1, &rec, 1, 0);
		    } else {
			ev=&temp;
			BuildEv(ev, mbuf);
			TRC(fprintf(stderr,"Mouse packet: %c%c%c "
				    "%d %d\n",
				    SET_IN(ev->buttons, Mouse_Button_Left)?
				    'L':'-',
				    SET_IN(ev->buttons, Mouse_Button_Middle)?
				    'M':'-',
				    SET_IN(ev->buttons, Mouse_Button_Right)?
				    'R':'-',
				    ev->dx, ev->dy));
		    }
		    mp=0;
		}
#else /* !CONFIG_PS2_MOUSE */
		fprintf(stderr,"ps2: unexpected aux port byte;\n"
			" + is there a mouse in the house?\n");
#endif /* CONFIG_PS2_MOUSE */
	    } else {
		/* It's a keyboard byte */
		byte=inb(KC_OUT);

		TRC(fprintf(stderr,"ps2: kbd byte %02x\n",byte));
		if (stat & KC_ST_PARE) {
		    senddata(KBD_COM_RESEND);
		    fprintf(stderr,"keyboard: parity error\n");
		} else {
		    switch (byte) {
		    case KBD_RET_OVF:
			fprintf(stderr,"keyboard: overflow\n");
			break;
		    case KBD_RET_ERR:
			fprintf(stderr,"keyboard: error\n");
			break;
		    case KBD_RET_ECHO:
			fprintf(stderr,"keyboard: unexpected echo\n");
			break;
		    case KBD_RET_ACK:
			TRC(fprintf(stderr,"keyboard: unexpected ack\n"));
			break;
		    case KBD_RET_RESEND:
			fprintf(stderr,"keyboard: resend request received\n");
			break;
		    case 0xe0:
		      TRC(fprintf(stderr,"ps2: got escaped scancode.\n"));
		      esc = byte;
		      break;
		    default:
			if (st->kbd_io) {
			    if (IO$GetPkt(st->kbd_io, 1, &rec, 0, 
					  &nrecs, &value)) {
				TRC(fprintf(stderr,"ps2: got pkt\n"));
				kev=(void *)rec.base;
				kev->time = NOW();

				if (esc == 0xe0) the_key = st->e0keysymtab[(byte&0x7f)];
				else the_key = st->keysymtab[(byte&0x7f)];
				
				if(byte & 0x80) {

				    kev->state = Kbd_State_Up;

				    if (st->shft_down && 
					(the_key == XK_Shift_L ||
					 the_key == XK_Shift_R) ) 
					st->shft_down = False;

				    if (st->ctrl_down && 
					(the_key == XK_Control_L ||
					 the_key == XK_Control_R) ) 
					st->ctrl_down = False;

				    if (the_key == XK_Caps_Lock)
					st->capslock = !st->capslock;

				} else {

				    kev->state = Kbd_State_Down;

				    if (!st->shft_down && 
					(the_key == XK_Shift_L ||
					 the_key == XK_Shift_R) ) 
					st->shft_down = True;

				    if (!st->ctrl_down && 
					(the_key == XK_Control_L ||
					 the_key == XK_Control_R) ) 
					st->ctrl_down = True;

				}


#define SHIFT(_x)      (st->keysymtab[(_x)+128])
#define CTRL(_x)       (st->keysymtab[(_x)+256])
#define CTRL_SHIFT(_x) (st->keysymtab[(_x)+384])
				if(st->shft_down && st->ctrl_down && !esc) 
				    kev->key = CTRL_SHIFT(byte & 0x7F);
				else if(st->ctrl_down && !esc)
				    kev->key = CTRL(byte & 0x7F);
				else if(st->shft_down && !esc)
				    kev->key = SHIFT(byte & 0x7F);
				else {

				    if (st->capslock && the_key >= XK_a 
					&& the_key <= XK_z) {
					the_key -= (XK_a - XK_A);
				    }
				    kev->key = the_key;
				}
				TRC(fprintf(stderr,
"Mapped scan code %x to keysym %x\n", (byte & 0x7f), kev->key));
				IO$PutPkt(st->kbd_io, 1, &rec, 1, 0);
				esc = 0;
			    } else {
				TRC(fprintf(stderr,
					    "keyboard: dropped event\n"));
			    }
			} else {
			    TRC(fprintf(stderr, "keyboard: ignored event.\n"));
			}
		    } /* switch */
		} /* if PARE */
	    } /* if AUXB */
	} /* while data available */

	/* unmask the keyboard and mouse interrupts again */
	ENTER_KERNEL_CRITICAL_SECTION();
	if(st->have_keyb) ntsc_unmask_irq(KC_IRQ);
#ifdef CONFIG_PS2_MOUSE
	if(st->have_mouse) ntsc_unmask_irq(AUX_IRQ);
#endif
	LEAVE_KERNEL_CRITICAL_SECTION();
    }
 
    /* NOT REACHED */
}
