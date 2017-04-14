#include "lksound.h"
#include <IDCMacros.h>
#include <Interrupt.ih>
#include <dma.h>
#include <math.h>
#include <Au.ih>
#include <assert.h>
caddr_t sound_mem_blocks[1024];
int sound_nblocks;
int sb_be_quiet;
int sm_games;
extern sb_devc *detected_devc;

#define TRC(_x)
#define CHK(_x)
#define PROGRESS(_x)
#if 1
/* render a sine wave followed by square wave at International A */
#define CYCLES_PER_BUF 163.0
#else
#define CYCLES_PER_BUF 60.0
#endif
void render_wave(int32_t *base, int count) {
    int i;
    double theta;
    double x;
    double scalefactor;
    int y;

    scalefactor = 8192.0 / count;
    TRC(printk("renderwave base %x\n", base));
    for (i=0; i<count; i++) {
#define SINEWAVE
#ifdef SINEWAVE
	theta = ((i*CYCLES_PER_BUF)/((double) count))*M_PI*2.0;
	x = sin(theta);
	y = ((int) (x * 8192))+16384;
	*base++ = (y) + (y<<16);
#else
	*base++ = 0xbbbbbbbb;
#endif	    
    }
    TRC(printk("renderwave end %x\n", base));
}


int request_irq(unsigned int irq,
                       void (*handler)(int, void *, struct pt_regs *),
                       unsigned long flags, 
                       const char *device,
		void *dev_id) {
    int result;

    TRC(printk("Device %s wants irq %d\n",  device, irq));
    result =Interrupt$Allocate(IDC_OPEN("sys>InterruptOffer", Interrupt_clp),
		       irq,
		       handler,
		       dev_id);
    TRC(printk("Allocator result was %d\n", result));
    if (result != Interrupt_AllocCode_Ok) {
	printk("Interrupt allocation failed!\n");
	return -EINVAL;
    }
    ENTER_KERNEL_CRITICAL_SECTION();
    ntsc_unmask_irq(irq);
    LEAVE_KERNEL_CRITICAL_SECTION();
    
    return 0;
}

void free_irq(unsigned int irq, void *dev_id) {
    TRC(printk("Free IRQ %d\n", irq));
}


static struct address_info config;
#define AUBUFSIZE_LG2 16
#define AUBUFSIZE (1<<AUBUFSIZE_LG2)
#define AUBUFSIZE_SAMPLES ((1<<AUBUFSIZE_LG2)>>2)

#define TEST_DATA_SIZE AUBUFSIZE
#define USE_LOW_DMA 1
#define SPINLIMIT (1<<30)
#define BACKGRANULARITY 64 /* distance to stave off the clear region */
#define CLEARZONE 1024 /* distance for client to use */
#define FORWARDGRANULARITY 16 /* distance to stave off the renderer */
#define AUBUFLOC(_x, now)  (_x->master_location + (((now - _x->master_timestamp) / (_x->master_sample_period))))
#define TARGET_TIME_THROUGH_LOOP (SECONDS(1)/100)

int main(int argc, char *argv[]) {
    int r;
    int dev;
    Stretch_clp dmastr;
    int32_t *sounddata;
    uint64_t ptr;
    int dmasize;
    int seen_interrupts;
    extern uint64_t interrupt_count; 
    extern Time_clp time_cl;
    extern Au_Rec aurec;
    Au_Rec *aubuf;
    uint64_t spincounter;
    uint64_t clearloc;
    int advertised_rate;
#ifdef LOWLOG_CLEARING
    uint64_t *clearlogptr;
    uint32_t clearlogcount;
    uint64_t *clearlogbase = (uint64_t *)(256 * 1024);
    void clearlog_start(uint32_t base, uint32_t sb, int num) {
	*(clearlogptr++) = NOW();
	*(clearlogptr++) = ((uint64_t)base) + (((uint64_t) sb) << 32);
	*(clearlogptr++) = num/4;
    }
    void clearlog_end(void) {
	*(clearlogptr++) = NOW();
	clearlogcount++;
	if (clearlogcount >= 8192) {
	    clearlogcount =0;
	    clearlogptr = clearlogbase;
	}	
    }
#endif    
    aubuf = &aurec;
    time_cl = Pvs(time);

    advertised_rate = 44100;

    if (argc>=2) advertised_rate = atoi(argv[1]);
#ifndef USE_LOW_DMA
    dmastr = STR_NEW(AUBUFSIZE);
    sounddata = (int32_t *) Stretch$Range(dmastr, &(dmasize));
    TRC(printf("Starting\n"));
    TRC(printk("aubufsb: Stretch (%x for %x) allocated\n", sounddata, dmasize));
    ptr = (addr_t) sounddata;
    ptr += AUBUFSIZE - 1;
    ptr = ((ptr)>>AUBUFSIZE_LG2)<<AUBUFSIZE_LG2;
    sounddata = (int32_t*)ptr;
    TRC(printk("aubufsb: dma stretch is base %x (%dk) length %x\n", sounddata, ((int) sounddata)/1024, dmastr));

    StretchAllocator$SetGlobal(Pvs(salloc), dmastr, SET_ELEM(Stretch_Right_Read) | SET_ELEM(Stretch_Right_Write));
#else
    
    sounddata = (int32_t*) ((uint32_t) ((64) * 1024));
    memset(sounddata, 0,(512-64)*1024);
    dmasize = AUBUFSIZE;
    TRC(printf("Using DMA region at %x (%dk) length %d\n", sounddata, ((uint32_t) sounddata)/1024, AUBUFSIZE));
    PAUSE(SECONDS(1));
#endif
    memset(sounddata, 0, TEST_DATA_SIZE);
#if 0
    render_wave(sounddata, AUBUFSIZE_SAMPLES);
#endif
    PROGRESS(printf("lksound starting\n"));
    config.io_base = 0x220;
    config.irq = 5;
    config.dma = 1;
    config.dma2 = 5;
    config.card_subtype = 0;
    

    PROGRESS(printk("Calling sb_probe\n"));
    r = probe_sb(&config);
    PROGRESS(printk("Probe result %d\n", r));
    if (!r) return 0;
    PROGRESS(printk("Calling attach sb card\n"));
    attach_sb_card(&config);
    dev = config.slots[0];
    printk("LKsound happy; dev is %d; doing setup\n", dev);
    PROGRESS(printk("Set speed\n"));
    audio_devs[dev]->d->set_speed(dev, advertised_rate);
    PROGRESS(printk("Set bits\n"));
    audio_devs[dev]->d->set_bits(dev, AFMT_S16_LE);
    PROGRESS(printk("Set channels\n"));
    audio_devs[dev]->d->set_channels(dev, 2);
    PROGRESS(printk("lk: open\n"));
    audio_devs[dev]->d->open(dev, OPEN_WRITE);
    PROGRESS(printk("lk: prepare for output %x\n", audio_devs[dev]->d->prepare_for_output));
    
    audio_devs[dev]->d->prepare_for_output(dev, 0,0);

    aubuf->base = sounddata;
    aubuf->length = AUBUFSIZE_SAMPLES;
    aubuf->playback = 1;
    aubuf->master_location = 0;
    aubuf->master_sample_period = 0; /* set by the stub after a while */
    aubuf->master_granularity = FORWARDGRANULARITY;
    aubuf->safe_zone = AUBUFSIZE_SAMPLES - BACKGRANULARITY - CLEARZONE - FORWARDGRANULARITY;
    aubuf->format.target_rate = advertised_rate;
    aubuf->format.channels = 2;
    aubuf->format.little_endianness = 1;
    aubuf->format.bits_per_channel = 16;
    aubuf->format.channel_stride = 16;
    aubuf->format.sample_stride = 32;
    spincounter = 0;
    seen_interrupts = 0;

    /* start the sound card going */
    PROGRESS(printk("lk: set output\n"));
    audio_devs[dev]->d->output_block(dev, sounddata, TEST_DATA_SIZE, 1);
    PROGRESS(printk("lk: trigger\n"));
    audio_devs[dev]->d->trigger(dev, 0xffff);

    /* wait till we see some activity from the interrupt stub */
    while (!aubuf->master_sample_period  && spincounter < SPINLIMIT ) {
	PAUSE(SECONDS(20)/1000);
	if (seen_interrupts != interrupt_count) {
	    printk("(%d)",STARTUP_DELAY_COUNT - interrupt_count);
	    seen_interrupts = interrupt_count;
	}
    }

    if (spincounter == SPINLIMIT || !interrupt_count) {
	printk("Sound card has stopped interrupts; dying\n");
	return 1;
    }
    TRC(printk("Aubuf running; initial spin was for %qx times around the loop\n", spincounter));

    CX_ADD("dev>audio0", aubuf, Au_Rec);
#ifdef LOWLOG_INTERRUPTS
    CX_ADD("dev>audio0_int_ts", (void*) (192*1024), addr_t);
#endif

    LOWLOG_CLEARING_ONLY(
    CX_ADD("dev>audio0_clear", (void*) (256*1024), addr_t);
    CX_ADD("dev>audio0_clear_size", 256*1024, uint64_t);
    clearlogptr =(uint64_t *) (256*1024);
    clearlogcount = 0);
    clearloc = 0;
    
    /* go in to clearing loop
       The idea is that clearloc is the absolute position in the ongoing buffer
       to which the buffer has been recorded, and sbloc is the absolute 
       position in the buffer to which the sound card has reached
       */
#define CLEAR(_x) _x

    while (1) {
	uint64_t sbloc, realsbloc;
	uint32_t clearlocb;
	uint32_t sblocb, realsblocb;
	Time_ns now;
	int64_t timeleft;

	now = NOW();
	realsbloc = AUBUFLOC(aubuf, now);
	sbloc = (realsbloc-BACKGRANULARITY);

	sblocb = sbloc % AUBUFSIZE_SAMPLES;
	realsblocb = realsbloc % AUBUFSIZE_SAMPLES;
	/* if clearloc is too far behind, make it exactly half a  buffer behind to avoid overflows */
	if (sbloc - clearloc > AUBUFSIZE_SAMPLES) {

	    /* XXX; we are behind! Let us print a message out so that we are even worse off! */
	    printk("lksound clearer got behind; clearloc %qx sbloc %qx\n", clearloc, sbloc);

	    clearloc = sbloc - (AUBUFSIZE_SAMPLES/2);
	    clearlocb = sblocb;
	} else {
	    clearlocb = clearloc % AUBUFSIZE_SAMPLES;
	}
	if (sblocb >= clearlocb) {
	    /* clear forwards */
	    CHK(assert(clearlocb + (sblocb-clearlocb) < AUBUFSIZE_SAMPLES));

	    LOWLOG_CLEARING_ONLY(clearlog_start(clearlocb, realsblocb, (sblocb - clearlocb)*4));
	    CLEAR(memset(sounddata + clearlocb, 0, (sblocb - clearlocb)*4));
	    LOWLOG_CLEARING_ONLY(clearlog_end());
	} else {
	    /* clear backwards */
	    /* so first to the end */
	    CHK(assert(clearlocb + (AUBUFSIZE_SAMPLES-clearlocb) <= AUBUFSIZE_SAMPLES));
	    LOWLOG_CLEARING_ONLY(clearlog_start(clearlocb, realsblocb, (AUBUFSIZE_SAMPLES - clearlocb)*4));
	    CLEAR(memset(sounddata + clearlocb, 0, (AUBUFSIZE_SAMPLES - clearlocb)*4));
	    LOWLOG_CLEARING_ONLY(clearlog_end());

	    CHK(assert(sblocb < AUBUFSIZE_SAMPLES));
	    /* and then from the front to the location */
	    LOWLOG_CLEARING_ONLY(clearlog_start(0, realsblocb, sblocb*4));
	    CLEAR(memset(sounddata, 0, (sblocb)*4));
	    LOWLOG_CLEARING_ONLY(clearlog_end());

	}
	clearloc = sbloc;
	timeleft = TARGET_TIME_THROUGH_LOOP - (NOW() - now);
	if (timeleft > 0) PAUSE(timeleft);
    }

    return 0;
}


int sound_alloc_dma(int channel, const char *name) {
    printk("Allocate DMA channel %d to %s\n", channel, name);
    return 0;
}

void conf_printf(char *name, struct address_info *hw_config)
{
        if (!trace_init)
                return;

        printk("<%s> at 0x%03x", name, hw_config->io_base);

        if (hw_config->irq)
                printk(" irq %d", (hw_config->irq > 0) ? hw_config->irq : -hw_config->irq);

        if (hw_config->dma != -1 || hw_config->dma2 != -1)
        {
                printk(" dma %d", hw_config->dma);
                if (hw_config->dma2 != -1)
                        printk(",%d", hw_config->dma2);
        }
        printk("\n");
}


void unload_sb(struct address_info *hw_config) {
    sb_dsp_unload(hw_config);
}

int check_region(int base, int x) {
    printk("Checking IO regions (%x,%x) (unimplemented)\n", base, x);
    return 0;
}

void request_region(int base, int x, const char *name) {
    printk("Requesting region %x for %d for %s\n", base,x,name);
}

int probe_sb(struct address_info *hw_config) {
    return sb_dsp_detect(hw_config);
}


int sound_open_dma(int chn, char *deviceID) {
    printk("Device %s using DMA channel %d\n", deviceID, chn);
    return 0;
}

void sound_free_dma(int chn) {
    printk("DMA Channel %d released\n", chn);
}

void sound_close_dma(int chn) {
    printk("DMA Channel %d closed\n", chn);
}

void DMAbuf_init(int dev, int dma1, int dma2) {
    printk("DMAbuf init: device %d dma1 %d dma2 %d\n", dev, dma1, dma2);
}

void DMAbuf_deinit(int dev) {
    printk("DMAbuf deinit: device %d\n", dev);
}

void DMAbuf_inputintr(int dev) {
#if 0
    printk("DMAbuf inputintr device %d\n", dev);
#endif
}

void DMAbuf_outputintr(int dev, int dummy) {
#if 0
    printk("DMAbuf outputintr device %d dummy %d\n", dev, dummy);
#endif
}

int DMAbuf_start_dma (int dev,  addr_t physaddr, 
                       int count, int dma_mode)
{
    sb_devc        *devc = audio_devs[dev]->devc;
    addr_t paddr = virt_to_bus(physaddr);
    int channel;
    DDB(printk("Physical DMA address is %x (%dk)\n", paddr, ((uint32_t) paddr) / 1024));
    channel = devc->dma16; /* XXX hack */
    disable_dma (channel);
    clear_dma_ff (channel);
    set_dma_mode (channel, dma_mode);
    set_dma_addr (channel, paddr);
    set_dma_count (channel, count);
    enable_dma (channel);
    return 0;
}


void attach_sb_card(struct address_info *hw_config)
{
#if defined(CONFIG_AUDIO) || defined(CONFIG_MIDI)
	sb_dsp_init(hw_config);
#endif
}
void audio_init_devices(void)
{
    /* it was blank in the kernel as well */
}
/*
 * Table for configurable mixer volume handling
 */
static mixer_vol_table mixer_vols[MAX_MIXER_DEV];
static int num_mixer_volumes = 0;


int *load_mixer_volumes(char *name, int *levels, int present)
{
        int             i, n;

        for (i = 0; i < num_mixer_volumes; i++)
                if (strcmp(name, mixer_vols[i].name) == 0)
                  {
                          if (present)
                                  mixer_vols[i].num = i;
                          return mixer_vols[i].levels;
                  }
        if (num_mixer_volumes >= MAX_MIXER_DEV)
          {
                  printk("Sound: Too many mixers (%s)\n", name);
                  return levels;
          }
        n = num_mixer_volumes++;

        strcpy(mixer_vols[n].name, name);

        if (present)
                mixer_vols[n].num = n;
        else
                mixer_vols[n].num = -1;

        for (i = 0; i < 32; i++)
                mixer_vols[n].levels[i] = levels[i];
        return mixer_vols[n].levels;
}



