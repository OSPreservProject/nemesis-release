#ifndef _ASM_IO_H
#define _ASM_IO_H

#define __SLOW_DOWN_IO "\noutb %%al,$0x80"
#define __FULL_SLOW_DOWN_IO __SLOW_DOWN_IO

#define __OUT1(s,x) \
extern inline void out##s(unsigned x value, unsigned short port) {

#define __OUT2(s,s1,s2) \
__asm__ __volatile__ ("out" #s " %" s1 "0,%" s2 "1"

#define __OUT(s,s1,x) \
__OUT1(s,x) __OUT2(s,s1,"w") : : "a" (value), "Nd" (port)); } \
__OUT1(s##_p,x) __OUT2(s,s1,"w") __FULL_SLOW_DOWN_IO : : "a" (value), "Nd" (port));} \

__OUT(b,"b",char)

#define NULL 0

/*
 * These are set up by the setup-routine at boot-time:
 */

struct screen_info {
        unsigned char  orig_x;
        unsigned char  orig_y;
        unsigned char  unused1[2];
        unsigned short orig_video_page;
        unsigned char  orig_video_mode;
        unsigned char  orig_video_cols;
        unsigned short unused2;
        unsigned short orig_video_ega_bx;
        unsigned short unused3;
        unsigned char  orig_video_lines;
        unsigned char  orig_video_isVGA;
        unsigned short orig_video_points;
};

typedef unsigned int size_t;

#endif
