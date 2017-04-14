#include <nemesis.h>
#include <stdio.h>
#include <salloc.h>
#include <time.h>
#include <exceptions.h>
#include <contexts.h>

#include <Rd.ih>
#include <WSSvr_st.h>
#include <environment.h>
#include <autoconf/backdrop.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif
 
/* XXX SMH: define PATIENCE_OF_JOB for bitmap backgrounds ;-) */

#ifdef CONFIG_BACKDROP
#define PATIENCE_OF_JOB
#endif


#ifdef PATIENCE_OF_JOB
/*static const char BACKDROP_FILENAME[] = "clouds.ppm"; */
#define BACKDROP_FILENAME "clouds.ppm"
#else
#define COLOUR16(_r,_g,_b) ((_b)|((_g)<<5)|((_r)<<10))
#define BG_COLOUR16        COLOUR16(0x14, 0x02, 0x1F)

#define COLOUR8(_r, _g, _b) ((_b)|((_g)<<2)|((_r)<<5))
#define BG_COLOUR8         COLOUR8(0x2, 0x02, 0x3)
#endif


void InitBackdrop(WSSvr_st *st)
{
    uint32_t size;

#ifdef PATIENCE_OF_JOB
    size = st->fb_st.width * st->fb_st.height * 2; /* 16 bbp */
    st->backdrop_stretch =  STR_NEW(size);
	    
    st->backdrop = STR_RANGE(st->backdrop_stretch, &size);
    memset(st->backdrop, 0, size);
    ExposeBackdrop(st, 0, 0, st->fb_st.width, st->fb_st.height);	    
    st->fill_bg  = False;

    Threads$Fork(Pvs(thds), BackdropLoadThread, st, 0);

#else

    TRC(fprintf(stderr, "Not loading backdrop: using plain fill.\n"));

    size = st->fb_st.width;

    if(st->fb_st.depth == 16) {
	size *= 2; /* 16 bbp */
    }
    st->backdrop_stretch = STR_NEW(size);
    st->backdrop = STR_RANGE(st->backdrop_stretch, &size);

    if(st->fb_st.depth == 16) {
	uint32_t i;
	uint16_t *bg = (uint16_t *)st->backdrop;

	size >>= 1;
	for(i = 0;  i < size; i++) 
	    bg[i] = BG_COLOUR16;
    } else {
	memset(st->backdrop, BG_COLOUR8, size);
    }


    st->fill_bg  = True;
    ExposeBackdrop(st, 0, 0, st->fb_st.width, st->fb_st.height);	    
    return;
#endif
}

#ifdef PATIENCE_OF_JOB
void BackdropLoadThread(WSSvr_st *st) {
    char buf[128], *buf2;
    Rd_clp rcrd;
    FILE *file;
    uint16_t *backdrop;
    
    TRY {
	char *filename = GET_STRING_ARG("backdrop_filename", 
					BACKDROP_FILENAME);

	file = fopen(filename, "r");
	TRC(fprintf(stderr,"Got file\n"));
	if(file == NULL)
	{
	    fprintf(stderr,"WSSvr: couldn't load backdrop\n");	     
	    return;
	}
		   
	rcrd = file->rd;
	TRC(fprintf(stderr,"Got reader\n"));
	    
	
	Rd$GetLine(rcrd,buf, 128);
	    
	if (!strcmp(buf,"P6")) {
	    fprintf(stderr,"Unknown image format\n");
	} else {
	    int sx, sy, depth, ox, oy, px, py, size;
	    do {
		Rd$GetLine(rcrd,buf, 128);
	    } while (buf[0] == '#');
	    
	    /* should now have x, y size */
	    sscanf(buf,"%d %d", &sx,&sy);
	    
	    TRC(fprintf(stderr,"Got image size %d,%d\n", sx,sy));

	    size = st->fb_st.width * st->fb_st.height * 2;

	    backdrop = st->backdrop;
    
	    do {
		Rd$GetLine(rcrd,buf, 128);
	    } while (buf[0] == '#');
	    
	    sscanf(buf,"%d", &depth);
	    
	    if (sx>st->fb_st.width) sx = st->fb_st.width;
	    if (sy>st->fb_st.height) sy = st->fb_st.height;
	    
	    ox = (st->fb_st.width - sx)/2;
	    oy = (st->fb_st.height - sy)/2;

	    buf2 = Heap$Malloc(Pvs(heap), sx*3);
	    
	    for (py = 0; py<sy; py++) {
		uint32_t *scrptr;
		uint32_t pack;
		char *bufptr;

		Rd$GetChars(rcrd, buf2, sx*3);
		
		bufptr = buf2;
		scrptr = (uint32_t *) (backdrop + (st->fb_st.width * py));

		for (px = 0; px<sx; px+=2) {
		    pack = 0;
		    
		    pack = ((*(bufptr++) & 248))<<7; /* 10 - 3 */
		    pack += ((*(bufptr++) & 248))<<2; /* 5 - 3 */
		    pack += ((*(bufptr++) & 248))>>3; /* 0 - 3 */
		    pack += ((*(bufptr++) & 248))<<(23); /* 16 + 10 - 3 */
		    pack += ((*(bufptr++) & 248))<<(18); /* 16 + 5 - 3 */
		    pack += ((*(bufptr++) & 248))<<(13); /* 16 + 0 - 3 */
		    
		    *scrptr = pack;
		    
		    scrptr++;
		}
	    }

	    Heap$Free(Pvs(heap), buf2);
	    
	}
	
	Rd$Close(rcrd);	    

	ExposeBackdrop(st, 0, 0, st->fb_st.width, st->fb_st.height);	    

    } /* do the work */
    CATCH_ALL {
	fprintf(stderr,"WSSvr: couldn't load backdrop\n");	     
    } ENDTRY;
}
#endif

void ExposeBackdrop(WSSvr_st *st, uint32_t x1, uint32_t y1, 
		    uint32_t x2, uint32_t y2) 
{

    FBBlit_Rec rec;
    
    x1 = x1 & ~7;
    x2 = (x2 + 7) & ~7;
    
    rec.x = x1;
    rec.y = y1;
    rec.sourcewidth = (x2-x1);
    rec.sourceheight = (y2 - y1);
    
    if(st->fill_bg) {
	rec.data   = st->backdrop;
	rec.stride = 0;
    } else {
	rec.data   = st->backdrop + x1 + (y1 * st->fb_st.width); 
	rec.stride = st->fb_st.width * 2;
    }

    MU_LOCK(&st->root_io_mu);
    if(st->blit_root) {
	
	while(FBBlit$Blit(st->blit_root, &rec))
	    ;
	
    } else if (st->io_root) {
	IO_Rec iorec;
	word_t value;
	uint32_t slots;

	iorec.base = &rec;
	iorec.len = sizeof(rec);

	IO$PutPkt(st->io_root, 1, &iorec, 0, FOREVER);
	IO$GetPkt(st->io_root, 1, &iorec, FOREVER, &slots, &value);
    }

    MU_RELEASE(&st->root_io_mu);
}
