/*
*****************************************************************************
**                                                                          *
**             Copyright (C) 1994,1995 Digital Equipment Corporation
**                          All Rights Reserved
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      dev/pci/oppo
**
** FUNCTIONAL DESCRIPTION:
** 
**      Oppo device driver
** 
**
*/

#define NOTTO 1

#define DEBUG_OUTPUT(stream, args...)   \
{\
     char out[256];\
     sprintf(out, args);\
     fprintf(stream, "Otto: " __FUNCTION__ " : %s", out);\
}
    
#define DEBUG_OUT(args...) DEBUG_OUTPUT(stdout, args)
#define DEBUG_ERR(args...) DEBUG_OUTPUT(stderr, args)
#define PRINT_RX(args...) 
#define PRINT_TX(args...) 
#define PRINT_IP(args...) 
#define PRINT_ERR(args...) DEBUG_ERR(args)
#define PRINT_DATA(args...)
#define PRINT_STATS(args...) DEBUG_OUT(args)
#define PRINT_INIT(args...) DEBUG_OUT(args)
#define PRINT_NORM(args...) DEBUG_OUT(args)
