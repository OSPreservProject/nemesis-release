/* placeholder */

#define CONFIG_SBDSP 1
#define CONFIG_AUDIO 1
#define CONFIG_SB 1

#include <nemesis.h>
#include <dma.h>
#define SOUND_MIXER_NRDEVICES   25

#define EINVAL 1
#define EBUSY 2
#define ENOMEM 3
#define ENXIO 4
#define EPERM 5
#define EIO 6
#       define AFMT_QUERY               0x00000000      /* Return current fmt */
#       define AFMT_MU_LAW              0x00000001
#       define AFMT_A_LAW               0x00000002
#       define AFMT_IMA_ADPCM           0x00000004
#       define AFMT_U8                  0x00000008
#       define AFMT_S16_LE              0x00000010      /* Little endian signed 16*/
#       define AFMT_S16_BE              0x00000020      /* Big endian signed 16 */
#       define AFMT_S8                  0x00000040
#       define AFMT_U16_LE              0x00000080      /* Little endian U16 */
#       define AFMT_U16_BE              0x00000100      /* Big endian U16 */
#       define AFMT_MPEG                0x00000200      /* MPEG (2) audio */


#define SOUND_MIXER_NRDEVICES   25
#define SOUND_MIXER_VOLUME      0
#define SOUND_MIXER_BASS        1
#define SOUND_MIXER_TREBLE      2
#define SOUND_MIXER_SYNTH       3
#define SOUND_MIXER_PCM         4
#define SOUND_MIXER_SPEAKER     5
#define SOUND_MIXER_LINE        6
#define SOUND_MIXER_MIC         7
#define SOUND_MIXER_CD          8
#define SOUND_MIXER_IMIX        9       /*  Recording monitor  */
#define SOUND_MIXER_ALTPCM      10
#define SOUND_MIXER_RECLEV      11      /* Recording level */
#define SOUND_MIXER_IGAIN       12      /* Input gain */
#define SOUND_MIXER_OGAIN       13      /* Output gain */
#define SOUND_MIXER_LINE1       14      /* Input source 1  (aux1) */
#define SOUND_MIXER_LINE2       15      /* Input source 2  (aux2) */
#define SOUND_MIXER_LINE3       16      /* Input source 3  (line) */
#define SOUND_MIXER_DIGITAL1    17      /* Digital (input) 1 */
#define SOUND_MIXER_DIGITAL2    18      /* Digital (input) 2 */
#define SOUND_MIXER_DIGITAL3    19      /* Digital (input) 3 */
#define SOUND_MIXER_PHONEIN     20      /* Phone input */
#define SOUND_MIXER_PHONEOUT    21      /* Phone output */
#define SOUND_MIXER_VIDEO       22      /* Video/TV (audio) in */
#define SOUND_MIXER_RADIO       23      /* Radio in */
#define SOUND_MIXER_MONITOR     24      /* Monitor (usually mic) volume */
#define SOUND_ONOFF_MIN         28
#define SOUND_ONOFF_MAX         30

/* Note!        Number 31 cannot be used since the sign bit is reserved */
#define SOUND_MIXER_NONE        31


#define SNDCARD_ADLIB           1
#define SNDCARD_SB              2
#define SNDCARD_PAS             3
#define SNDCARD_GUS             4
#define SNDCARD_MPU401          5
#define SNDCARD_SB16            6
#define SNDCARD_SB16MIDI        7
#define SNDCARD_UART6850        8
#define SNDCARD_GUS16           9
#define SNDCARD_MSS             10
#define SNDCARD_PSS             11
#define SNDCARD_SSCAPE          12
#define SNDCARD_PSS_MPU         13
#define SNDCARD_PSS_MSS         14
#define SNDCARD_SSCAPE_MSS      15
#define SNDCARD_TRXPRO          16
#define SNDCARD_TRXPRO_SB       17
#define SNDCARD_TRXPRO_MPU      18
#define SNDCARD_MAD16           19
#define SNDCARD_MAD16_MPU       20
#define SNDCARD_CS4232          21
#define SNDCARD_CS4232_MPU      22
#define SNDCARD_MAUI            23
#define SNDCARD_PSEUDO_MSS      24
#define SNDCARD_GUSPNP          25
#define SNDCARD_UART401         26

/*      Device mask bits        */

#define SOUND_MASK_VOLUME       (1 << SOUND_MIXER_VOLUME)
#define SOUND_MASK_BASS         (1 << SOUND_MIXER_BASS)
#define SOUND_MASK_TREBLE       (1 << SOUND_MIXER_TREBLE)
#define SOUND_MASK_SYNTH        (1 << SOUND_MIXER_SYNTH)
#define SOUND_MASK_PCM          (1 << SOUND_MIXER_PCM)
#define SOUND_MASK_SPEAKER      (1 << SOUND_MIXER_SPEAKER)
#define SOUND_MASK_LINE         (1 << SOUND_MIXER_LINE)
#define SOUND_MASK_MIC          (1 << SOUND_MIXER_MIC)
#define SOUND_MASK_CD           (1 << SOUND_MIXER_CD)
#define SOUND_MASK_IMIX         (1 << SOUND_MIXER_IMIX)
#define SOUND_MASK_ALTPCM       (1 << SOUND_MIXER_ALTPCM)
#define SOUND_MASK_RECLEV       (1 << SOUND_MIXER_RECLEV)
#define SOUND_MASK_IGAIN        (1 << SOUND_MIXER_IGAIN)
#define SOUND_MASK_OGAIN        (1 << SOUND_MIXER_OGAIN)
#define SOUND_MASK_LINE1        (1 << SOUND_MIXER_LINE1)
#define SOUND_MASK_LINE2        (1 << SOUND_MIXER_LINE2)
#define SOUND_MASK_LINE3        (1 << SOUND_MIXER_LINE3)
#define SOUND_MASK_DIGITAL1     (1 << SOUND_MIXER_DIGITAL1)
#define SOUND_MASK_DIGITAL2     (1 << SOUND_MIXER_DIGITAL2)
#define SOUND_MASK_DIGITAL3     (1 << SOUND_MIXER_DIGITAL3)
#define SOUND_MASK_PHONEIN      (1 << SOUND_MIXER_PHONEIN)
#define SOUND_MASK_PHONEOUT     (1 << SOUND_MIXER_PHONEOUT)
#define SOUND_MASK_RADIO        (1 << SOUND_MIXER_RADIO)
#define SOUND_MASK_VIDEO        (1 << SOUND_MIXER_VIDEO)
#define SOUND_MASK_MONITOR      (1 << SOUND_MIXER_MONITOR)

/* Obsolete macros */
#define SOUND_MASK_MUTE         (1 << SOUND_MIXER_MUTE)
#define SOUND_MASK_ENHANCE      (1 << SOUND_MIXER_ENHANCE)
#define SOUND_MASK_LOUD         (1 << SOUND_MIXER_LOUD)

#define SOUND_MIXER_RECSRC      0xff    /* Arg contains a bit for each recordi
ng source */
#define SOUND_MIXER_DEVMASK     0xfe    /* Arg contains a bit for each support
ed device */
#define SOUND_MIXER_RECMASK     0xfd    /* Arg contains a bit for each support
ed recording source */
#define SOUND_MIXER_CAPS        0xfc
#       define SOUND_CAP_EXCL_INPUT     0x00000001      /* Only one recording 
source at a time */

#define SOUND_MIXER_PRIVATE1            _SIOWR('M', 111, int)
#define SOUND_MIXER_PRIVATE2            _SIOWR('M', 112, int)
#define SOUND_MIXER_PRIVATE3            _SIOWR('M', 113, int)
#define SOUND_MIXER_PRIVATE4            _SIOWR('M', 114, int)
#define SOUND_MIXER_PRIVATE5            _SIOWR('M', 115, int)


#define SIOCPARM_MASK   IOCPARM_MASK
#define SIOC_VOID       IOC_VOID
#define SIOC_OUT        IOC_OUT
#define SIOC_IN         IOC_IN
#define SIOC_INOUT      IOC_INOUT
#define _SIOC_SIZE      _IOC_SIZE
#define _SIOC_DIR       _IOC_DIR
#define _SIOC_NONE      _IOC_NONE
#define _SIOC_READ      _IOC_READ
#define _SIOC_WRITE     _IOC_WRITE
#define _SIO            _IO
#define _SIOR           _IOR
#define _SIOW           _IOW
#define _SIOWR          _IOWR
#       define PCM_ENABLE_INPUT         0x00000001
#       define PCM_ENABLE_OUTPUT        0x00000002



typedef addr_t caddr_t;
struct pt_regs;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_REALTIME_FACTOR      4

#define KERN_WARNING "WARNING: "
#define KERN_ERR "ERROR: "
#define KERN_INFO "INFO: "
#define KERN_DEBUG "DEBUG: "
#define printk  printf
#define GFP_KERNEL 0

#include <ecs.h>
#include <time.h>
#include <io.h>
#define CONFIG_SB_BASE 0x220
#define CONFIG_SB_IRQ 5

#define jiffies         (NOW())
#define udelay(us)      PAUSE(MICROSECS(us))
#define HZ              SECONDS(1)
#define INTERRUPTS_OFF Threads$EnterCS(Pvs(thds),False)
#define INTERRUPTS_ON Threads$LeaveCS(Pvs(thds))

#define save_flags(x) x++; INTERRUPTS_OFF
#define cli()
#define restore_flags(x) x--; INTERRUPTS_ON
#define vmalloc(x) malloc(x)
#define kmalloc(x,y) malloc(x)
#define kfree(x) free(x)
extern int max_sound_cards;
#include "sound_config.h"
#include "dev_table.h"

extern int request_irq(unsigned int irq,
                       void (*handler)(int, void *, struct pt_regs *),
                       unsigned long flags, 
                       const char *device,
                       void *dev_id);
extern void free_irq(unsigned int irq, void *dev_id);

extern caddr_t sound_mem_blocks[1024];
extern int sound_nblocks;

typedef struct mixer_vol_table {
  int num;      /* Index to volume table */
  char name[32];
  int levels[32];
} mixer_vol_table;
int sound_open_dma(int chn, char *deviceID);
void sound_close_dma(int chn);

int sound_alloc_dma(int chn, const char *deviceID);

void request_region(int base, int x, const char *name);
int check_region(int base, int x);
void sb_dsp_unload(struct address_info *hw_config);
void sb_dsp_init(struct address_info *hw_config);
int sb_dsp_detect(struct address_info *hw_config);
#include "sb.h"
void sb_audio_init(sb_devc *devc, char *name);

#define NUMLASTTIMES 32
#define LASTTIMES_MASK 31
extern uint64_t interrupt_count;
extern uint64_t lasttimes[];

#define STARTUP_DELAY_COUNT 32


#define LOWLOG_INTERRUPTS 1
#define LOWLOG_CLEARING 1

#ifdef LOWLOG_CLEARING
#define LOWLOG_CLEARING_ONLY(_x) _x
#else
#define LOWLOG_CLEARING_ONLY(_x)
#endif



