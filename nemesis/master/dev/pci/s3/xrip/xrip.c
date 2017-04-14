
#define SET_1024
#include <io.h>
#include <xrip.h>
#include <pci.h>

#include <ecs.h>
#include <time.h>
#include <frames.h>
#include <contexts.h>
#include <exceptions.h>

#include <PCIBios.ih>
#include <Frames.ih>

#ifdef FBDEBUG
#define printf eprintf
#define DB(_x) _x
#define TRC(_x) _x
#else
#define DB(_x) 
#define TRC(_x)
#endif

#ifdef SET_1024
#define TARGETMODE "1024x768"
#endif
#ifdef SET_1280
#define TARGETMODE "1280x1024"
#endif
#ifdef SET_800
#define TARGETMODE "800x600"
#endif
#ifdef SET_640
#define TARGETMODE "640x480"
#endif



INLINE static void EnableLinear() {
    WaitIdle();
    if (S3_801_928_SERIES (s3ChipId)) {
	int   i;
	
	/* begin 801 sequence for going in to linear mode */
	/* x64: CR40 changed a lot for 864/964; wait and see if still works */
	outb(vgaCRIndex, 0x40);
	i = (s3Port40 & 0xf6) | 0x0a; /* enable fast write buffer and disable
				       * 8514/a mode */

	outb(vgaCRReg, (unsigned char) i);
	DISABLE_MMIO; 
	outb (vgaCRIndex, 0x58);
	outb (vgaCRReg, s3LinApOpt | s3SAM256);   /* go on to linear mode */

	if (!S3_x64_SERIES(s3ChipId)) {
	    outb (vgaCRIndex, 0x54);
	    outb (vgaCRReg, (s3Port54 + 07));
	}

	/* end  801 sequence to go into linear mode, now lock the registers */
	outb(vgaCRIndex, 0x39);
	outb(vgaCRReg, 0x50); 
    } else 
	outb(vgaCRIndex, 0x35);
}






uint32_t *fb_init() 
{
    MonPtr monp;
    DisplayModePtr mode;
    ScrnInfoPtr screen;
    DispPtr disp;
    int *ptr;
    char *cptr;
    int j;
    int i, count;
    unsigned int screen_pcidense; 

    s3Bpp = 1;
    xf86Verbose = TRUE;
    monp = xalloc(sizeof(MonRec));
    monp->Last = 0;
    monp->n_hsync = 0;
    monp->n_vrefresh = 0;
    monp->id = "VM17";
    monp->vendor = "Iiyama";
    monp->model = "Vision Master 17";
    monp->bandwidth = 135.0;
    monp->hsync[0].lo = 27.0;
    monp->hsync[0].hi = 86.0;
    monp->n_hsync = 1;
    monp->vrefresh[0].lo = 50.0;
    monp->vrefresh[0].hi = 120.0;
    monp->n_vrefresh = 1;

    mode = xalloc(sizeof(DisplayModeRec));
    mode->next= mode; /* circular */
    mode->prev = mode;
    mode->Flags = 0;
    mode->HDisplay = mode->VDisplay = 0;
    mode->CrtcHAdjusted = mode->CrtcVAdjusted = FALSE;

#ifdef SET_1280
    mode->name = "1280x1024";
    mode->Clock = 135000;
    mode->HDisplay = 1280;
    mode->HSyncStart = 1312;
    mode->HSyncEnd = 1456;
    mode->HTotal = 1712;
    mode->VDisplay = 1024;
    mode->VSyncStart = 1027;
    mode->VSyncEnd = 1030;
    mode->VTotal = 1064;
#endif
#ifdef SET_1024
    mode->name = "1024x768";
/* define 0 for low refresh rates suitable for scan coverter */
#if 1
    mode->Clock = 85000;
    mode->HDisplay = 1024;
    mode->HSyncStart = 1032;
    mode->HSyncEnd = 1152;
    mode->HTotal = 1360;
    mode->VDisplay = 768;
    mode->VSyncStart = 784;
    mode->VSyncEnd = 787;
    mode->VTotal = 823;
#else
    mode->Clock = 62000;
    mode->HDisplay = 1024;
    mode->HSyncStart = 1088;
    mode->HSyncEnd = 1200;
    mode->HTotal = 1328;
    mode->VDisplay = 768;
    mode->VSyncStart = 783;
    mode->VSyncEnd = 789;
    mode->VTotal = 818;
#endif
#endif
#ifdef SET_800
    mode->name = "800x600";
    mode->Clock = 50000;
    mode->HDisplay = 800;
    mode->HSyncStart = 856;
    mode->HSyncEnd = 976;
    mode->HTotal = 1040;
    mode->VDisplay = 600;
    mode->VSyncStart = 637;
    mode->VSyncEnd = 643;
    mode->VTotal = 666;
#endif
#ifdef SET_640
    mode->name = "640x480";
    mode->Clock = 25175;
    mode->HDisplay = 640;
    mode->HSyncStart =664;
    mode->HSyncEnd = 760;
    mode->HTotal = 800;
    mode->VDisplay =480;
    mode->VSyncStart = 491;
    mode->VSyncEnd = 493;
    mode->VTotal = 525;
#endif
    mode->CrtcHDisplay = mode->HDisplay;
    mode->CrtcHSyncStart = mode->HSyncStart;
    mode->CrtcHSyncEnd = mode->HSyncEnd;
    mode->CrtcHTotal = mode->HTotal;
    mode->CrtcVDisplay = mode->VDisplay;
    mode->CrtcVSyncStart = mode->VSyncStart;
    mode->CrtcVSyncEnd = mode->VSyncEnd;
    mode->CrtcVTotal = mode->VTotal;

    monp->Modes = mode;
#ifdef XRIPDEBUG
    ErrorF("mode params are\n Horizontal %d %d %d %d\n Vertical %d %d %d %d\n",
	   mode->HDisplay, mode->HSyncStart, mode->HSyncEnd, mode->HTotal,
	   mode->VDisplay, mode->VSyncStart, mode->VSyncEnd, mode->VTotal);
#endif
    screen = &s3InfoRec;
    screen->configured = TRUE;
    screen->defaultVisual = PseudoColor;
    screen->width = 240;
    screen->height = 180;
    screen->bankedMono = FALSE;
    screen->textclock = -1;
    screen->blackColour.red = 0;
    screen->blackColour.green = 0;
    screen->blackColour.blue = 0;
    screen->whiteColour.red = 0x3F;
    screen->whiteColour.green = 0x3F;
    screen->whiteColour.blue = 0x3F;
    screen->suspendTime = 15*60*1000;
    screen->offTime = 30*60*1000;
    screen->clocks = 0;
#ifdef SET_1280
    screen->depth = 8;
    screen->weight.red = screen->weight.green = screen->weight.blue = 0;
    screen->frameX0 = 0;
    screen->frameY0 = 0;
    screen->frameX1 = 1279;
    screen->frameY1 = 1023;
    screen->virtualX = 1280;
    screen->virtualY = 1024;
#endif
#ifdef SET_1024
    screen->depth = 15;
    screen->weight.red = screen->weight.green = screen->weight.blue = 8;
    screen->frameX0 = 0;
    screen->frameY0 = 0;
    screen->frameX1 = 1023;
    screen->frameY1 = 767;
    screen->virtualX = 1024;
    screen->virtualY = 768;
#endif
#ifdef SET_800
    screen->depth = 32;
    screen->weight.red = screen->weight.green = screen->weight.blue = 0;
    screen->frameX0 = 0;
    screen->frameY0 = 0;
    screen->frameX1 = 799;
    screen->frameY1 = 599;
    screen->virtualX = 800;
    screen->virtualY = 600;
#endif
#ifdef SET_640
    screen->depth = 32;
    screen->weight.red = screen->weight.green = screen->weight.blue = 0;
    screen->frameX0 = 0;
    screen->frameY0 = 0;
    screen->frameX1 = 799;
    screen->frameY1 = 599;
    screen->virtualX = 800;
    screen->virtualY = 600;

#endif

    screen->modes = mode;
    screen->whiteColour.red = screen->whiteColour.green = 
	screen->whiteColour.blue = 0x3f;
    screen->blackColour.red = screen->blackColour.green = 
	screen->blackColour.blue = 0;
    screen->defaultVisual = -1;
    OFLG_ZERO(&(screen->options));
    OFLG_ZERO(&(screen->xconfigFlag));

    screen->chipset = "mmio_928";
    screen->ramdac = NULL;
    screen->dacSpeed = 0;
    OFLG_ZERO(&(screen->options));
    OFLG_ZERO(&(screen->xconfigFlag));
    screen->videoRam = 0;
    screen->speedup = 0; /* XXX */
    OFLG_ZERO(&(screen->clockOptions));
    screen->clocks = 0;
    screen->clockprog = NULL;
    /* 
    ** GJA -- We initialize the following fields to known values.
    ** If later on we find they contain different values,
    ** they might be interesting to print.
    */
    screen->IObase = 0;
    screen->DACbase = 0;
    screen->COPbase = 0;
    screen->POSbase = 0;
    screen->instance = 0;
    screen->BIOSbase = 0;
    screen->MemBase = 0;
    {
	PCIBios_Bus pci_bus; 
	PCIBios_DevFn pci_devfn;
	PCIBios_clp pci = NULL;
	PCIBios_Error error;

	screen_pcidense = 0;

	TRY {
	    pci = NAME_FIND("sys>PCIBios", PCIBios_clp);
	    ENTER_KERNEL_CRITICAL_SECTION();
	    error = PCIBios$FindDevice(pci, PCI_VENDOR_ID_S3, 
				       PCI_DEVICE_ID_S3_968, 
				       0, &pci_bus, &pci_devfn);

	    LEAVE_KERNEL_CRITICAL_SECTION();

	    if(error) {
		fprintf(stderr, "FB/S3: Can't find an S3 968.\n");
		screen->MemBase = NULL;
		return NULL;
	    } else {
		ENTER_KERNEL_CRITICAL_SECTION();
		PCIBios$ReadConfigDWord(pci, pci_bus, pci_devfn,
					PCI_BASE_ADDRESS_0,
					&screen_pcidense);
		LEAVE_KERNEL_CRITICAL_SECTION();

		TRC(fprintf(stderr, "PCI bios probe phys address: %p\n",
			    screen_pcidense));
	    }
	  
	} 
	CATCH_Context$NotFound(name) {
	    eprintf("pci_probe: PCIBios not present; defaulting mem base.\n");
	    PAUSE(TENTHS(5));
	    screen_pcidense = SCREEN_MEM_BASE;
	}
	ENDTRY;
    }

    /* SMH: when we get here, we have the physical memory address
       of the screen memory in "screen_pcidense"; we want to allocate 
       a stretch over it. */
    {
	Mem_PMemDesc pmem;
	Stretch_clp fbstr; 
	Stretch_Size size, sz; 
	Frames_clp frames; 
	addr_t pbase, chk; 
	uint32_t fwidth; 
	Mem_Attrs attr; 
	bool_t free; 
	word_t screen_phys; 

	screen_phys = pci_dense_to_phys(screen_pcidense);
	size = (screen->width * screen->height * screen->bitsPerPixel) >> 3;
	eprintf("s3: screen_pcidense = %p, screen_phys = %p, size = %lx\n", 
		screen_pcidense, screen_phys, size);
	frames = NAME_FIND("sys>Frames", Frames_clp);
	pbase  = Frames$AllocRange(frames, size, FRAME_WIDTH, screen_phys, 0); 
	fwidth = Frames$Query(frames, pbase, &attr);
	eprintf("Got memory (pbase is %p): width is 0x%x, attr 0x%x\n", 
		pbase, fwidth, attr);
	pmem.start_addr  = pbase; 
	pmem.nframes     = BYTES_TO_LFRAMES(size, fwidth); 
	pmem.frame_width = fwidth; 
	pmem.attr        = 0;

	fbstr = StretchAllocator$NewAt(Pvs(salloc), size, 0, 
				       screen_phys, 0, &pmem);
	screen->MemBase = screen_phys;
    }

    TRC(fprintf(stderr,"Card memory base is %p\n", screen->MemBase));

    screen->s3Madjust = 0;
    screen->s3Nadjust = 0;
    screen->s3MClk = 0;
    screen->s3RefClk = 14318; /* XXX- we should be able to autoprobe */
    screen->s3BlankDelay = -1;


    i = 0;
    while (xf86_OptionTab[i].token != -1) 
    {
	if (strcasecmp("Diamond", xf86_OptionTab[i].name) == 0)
	{
	    OFLG_SET(xf86_OptionTab[i].token, &(screen->options));
	    break;
	}
	i++;
    }
    screen->monitor = monp;
    DB(eprintf("Calling Probe\n"));
    screen->Probe();
    DB(eprintf("Calling s3Initialize\n"));
    s3Initialize(0,NULL,0,NULL);
    DB(eprintf("That's all, folks!\n"));

    DB(eprintf("Enabling linear addressing..."));
    EnableLinear();
    DB(eprintf("Got linear addressing\n"));


    ptr = screen->MemBase;
    for(i=0; i<0xffff; i++)
	;
    memset(ptr, 0x55, 1024*768*2);
    DB(eprintf("Done blanky thing\n"));

    TRC(fprintf(stderr,"Announcement: FB is at %x\n", screen->MemBase));

    DB(eprintf("Light is changing to shadow\n"));
    return(screen->MemBase);
}

