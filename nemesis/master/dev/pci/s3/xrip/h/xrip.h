/*
*****************************************************************************
**                                                                          *
**  Copyright 1995, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.		                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      dev/pci/s3/xrip/h
**
**
*/

/* xrip.h Dickon Reed */

#ifndef XRIP

#ifdef EB164
#define SCREEN_MEM_BASE  0x02000000
#else
#define SCREEN_MEM_BASE  0xF2000000
#endif

#define SPANBYTES (1024>>1)
#define SETPIXEL16(x,y,c) do { \
      uint32_t *ptr; \
      ptr = SCREEN_MEM_BASE; \
      ptr += SPANBYTES *(y); \
      ptr += (x)>>1; \
      if ((x) & 1) *ptr = (*ptr & 0xffff) | ((c)<<16); \
      else       *ptr = (*ptr & 0xffff0000) | (c); \
						       } while(0)
									      
#include <stdio.h>
#include "compiler.h"
#define XRIP

#define NeedFunctionPrototypes 1

#include "Ti302X.h"
#include "IBMRGB.h"
#define S3

#define TRUE 1
#define FALSE 0
#ifndef NULL
#define NULL 0
#endif
/* Values for vgaInterlaceType */
#define VGA_NO_DIVIDE_VERT     0
#define VGA_DIVIDE_VERT        1





typedef int *pointer;
#undef Bool /* nemesis_types.h defines Bool as an alias for bool_t */
typedef int Bool;
typedef int INT32; /* XXX */
typedef unsigned int CARD32; /* XXX */
typedef unsigned short CARD16; /* XXX */
typedef struct {
    CARD16 red, green, blue, pad;
} xrgb;
typedef CARD32 Pixel;
typedef void *xf86InfoRec;
typedef void * ScreenPtr;
typedef struct {
  unsigned char MiscOutReg;     /* */
  unsigned char CRTC[25];       /* Crtc Controller */
  unsigned char Sequencer[5];   /* Video Sequencer */
  unsigned char Graphics[9];    /* Video Graphics */
  unsigned char Attribute[21];  /* Video Atribute */
  unsigned char DAC[768];       /* Internal Colorlookuptable */
  char NoClock;                 /* number of selected clock */
  pointer FontInfo1;            /* save area for fonts in plane 2 */ 
  pointer FontInfo2;            /* save area for fonts in plane 3 */ 
  pointer TextInfo;             /* save area for text */ 
} vgaHWRec, *vgaHWPtr;

#ifdef S3
#include "../s3/regs3.h"
#include "../s3/s3consts.h"
#include "../s3/s3Bt485.h"
#include "../s3/s3ELSA.h"
#include "../s3/s3linear.h"
#define S3_SERVER
#define WaitQueue16_32(n16,n32) \
         if (s3InfoRec.bitsPerPixel == 32) { \
            WaitQueue(n32); \
         } \
         else \
            WaitQueue(n16)

#endif
#include "Ti302X.h"
#include "xf86_Option.h"


#define MAX_HSYNC 8
#define MAX_VREFRESH 8

/*
 * structure common for all modes
 */
typedef struct _DispM {
  struct _DispM	*prev,*next;
  char		*name;              /* identifier of this mode */
  /* These are the values that the user sees/provides */
  int		Clock;              /* pixel clock */
  int           HDisplay;           /* horizontal timing */
  int           HSyncStart;
  int           HSyncEnd;
  int           HTotal;
  int           VDisplay;           /* vertical timing */
  int           VSyncStart;
  int           VSyncEnd;
  int           VTotal;
  int           Flags;
  /* These are the values the hardware uses */
  int		SynthClock;         /* Actual clock freq to be programmed */
  int		CrtcHDisplay;
  int		CrtcHSyncStart;
  int		CrtcHSyncEnd;
  int		CrtcHTotal;
  int		CrtcVDisplay;
  int		CrtcVSyncStart;
  int		CrtcVSyncEnd;
  int		CrtcVTotal;
  Bool		CrtcHAdjusted;
  Bool		CrtcVAdjusted;
  int		PrivSize;
  INT32 	*Private;
} DisplayModeRec, *DisplayModePtr;


typedef struct { float hi, lo; } range;

typedef struct {
   char *id;
   char *vendor;
   char *model;
   float bandwidth;
   int n_hsync;
   range hsync[MAX_HSYNC];       
   int n_vrefresh;                  
   range vrefresh[MAX_VREFRESH];
   DisplayModePtr Modes, Last; /* Start and end of monitor's mode list */
} MonRec, *MonPtr;

#define MAXCLOCKS   128


/*
 * the graphic device
 */
typedef struct {
  Bool           configured;
  int            tmpIndex;
  int            scrnIndex;
  Bool           (* Probe)();
  Bool           (* Init)();
  Bool           (* ValidMode)();
  void           (* EnterLeaveVT)(
#if NeedFunctionPrototypes
    int,
    int
#endif
);
  void           (* EnterLeaveMonitor)(
#if NeedVarargsPrototypes
    int
#endif
);
  void           (* EnterLeaveCursor)(
#if NeedVarargsPrototypes
    int
#endif
);
  void           (* AdjustFrame)();
  Bool           (* SwitchMode)();
  void           (* PrintIdent)();
  int            depth;
  xrgb		 weight; 
  int            bitsPerPixel;
  int            defaultVisual;
  int            virtualX,virtualY; 
  int		 displayWidth;
  int            frameX0, frameY0, frameX1, frameY1;
  OFlagSet       options;
  OFlagSet       clockOptions;
  OFlagSet	 xconfigFlag;
  char           *chipset;
  char           *ramdac;
  int            dacSpeed;
  int            clocks;
  int            clock[MAXCLOCKS];
  int            maxClock;
  int            videoRam;
  int            BIOSbase;                 /* Base address of video BIOS */
  unsigned long  MemBase;                  /* Frame buffer base address */
  int            width, height;            /* real display dimensions */
  unsigned long  speedup;                  /* Use SpeedUp code */
  DisplayModePtr modes; 
  MonPtr         monitor;
  char           *clockprog;
  int            textclock;
  Bool           bankedMono;	  /* display supports banking for mono server */
  char           *name;
  xrgb           blackColour;
  xrgb           whiteColour; 
  int            *validTokens;
  char           *patchLevel;
  unsigned int   IObase;          /* AGX - video card I/O reg base        */
  unsigned int   DACbase;         /* AGX - dac I/O reg base               */
  unsigned long  COPbase;         /* AGX - coprocessor memory base        */
  unsigned int   POSbase;         /* AGX - I/O address of POS regs        */
  int            instance;        /* AGX - XGA video card instance number */
  int            s3Madjust;
  int            s3Nadjust;
  int            s3MClk;
  unsigned long  VGAbase;         /* AGX - 64K aperture memory addreee    */
  int            s3RefClk;
  int            suspendTime;
  int            offTime;
  int            s3BlankDelay;
} ScrnInfoRec, *ScrnInfoPtr;

typedef struct {
  int           token;                /* id of the token */
  char          *name;                /* pointer to the LOWERCASED name */
} SymTabRec, *SymTabPtr;

typedef struct {
   char *identifier;
   char *vendor;
   char *board;
   char *chipset;
   char *ramdac;
   int dacSpeed;
   int clocks;
   int clock[MAXCLOCKS];
   OFlagSet options;
   OFlagSet clockOptions;
   OFlagSet xconfigFlag;
   int videoRam;
   unsigned long speedup;
   char *clockprog;
   int textClockValue;
   int BIOSbase;                 /* Base address of video BIOS */
   unsigned long MemBase;        /* Frame buffer base address */
   unsigned int  IObase;
   unsigned int  DACbase;
   unsigned long COPbase;
   unsigned int  POSbase;
   int instance;
   int s3Madjust;
   int s3Nadjust;
   int s3MClk;
   unsigned long VGAbase;      /* VGA ot XGA 64K aperature base address */
   int s3RefClk;
   int s3BlankDelay;
} GDevRec, *GDevPtr;

typedef struct {
   int depth;
   xrgb weight;
   int frameX0;
   int frameY0;
   int virtualX;
   int virtualY;
   DisplayModePtr modes;
   xrgb whiteColour;
   xrgb blackColour;
   int defaultVisual;
   OFlagSet options;
   OFlagSet xconfigFlag;
} DispRec, *DispPtr;


typedef struct {
   Bool (*ChipProbe)();
   char *(*ChipIdent)();
   void (*ChipEnterLeaveVT)();
   Bool (*ChipInitialize)();
   void (*ChipAdjustFrame)();
   Bool (*ChipSwitchMode)();
} s3VideoChipRec, *s3VideoChipPtr;

typedef struct _miPointerScreenFuncRec {
    Bool        (*CursorOffScreen)();   /* ppScreen, px, py */
    void        (*CrossScreen)();       /* pScreen, entering */
    void        (*WarpCursor)();        /* pScreen, x, y */
    void        (*EnqueueEvent)();      /* xEvent */
    void        (*NewEventScreen)();    /* pScreen */
} miPointerScreenFuncRec, *miPointerScreenFuncPtr;

struct pci_config_reg {
    /* start of official PCI config space header */
    union {
        unsigned long device_vendor;
	struct {
	    unsigned short vendor;
	    unsigned short device;
	} dv;
    } dv_id;
#define _device_vendor dv_id.device_vendor
#define _vendor dv_id.dv.vendor
#define _device dv_id.dv.device
    union {
        unsigned long status_command;
	struct {
	    unsigned short command;
	    unsigned short status;
	} sc;
    } stat_cmd;
#define _status_command stat_cmd.status_command
#define _command stat_cmd.sc.command
#define _status  stat_cmd.sc.status
    union {
        unsigned long class_revision;
	struct {
	    unsigned char rev_id;
	    unsigned char prog_if;
	    unsigned char sub_class;
	    unsigned char base_class;
	} cr;
    } class_rev;
#define _class_revision class_rev.class_revision
#define _rev_id     class_rev.cr.rev_id
#define _prog_if    class_rev.cr.prog_if
#define _sub_class  class_rev.cr.sub_class
#define _base_class class_rev.cr.base_class
    union {
        unsigned long bist_header_latency_cache;
	struct {
	    unsigned char cache_line_size;
	    unsigned char latency_timer;
	    unsigned char header_type;
	    unsigned char bist;
	} bhlc;
    } bhlc;
#define _bist_header_latency_cache bhlc.bist_header_latency_cache
#define _cache_line_size bhlc.bhlc.cache_line_size
#define _latency_timer   bhlc.bhlc.latency_timer
#define _header_type     bhlc.bhlc.header_type
#define _bist            bhlc.bhlc.bist
    union {
	struct {
	    unsigned long dv_base0;
	    unsigned long dv_base1;
	    unsigned long dv_base2;
	    unsigned long dv_base3;
	    unsigned long dv_base4;
	    unsigned long dv_base5;
	} dv;
	struct {
	    unsigned long bg_rsrvd[2];
	    unsigned char primary_bus_number;
	    unsigned char secondary_bus_number;
	    unsigned char subordinate_bus_number;
	    unsigned char secondary_latency_timer;
	    unsigned char io_base;
	    unsigned char io_limit;
	    unsigned short secondary_status;
	    unsigned short mem_base;
	    unsigned short mem_limit;
	    unsigned short prefetch_mem_base;
	    unsigned short prefetch_mem_limit;
	} bg;
    } bc;
#define	_base0				bc.dv.dv_base0
#define	_base1				bc.dv.dv_base1
#define	_base2				bc.dv.dv_base2
#define	_base3				bc.dv.dv_base3
#define	_base4				bc.dv.dv_base4
#define	_base5				bc.dv.dv_base5
#define	_primary_bus_number		bc.bg.primary_bus_number
#define	_secondary_bus_number		bc.bg.secondary_bus_number
#define	_subordinate_bus_number		bc.bg.subordinate_bus_number
#define	_secondary_latency_timer	bc.bg.secondary_latency_timer
#define _io_base			bc.bg.io_base
#define _io_limit			bc.bg.io_limit
#define _secondary_status		bc.bg.secondary_status
#define _mem_base			bc.bg.mem_base
#define _mem_limit			bc.bg.mem_limit
#define _prefetch_mem_base		bc.bg.prefetch_mem_base
#define _prefetch_mem_limit		bc.bg.prefetch_mem_limit
    unsigned long rsvd1;
    unsigned long rsvd2;
    unsigned long _baserom;
    unsigned long rsvd3;
    unsigned long rsvd4;
    union {
        unsigned long max_min_ipin_iline;
	struct {
	    unsigned char int_line;
	    unsigned char int_pin;
	    unsigned char min_gnt;
	    unsigned char max_lat;
	} mmii;
    } mmii;
#define _max_min_ipin_iline mmii.max_min_ipin_iline
#define _int_line mmii.mmii.int_line
#define _int_pin  mmii.mmii.int_pin
#define _min_gnt  mmii.mmii.min_gnt
#define _max_lat  mmii.mmii.max_lat
    /* end of official PCI config space header */
    unsigned long _pcibusidx;
    unsigned long _pcinumbus;
    unsigned long _pcibuses[16];
    unsigned short _configtype;   /* config type found                   */
    unsigned short _ioaddr;       /* config type 1 - private I/O addr    */
    unsigned long _cardnum;       /* config type 2 - private card number */
};

#define PCI_EN 0x80000000
#define MAX_PCI_DEVICES 64

extern int xf86Verbose;

extern struct pci_config_reg *pci_devp[];

extern ScrnInfoRec s3InfoRec;
extern short s3ChipId;
extern unsigned char s3LinApOpt;

extern ScrnInfoRec s3InfoRec;
extern Bool s3UsingPixMux;
extern Bool s3Bt485PixMux;
extern unsigned char s3SAM256;
extern int s3Weight, s3Bpp, s3BppDisplayWidth, s3ScissB;
extern int s3ValidTokens[];
extern int Num_VGA_IOPorts;
extern unsigned VGA_IOPorts[];
extern int vgaIOBase;
extern int vgaCRIndex;
extern int vgaCRReg;
extern int xf86bpp;
extern int s3DisplayWidth;
extern unsigned char s3Port31, s3Port51;
extern xrgb xf86weight;
extern pointer vgaBase;
extern pointer vgaNewVideoState;
extern Bool xf86VTSema;
extern int vgaInterlaceType;
extern short s3alu[];
extern unsigned char s3SwapBits[];

extern void (* vgaSaveScreenFunc)();
#define SS_START                0
#define SS_FINISH               1
#define GXcopy 3

#define IBMRGB_WRITE_ADDR           0x3C8   /* CR55 low bit == 0 */
#define IBMRGB_RAMDAC_DATA          0x3C9   /* CR55 low bit == 0 */
#define IBMRGB_PIXEL_MASK           0x3C6   /* CR55 low bit == 0 */
#define IBMRGB_READ_ADDR            0x3C7   /* CR55 low bit == 0 */
#define IBMRGB_INDEX_LOW            0x3C8   /* CR55 low bit == 1 */
#define IBMRGB_INDEX_HIGH           0x3C9   /* CR55 low bit == 1 */
#define IBMRGB_INDEX_DATA           0x3C6   /* CR55 low bit == 1 */
#define IBMRGB_INDEX_CONTROL        0x3C7   /* CR55 low bit == 1 */


#define V_PHSYNC    0x0001
#define V_NHSYNC    0x0002
#define V_PVSYNC    0x0004
#define V_NVSYNC    0x0008
#define V_INTERLACE 0x0010
#define V_DBLSCAN   0x0020
#define V_CSYNC     0x0040
#define V_PCSYNC    0x0080
#define V_NCSYNC    0x0100
#define V_PIXMUX    0x1000
#define V_DBLCLK    0x2000


#define XCONFIG_FONTPATH        1       /* Commandline/XF86Config or default  */
#define XCONFIG_RGBPATH         2       /* XF86Config or default */
#define XCONFIG_CHIPSET         3       /* XF86Config or probed */
#define XCONFIG_CLOCKS          4       /* XF86Config or probed */
#define XCONFIG_DISPLAYSIZE     5       /* XF86Config or default/calculated */
#define XCONFIG_VIDEORAM        6       /* XF86Config or probed */
#define XCONFIG_VIEWPORT        7       /* XF86Config or default */
#define XCONFIG_VIRTUAL         8       /* XF86Config or default/calculated */
#define XCONFIG_SPEEDUP         9       /* XF86Config or default/calculated */
#define XCONFIG_NOMEMACCESS     10      /* set if forced on */
#define XCONFIG_INSTANCE        11      /* XF86Config or default */
#define XCONFIG_RAMDAC          12      /* XF86Config or default */
#define XCONFIG_DACSPEED        13      /* XF86Config or default */
#define XCONFIG_BIOSBASE        14      /* XF86Config or default */
#define XCONFIG_MEMBASE         15      /* XF86Config or default */
#define XCONFIG_IOBASE          16      /* XF86Config or default */
#define XCONFIG_DACBASE         17      /* XF86Config or default */
#define XCONFIG_COPBASE         18      /* XF86Config or default */
#define XCONFIG_POSBASE         19      /* XF86Config or default */
#define XCONFIG_VGABASE         20      /* XF86Config or default */


#define XCONFIG_GIVEN           "(**)"
#define XCONFIG_PROBED          "(--)"


#define S3_MODEPRIV_SIZE        4
#define S3_INVERT_VCLK          1
#define S3_BLANK_DELAY          2
#define S3_EARLY_SC             3
#define CLK_REG_SAVE    -1
#define CLK_REG_RESTORE -2


#define StaticGray              0
#define GrayScale               1
#define StaticColor             2
#define PseudoColor             3
#define TrueColor               4
#define DirectColor             5

#define S3_PATCHLEVEL "0"

#define VGA_REGION 0
#define LINEAR_REGION 1
#define EXTENDED_REGION 2



#define CLOCK_TOLERANCE 2000
#define MAXSCREENS     3
#define DACDelay \
        { \
                unsigned char temp = inb(vgaIOBase + 0x0A); \
                temp = inb(vgaIOBase + 0x0A); \
        }



/* prototypes */
void vgaHWSaveScreen(
#if NeedFunctionPrototypes
    Bool start
#endif
);
Bool s3Probe(
#if NeedFunctionPrototypes
    void
#endif
);
void s3PrintIdent(
#if NeedFunctionPrototypes
    void
#endif
);

struct _OsTimerRec {
};

extern void (*s3ImageReadFunc)(
#if NeedFunctionPrototypes
    int, int, int, int, char *, int, int, int, unsigned long
#endif
);

unsigned char s3InIBMRGBIndReg(
    unsigned char 
);
void s3OutIBMRGBIndReg(
    unsigned char,
    unsigned char,
    unsigned char 
);


typedef struct _OsTimerRec *OsTimerPtr;

typedef CARD32 (*OsTimerCallback)(
#if NeedFunctionPrototypes
    OsTimerPtr /* timer */,
    CARD32 /* time */,
    pointer /* arg */
#endif
);

extern void TimerInit(
#if NeedFunctionPrototypes
    void
#endif
);

extern Bool TimerForce(
#if NeedFunctionPrototypes
    OsTimerPtr /* timer */
#endif
);


#define TimerAbsolute (1<<0)
#define TimerForceOld (1<<1)

extern OsTimerPtr TimerSet(
#if NeedFunctionPrototypes
    OsTimerPtr /* timer */,
    int /* flags */,
    CARD32 /* millis */,
    OsTimerCallback /* func */,
    pointer /* arg */
#endif
);

extern void TimerCheck(
#if NeedFunctionPrototypes
    void
#endif
);

extern void TimerFree(
#if NeedFunctionPrototypes
    OsTimerPtr /* pTimer */
#endif
);



#if 0
#define inb(x) ({ int p = x; int b = inb(p); fprintf(stderr,"%s:%d (%s) inb(0x%x=0x%x)\n",__FILE__,__LINE__,__FUNCTION__,p, b); b;})

#define outb(x,y) fprintf(stderr,"%s:%d (%s) outb(0x%x,0x%x)\n",__FILE__,__LINE__,__FUNCTION__,x,y),outb(x,y)


#define inw(x) ({ int p = x; int b = inw(p); fprintf(stderr,"%s:%d (%s) inw(0x%x=0x%x)\n",__FILE__,__LINE__,__FUNCTION__,p, b); b;})

#define outw(x,y) fprintf(stderr,"%s:%d (%s) outw(0x%x,0x%x)\n",__FILE__,__LINE__,__FUNCTION__,x,y),outw(x,y)

#define outl(x,y) fprintf(stderr,"%s:%d (%s) outl(0x%x,0x%x)\n",__FILE__,__LINE__,__FUNCTION__,x,y),outl(x,y)

#endif /* 0 */

extern void NoopDDA();

#endif
