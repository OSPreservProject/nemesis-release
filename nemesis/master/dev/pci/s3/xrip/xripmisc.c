
#define SET_1024
#include <io.h>

#define INIT_OPTIONS
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <string.h>

#include <time.h>
#include <ecs.h>

#include "xrip.h"

#include <nemesis.h>

#ifdef FBDEBUG
#define DEBUG(x) fprintf(stderr,x)
#define TRC(x) x
#else
#define DEBUG(x)
#define TRC(x) 
#endif

#ifdef SET_1024
#define TARGETMODE "1024x768"
#endif
#ifdef SET_1280
#define TARGETMODE "1280x1024"
#endif

#define reorder(a,b)    b = \
        (a & 0x80) >> 7 | \
        (a & 0x40) >> 5 | \
        (a & 0x20) >> 3 | \
        (a & 0x10) >> 1 | \
        (a & 0x08) << 1 | \
        (a & 0x04) << 3 | \
        (a & 0x02) << 5 | \
        (a & 0x01) << 7;

int  xf86bpp = -1;
xrgb xf86weight = { 0, 0, 0 } ;
int xf86Verbose = 1;
int monitorResolution;
int InitDone = FALSE;
int ExtendedEnabled = FALSE;
static Bool ScreenEnabled[MAXSCREENS];
int xf86ProbeFailed = FALSE;
int defaultColorVisualClass = TrueColor; /* XXX */

/* double xf86rGamma=1.0, xf86gGamma=1.0, xf86bGamma=1.0; */
unsigned char xf86rGammaMap[256], xf86gGammaMap[256], xf86bGammaMap[256];
char *xf86VisualNames[] = {
    "StaticGray",
    "GrayScale",
    "StaticColor",
    "PseudoColor",
    "TrueColor",
    "DirectColor"
};
LUTENTRY currents3dac[256];
unsigned short s3BtLowBits[] = { 0x3C8, 0x3C9, 0x3C6, 0x3C7 };


Bool xf86VTSema;
static debugcache = 0;

 Bool s3ModeSwitched = FALSE;


#define FONT_AMOUNT 8192
#if defined(Lynx) || defined(linux) || defined(MINIX)
#define TEXT_AMOUNT 16384
#else
#define TEXT_AMOUNT 4096
#endif
static int currentExternClock = -1;
static int currentGraphicsClock = -1;
CARD32 ScreenSaverTime;
static char old_bank = -1;
extern char s3Mbanks;

extern void (*s3ImageWriteFunc)(
#if NeedFunctionPrototypes
    int, int, int, int, char *, int, int, int, short, unsigned long
#endif
);
extern void (*s3ImageFillFunc)(
#if NeedFunctionPrototypes
    int, int, int, int, char *, int, int, int, int, int, short, unsigned long
#endif
);
extern Bool s3LinearAperture;
extern int   s3BankSize;


int StrCaseCmp(char *s1, char *s2) {
  return strcasecmp(s1,s2);
}

int max(int x, int y) {
  return x>y ? x:y;
}


int FatalError(char *str, ...) {
  va_list args;
  va_start(args, str);
  vfprintf(stderr,str,args);
  va_end(args);
}


void xfree(void *me) {
  free(me);
}

void *xcalloc(size_t nmemb, size_t size) {
    return xalloc(nmemb*size);
}

void *xalloc(size_t size) {
  return malloc(size);
}

void *xrealloc(void *ptr, size_t size) {
  return realloc(ptr,size);
}



/*
 *
 * xf86GetNearestClock --
 *      Find closest clock to given frequency (in kHz).  This assumes the
 *      number of clocks is greater than zero.
 */
int
xf86GetNearestClock(Screen, Frequency)
        ScrnInfoPtr Screen;
        int Frequency;
{
  int NearestClock = 0;
  int MinimumGap = abs(Frequency - Screen->clock[0]);
  int i;
  for (i = 1;  i < Screen->clocks;  i++)
  {
    int Gap = abs(Frequency - Screen->clock[i]);
    if (Gap < MinimumGap)
    {
      MinimumGap = Gap;
      NearestClock = i;
    }
  }
  return NearestClock;
}


unsigned char rdinx(unsigned short port, unsigned char ind)
{
        if (port == 0x3C0)              /* reset attribute flip-flop */
                (void) inb(0x3DA);
        outb(port, ind);
        return(inb(port+1));
}

void xf86AddIOPorts(ScreenNum, NumPorts, Ports)
int ScreenNum;
int NumPorts;
unsigned *Ports;
{
        return;
}

void xf86ClearIOPortList(ScreenNum)
int ScreenNum;
{
        if (!InitDone)
        {
                int i;

                for (i = 0; i < MAXSCREENS; i++)
                        ScreenEnabled[i] = FALSE;
                InitDone = TRUE;
        }

        return;
}


#ifndef DEV_MEM
# define DEV_MEM "/dev/mem"
#endif
unsigned char s3InTiIndReg(unsigned char reg)
{
   unsigned char tmp, tmp1, ret;

   /* High 2 bits of reg in CR55 bits 0-1 (1 is cleared for the TI ramdac) */
   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | 0x01);  /* toggle to upper 4 direct registers */
   tmp1 = inb(TI_INDEX_REG);
   outb(TI_INDEX_REG, reg);

   /* Have to map the low two bits to the correct DAC register */
   ret = inb(TI_DATA_REG);

   /* Now clear 2 high-order bits so that other things work */
   outb(TI_INDEX_REG, tmp1);  /* just in case anyone relies on this */
   outb(vgaCRReg, tmp);

   return(ret);
}

void s3OutTiIndReg(unsigned char reg, unsigned char mask, unsigned char data)
{
   unsigned char tmp, tmp1, tmp2 = 0x00;

   /* High 2 bits of reg in CR55 bits 0-1 (1 is cleared for the TI ramdac) */
   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | 0x01);  /* toggle to upper 4 direct registers */
   tmp1 = inb(TI_INDEX_REG);
   outb(TI_INDEX_REG, reg);

   /* Have to map the low two bits to the correct DAC register */
   if (mask != 0x00)
      tmp2 = inb(TI_DATA_REG) & mask;
   outb(TI_DATA_REG, tmp2 | data);
   
   /* Now clear 2 high-order bits so that other things work */
   outb(TI_INDEX_REG, tmp1);  /* just in case anyone relies on this */
   outb(vgaCRReg, tmp);
}


void s3OutTi3026IndReg(unsigned char reg, unsigned char mask, unsigned char data)

{
   unsigned char tmp, tmp1, tmp2 = 0x00;

   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | 0x00);
   tmp1 = inb(0x3c8);
   outb(0x3c8, reg);
   outb(vgaCRReg, tmp | 0x02);

   if (mask != 0x00)
      tmp2 = inb(0x3c6) & mask;
   outb(0x3c6, tmp2 | data);
   
   outb(vgaCRReg, tmp | 0x00);
   outb(0x3c8, tmp1);  /* just in case anyone relies on this */
   outb(vgaCRReg, tmp);
}
unsigned char s3InTi3026IndReg(unsigned char reg)
{
   unsigned char tmp, tmp1, ret;

   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | 0x00);
   tmp1 = inb(0x3c8);
   outb(0x3c8, reg);
   outb(vgaCRReg, tmp | 0x02);

   ret = inb(0x3c6);

   outb(vgaCRReg, tmp | 0x00);
   outb(0x3c8, tmp1);  /* just in case anyone relies on this */
   outb(vgaCRReg, tmp);

   return(ret);
}


unsigned char s3InBtReg(unsigned short reg)
{
   unsigned char tmp, ret;

   /* High 2 bits of reg in CR55 bits 0-1 */
   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | ((reg & 0x0C) >> 2));

   /* Have to map the low two bits to the correct DAC register */
   ret = inb(s3BtLowBits[reg & 0x03]);

   /* Now clear 2 high-order bits so that other things work */
   outb(vgaCRReg, tmp);

   return(ret);
}


unsigned char s3InBtRegCom3()
{
   unsigned char ret;

   s3OutBtReg(BT_COMMAND_REG_0, 0x7F, 0x80);
   s3OutBtReg(BT_WRITE_ADDR, 0x00, 0x01);
   ret = s3InBtReg(BT_STATUS_REG);
   return(ret);
}


/*
 * The Command Register 3 register in the Bt485 must be accessed through
 * a very odd sequence, as the older RAMDACs had already defined 16
 * registers.  Apparently this overlays the Status register.
 */
void s3OutBtRegCom3(unsigned char mask, unsigned char data)
{
   s3OutBtReg(BT_COMMAND_REG_0, 0x7F, 0x80);
   s3OutBtReg(BT_WRITE_ADDR, 0x00, 0x01);
   s3OutBtReg(BT_STATUS_REG, mask, data);
}



void s3OutBtReg(unsigned short reg, unsigned char mask, unsigned char data)
{
   unsigned char tmp;
   unsigned char tmp1 = 0x00;

   /* High 2 bits of reg in CR55 bits 0-1 */
   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | ((reg & 0x0C) >> 2));

   /* Have to map the low two bits to the correct DAC register */
   if (mask != 0x00)
      tmp1 = inb(s3BtLowBits[reg & 0x03]) & mask;
   outb(s3BtLowBits[reg & 0x03], tmp1 | data);
   
   /* Now clear 2 high-order bits so that other things work */
   outb(vgaCRReg, tmp);
}

unsigned char s3InBtStatReg()
{
   unsigned char tmp, ret;

   tmp = s3InBtReg(BT_COMMAND_REG_0);
   if ((tmp & 0x80) == 0x80) {
      /* Behind Command Register 3 */
      tmp = s3InBtReg(BT_WRITE_ADDR);
      s3OutBtReg(BT_WRITE_ADDR, 0x00, 0x00);
      ret = s3InBtReg(BT_STATUS_REG);
      s3OutBtReg(BT_WRITE_ADDR, 0x00, tmp);
   } else {
      ret = s3InBtReg(BT_STATUS_REG);
   }
   return(ret);
}




void xf86EnableIOPorts(ScreenNum)
int ScreenNum;
{
        int i;

        ScreenEnabled[ScreenNum] = TRUE;

        if (ExtendedEnabled)
                return;

        if (iopl(3))
                ErrorF("%s: Failed to set IOPL for I/O\n",
                           "xf86EnableIOPorts");
        ExtendedEnabled = TRUE;

        return;
}

int xf86ReadBIOS(Base, Offset, Buf, Len)
unsigned long Base;
unsigned long Offset;
unsigned char *Buf;
int Len;
{
	memcpy(Buf, Base+Offset, Len);
        return(Len);
}

void wrinx(unsigned short port, unsigned char ind, unsigned char val)
{
        outb(port, ind);
        outb(port+1, val);
}

int
testinx2(unsigned short port, unsigned char ind, unsigned char mask)
{
        unsigned char old, new1, new2;

        old = rdinx(port, ind);
        wrinx(port, ind, old & ~mask);
        new1 = rdinx(port, ind) & mask;
        wrinx(port, ind, old | mask);
        new2 = rdinx(port, ind) & mask;
        wrinx(port, ind, old);
        return((new1 == 0) && (new2 == mask));
}


int
xf86StringToToken(table, string)
     SymTabPtr table;
     char *string;
{
  int i;

  for (i = 0; table[i].token >= 0 && strcasecmp(string, table[i].name); i++)
    ;
  return(table[i].token);
}

char *
xf86TokenToString(table, token)
     SymTabPtr table;
     int token;
{
  int i;

  for (i = 0; table[i].token >= 0 && table[i].token != token; i++)
    ;
  if (table[i].token < 0)
    return("unknown");
  else
    return(table[i].name);
}

void
xf86VerifyOptions(allowedOptions, driver)
     OFlagSet      *allowedOptions;
     ScrnInfoPtr    driver;
{
  int j;

  for (j=0; xf86_OptionTab[j].token >= 0; j++)
    if ((OFLG_ISSET(xf86_OptionTab[j].token, &driver->options)))
      if (OFLG_ISSET(xf86_OptionTab[j].token, allowedOptions))
      {
        if (xf86Verbose)
          ErrorF("%s %s: Option \"%s\"\n", XCONFIG_GIVEN,
                 driver->name, xf86_OptionTab[j].name);
      }
      else
        ErrorF("%s %s: Option flag \"%s\" is not defined for this driver\n",
               XCONFIG_GIVEN, driver->name, xf86_OptionTab[j].name);
}




void
xf86DeleteMode(infoptr, dispmp)
ScrnInfoPtr     infoptr;
DisplayModePtr  dispmp;
{
        if(infoptr->modes == dispmp)
                infoptr->modes = dispmp->next;

        if(dispmp->next == dispmp)
                ErrorF("No valid modes found.\nSuch is life\n");

        ErrorF("%s %s: Removing mode \"%s\" from list of valid modes.\n",
               XCONFIG_PROBED, infoptr->name, dispmp->name);
        dispmp->prev->next = dispmp->next;
        dispmp->next->prev = dispmp->prev;

        xfree(dispmp->name);
        xfree(dispmp);
}


Bool 
xf86LookupMode(target, driver)
     DisplayModePtr target;
     ScrnInfoPtr    driver;
{
  DisplayModePtr p;
  DisplayModePtr best_mode = NULL;
  int            i, Gap;
  int            Minimum_Gap = CLOCK_TOLERANCE + 1;
  Bool           found_mode = FALSE;
  Bool           clock_too_high = FALSE;
  static Bool	 first_time = TRUE;
#if 0
  /* XXX - mode string doesn't appear to survive. Hack it */
  target->name = TARGETMODE;
#endif /* 0 */
  TRC(fprintf(stderr,"Lookup up mode [%s]\n", target->name));
  if (first_time)
  {
    ErrorF("%s %s: Maximum allowed dot-clock: %d KHz\n", XCONFIG_PROBED,
	   driver->name, driver->maxClock);
    first_time = FALSE;
  }
#ifdef XRIPDEBUG
  fprintf(stderr,"Driver at %x\n",driver);
  fprintf(stderr,"Monitor at %x\n", driver->monitor);
  fprintf(stderr,"Modes at %x\n", driver->monitor->Modes);
#endif
#if 0
  for (p = driver->monitor->Modes; p!=NULL; p = p->next /* XXX- dr -only look at first one */)	/* scan list */
#else
    p = driver->monitor->Modes;
#endif /* 0 */
  {
    TRC(fprintf(stderr,"Mode %s at %x\n", p->name, p));
    if (!strcmp(p->name, target->name))		/* names equal ? */
    {
	ErrorF("Matched\n");
      if (OFLG_ISSET(CLOCK_OPTION_PROGRAMABLE, &(driver->clockOptions)))
      {
        if (driver->clocks == 0)
        {
#ifdef XRIPDEBUG
	    ErrorF("And here's some I made earlier\n");

#endif
	    /* this we know */
          driver->clock[0] = 25175;		/* 25.175Mhz */
          driver->clock[1] = 28322;		/* 28.322MHz */
          driver->clocks = 2;
        }

        if ((p->Clock) > (driver->maxClock))
          clock_too_high = TRUE;
        else
        {
	    /* We fill in the the programmable clocks as we go */
	    for (i=0; i < driver->clocks; i++) {
		if (driver->clock[i] == p->Clock) {
		    ErrorF("Using programmable clock %d\n",i);
		    break;
		}
	    }
          if (i >= MAXCLOCKS)
          {
            ErrorF("%s %s: Too many programmable clocks used (limit %d)!\n",
                   XCONFIG_PROBED, driver->name, MAXCLOCKS);
            return FALSE;
          }

          if (i == driver->clocks)
          {
#ifdef DEBUG
	      ErrorF("Programming new clock\n");
#endif
	      driver->clock[i] = p->Clock;
	      driver->clocks++;
          }
#ifdef DEBUG
	  ErrorF("p clock is %d\n", p->Clock);
	  ErrorF("Target clock is %d\n", target->Clock);
#endif
	  /* hum ho- s3 is borken. XXX */
#if 1
          target->Clock = i;
#endif /* 0 */
          best_mode = p;
        }
      }
      else
      {
	  /* Not programmable */
        i = xf86GetNearestClock(driver, p->Clock);
        Gap = abs(p->Clock - driver->clock[i]);
        if (Gap < Minimum_Gap)
        {
          if ((driver->clock[i]) > (driver->maxClock))
            clock_too_high = TRUE;
          else
          {

            target->Clock = i;

            best_mode = p;
            Minimum_Gap = Gap;
          }
        }
      }
      found_mode = TRUE;
    }
  }

  if (best_mode != NULL)
  {
    target->HDisplay       = best_mode->HDisplay;
    target->HSyncStart     = best_mode->HSyncStart;
    target->HSyncEnd       = best_mode->HSyncEnd;
    target->HTotal         = best_mode->HTotal;
    target->VDisplay       = best_mode->VDisplay;
    target->VSyncStart     = best_mode->VSyncStart;
    target->VSyncEnd       = best_mode->VSyncEnd;
    target->VTotal         = best_mode->VTotal;
    target->Flags          = best_mode->Flags; 
    target->CrtcHDisplay   = best_mode->CrtcHDisplay;
    target->CrtcHSyncStart = best_mode->CrtcHSyncStart;
    target->CrtcHSyncEnd   = best_mode->CrtcHSyncEnd;
    target->CrtcHTotal     = best_mode->CrtcHTotal;
    target->CrtcVDisplay   = best_mode->CrtcVDisplay;
    target->CrtcVSyncStart = best_mode->CrtcVSyncStart;
    target->CrtcVSyncEnd   = best_mode->CrtcVSyncEnd;
    target->CrtcVTotal     = best_mode->CrtcVTotal;
    target->CrtcHAdjusted  = best_mode->CrtcHAdjusted;
    target->CrtcVAdjusted  = best_mode->CrtcVAdjusted;
    if (target->Flags & V_DBLSCAN)
    {
      target->CrtcVDisplay *= 2;
      target->CrtcVSyncStart *= 2;
      target->CrtcVSyncEnd *= 2;
      target->CrtcVTotal *= 2;
      target->CrtcVAdjusted = TRUE;
    }

    /* I'm not sure if this is the best place for this in the
     * new XF86Config organization. - SRA
     */
    if (found_mode)
      if ((driver->ValidMode)(target) == FALSE)
        {
         ErrorF("%s %s: Unable to support mode \"%s\"\n",
              XCONFIG_GIVEN,driver->name, target->name );
         return(FALSE);
        }

    if (xf86Verbose)
    {
      ErrorF("%s %s: Mode \"%s\": mode clock = %d",
             XCONFIG_GIVEN, driver->name, target->name,
             driver->clock[target->Clock]);
      if (!OFLG_ISSET(CLOCK_OPTION_PROGRAMABLE, &(driver->clockOptions)))
        ErrorF(", clock used = %d", driver->clock[target->Clock]);       
      ErrorF("\n");
    }
  }
  else if (!found_mode) {
    ErrorF("%s %s: There is no mode definition named \"%s\"\n",
	   XCONFIG_PROBED, driver->name, target->name);
  }
  else if (clock_too_high) {
    ErrorF("%s %s: Clock for mode \"%s\" %s\n\tLimit is %d KHz\n",
           XCONFIG_PROBED, driver->name, target->name,
           "is too high for the configured hardware.",
           driver->maxClock);
  }
  else {
    ErrorF("%s %s: There is no defined dot-clock matching mode \"%s\"\n",
           XCONFIG_PROBED, driver->name, target->name);
  }
  return (best_mode != NULL);
}

Bool xf86DisableInterrupts()
{
        return(TRUE);
}

void xf86EnableInterrupts()
{
        return;
}

void xf86DisableIOPorts(ScreenNum)
int ScreenNum;
{
        int i;

        ScreenEnabled[ScreenNum] = FALSE;

        if (!ExtendedEnabled)
                return;

        for (i = 0; i < MAXSCREENS; i++)
                if (ScreenEnabled[i])
                        return;

        iopl(0);
        ExtendedEnabled = FALSE;

        return;
}


void TimerInit(void) {
}

Bool TimerForce(OsTimerPtr wibble) {
  return FALSE;
}

OsTimerPtr TimerSet( OsTimerPtr tiemr, int flags, CARD32 millis, OsTimerCallback func, pointer arg) {
  return NULL;
}

void TimerFree(OsTimerPtr pTimer) {
}

void SetTimeSinceLastInputEvent(void) {
}


void
xf86GetClocks(num, ClockFunc, ProtectRegs, SaveScreen, vertsyncreg, maskval, 
                knownclkindex, knownclkvalue, InfoRec)
int num;
Bool (*ClockFunc)();
void (*ProtectRegs)();
void (*SaveScreen)();
int vertsyncreg, maskval, knownclkindex, knownclkvalue;
ScrnInfoRec *InfoRec;
{
    register int status = vertsyncreg;
    unsigned long i, cnt, rcnt, sync;
    int saved_nice;

    /* First save registers that get written on */
    (*ClockFunc)(CLK_REG_SAVE);

#if defined(CSRG_BASED) || defined(MACH386)
    saved_nice = getpriority(PRIO_PROCESS, 0);
    setpriority(PRIO_PROCESS, 0, -20);
#endif
#if defined(SYSV) || defined(SVR4) || defined(linux)
#if 0
    saved_nice = nice(0);
    nice(-20 - saved_nice);
#endif /* 0 */
#endif


    for (i = 0; i < num; i++) 
    {
        (*ProtectRegs)(TRUE);
        if (!(*ClockFunc)(i))
        {
            InfoRec->clock[i] = -1;
            continue;
        }
        (*ProtectRegs)(FALSE);
        (*SaveScreen)(NULL, FALSE);
            
        usleep(50000);     /* let VCO stabilise */

        cnt  = 0;
        sync = 200000;

        if (!xf86DisableInterrupts())
        {
            (*ClockFunc)(CLK_REG_RESTORE);
            ErrorF("Failed to disable interrupts during clock probe.  If\n");
            ErrorF("your OS does not support disabling interrupts, then you\n");
            FatalError("must specify a Clocks line in the XF86Config file.\n");
        }
        while ((inb(status) & maskval) == 0x00) 
            if (sync-- == 0) goto finish;
        /* Something appears to be happening, so reset sync count */
        sync = 200000;
        while ((inb(status) & maskval) == maskval) 
            if (sync-- == 0) goto finish;
        /* Something appears to be happening, so reset sync count */
        sync = 200000;
        while ((inb(status) & maskval) == 0x00) 
            if (sync-- == 0) goto finish;
    
        for (rcnt = 0; rcnt < 5; rcnt++) 
        {
            while (!(inb(status) & maskval)) 
                cnt++;
            while ((inb(status) & maskval)) 
                cnt++;
        }
    
finish:
        xf86EnableInterrupts();

        InfoRec->clock[i] = cnt ? cnt : -1;
        (*SaveScreen)(NULL, TRUE);
    }
#if defined(CSRG_BASED) || defined(MACH386)
    setpriority(PRIO_PROCESS, 0, saved_nice);
#endif
#if defined(SYSV) || defined(SVR4) || defined(linux)
#if 0
    nice(20 + saved_nice);
#endif /* 0 */
#endif

    for (i = 0; i < num; i++) 
    {
        if (i != knownclkindex)
        {
            if (InfoRec->clock[i] == -1)
            {
                InfoRec->clock[i] = 0;
            }
            else 
            {
                InfoRec->clock[i] = 
                    ((knownclkvalue) * InfoRec->clock[knownclkindex]) / 
                    InfoRec->clock[i];
                /* Round to nearest 10KHz */
                InfoRec->clock[i] += 5;
                InfoRec->clock[i] /= 10;
                InfoRec->clock[i] *= 10;
            }
        }
    }

    InfoRec->clock[knownclkindex] = knownclkvalue;
    InfoRec->clocks = num; 

    /* Restore registers that were written on */
    (*ClockFunc)(CLK_REG_RESTORE);
}

pointer xf86MapVidMem(ScreenNum, Region, Base, Size)
int ScreenNum;
int Region;
pointer Base;
unsigned long Size;
{

 #ifdef EB164
     pointer result;
     TRC(eprintf("xf86MapVidMem: region=%x, base=%p, size=%x\n", 
 	   Region, Base, Size));
     result = (pointer)((uint64_t)Base | ALCOR_DENSE_MEM) ;
     TRC(eprintf("xf86MapVidMem: result=%p\n", result));
     return result;
 #else
     return Base;
 #endif
}

void xf86UnMapVidMem(ScreenNum, Region, Base, Size)
int ScreenNum;
int Region;
pointer Base;
unsigned long Size;
{
  /* noop */
}

Bool xf86LinearVidMem()
{
        return(TRUE);
}

/* ARGSUSED */
void xf86MapDisplay(ScreenNum, Region)
int ScreenNum;
int Region;
{
        return;
}

/* ARGSUSED */
void xf86UnMapDisplay(ScreenNum, Region)
int ScreenNum;
int Region;
{
        return;
}

void usleep(unsigned long usec) {
    PAUSE(usec*1000);
}

/* post-justification; it's only used in the gamma correction palette stuff, anyway; this merely makes the effect always gamma=1.0, which is vaguly sensible. */

/* XXX */

int iopl(int level) {
  /* dum de dum */
  return 0;
}


void miPointerPosition(int *x, int *y) {
    *x = 100;
    *y = 100;
}


#undef ErrorF
int ErrorF(char *str, ...) {
  va_list args;
  va_start(args, str);
#ifdef FBDEBUG
  vfprintf(stderr,str,args);
#endif
  va_end(args);
}

/* No-op Don't Do Anything : sometimes we need to be able to call a procedure
 * that doesn't do anything.  For example, on screen with only static
 * colormaps, if someone calls install colormap, it's easier to have a dummy
 * procedure to call than to check if there's a procedure
 */
void
NoopDDA(
#if NeedVarargsPrototypes
    void* f, ...
#endif
)
{
}
