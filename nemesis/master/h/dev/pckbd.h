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
**      PC keyboard controller header
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Definitions for controlling PC keyboards and PS/2 mice
** 
** ENVIRONMENT: 
**
**      User mode device driver
** 
** ID : $Id: pckbd.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef pckbd_h
#define pckbd_h

#define KC_OUT     0x60 /* read */
#define KC_IN      0x60 /* write */
#define KC_CONTROL 0x64 /* write */
#define KC_STATUS  0x64 /* read */

#define KC_IRQ 1

#define KC_ST_OUTB 0x01 /* Output buffer state */
#define KC_ST_INPB 0x02 /* Input buffer state */
#define KC_ST_SYSF 0x04 /* System flag */
#define KC_ST_CD   0x08 /* Command/data */
#define KC_ST_KEYL 0x10 /* Keyboard lock status */
#define KC_ST_AUXB 0x20 /* Output buffer for aux device */
#define KC_ST_TIM  0x40 /* Timeout */
#define KC_ST_PARE 0x80 /* Parity error */

#define KC_COM_WRITE_MODE   0x60
#define KC_COM_DISABLE_AUX  0xa7
#define KC_COM_ENABLE_AUX   0xa8
#define KC_COM_CHECK_AUX    0xa9
#define KC_COM_SELFTEST     0xaa
#define KC_COM_CHECK_KBD    0xab
#define KC_COM_DISABLE_KBD  0xad
#define KC_COM_ENABLE_KBD   0xae
#define KC_COM_READ_INPUT   0xc0
#define KC_COM_READOUT_LO   0xc1
#define KC_COM_READOUT_HI   0xc2
#define KC_COM_READ_OUTPUT  0xd0
#define KC_COM_WRITE_OUTPUT 0xd1
#define KC_COM_WRITE_KBDOUT 0xd2
#define KC_COM_WRITE_AUXOUT 0xd3
#define KC_COM_WRITE_AUX    0xd4
#define KC_COM_READ_TEST    0xe0

#define KC_M_EKI  0x01
#define KC_M_EMI  0x02
#define KC_M_SYS  0x04
#define KC_M_DMS  0x20
#define KC_M_KCC  0x40

#define KBD_COM_LED     0xed
#define KBD_COM_ECHO    0xee
#define KBD_COM_SETSCAN 0xf0
#define KBD_COM_ID      0xf2
#define KBD_COM_SETREP  0xf3
#define KBD_COM_ENABLE  0xf4
#define KBD_COM_STDDIS  0xf5
#define KBD_COM_STDENB  0xf6
#define KBD_COM_RESEND  0xfe
#define KBD_COM_RESET   0xff

#define KBD_RET_OVF    0x00
#define KBD_RET_ERR    0xff
#define KBD_RET_ID1    0x41
#define KBD_RET_ID2    0xab
#define KBD_RET_BATOK  0xaa
#define KBD_RET_ECHO   0xee
#define KBD_RET_ACK    0xfa
#define KBD_RET_BATERR 0xfc
#define KBD_RET_RESEND 0xfe

#define MOUSE_COM_RESETSCL  0xe6
#define MOUSE_COM_SETSCL    0xe7
#define MOUSE_COM_SETRES    0xe8
#define MOUSE_COM_GETSTAT   0xe9
#define MOUSE_COM_SETSTREAM 0xea
#define MOUSE_COM_READDATA  0xeb
#define MOUSE_COM_RESWRAP   0xec
#define MOUSE_COM_SETWRAP   0xee
#define MOUSE_COM_SETREM    0xf0
#define MOUSE_COM_IDENTIFY  0xf2
#define MOUSE_COM_SETSAMPLE 0xf3
#define MOUSE_COM_ENABLE    0xf4
#define MOUSE_COM_DISABLE   0xf5
#define MOUSE_COM_SETSTD    0xf6
#define MOUSE_COM_RESEND    0xfe
#define MOUSE_COM_RESET     0xff

#define AUX_INTS_ON   0x47        /* enable controller interrupts */
#define AUX_INTS_OFF  0x65        /* disable controller interrupts */




#endif /* pckbd_h */
