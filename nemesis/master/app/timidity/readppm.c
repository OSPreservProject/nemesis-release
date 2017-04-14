#include <nemesis.h>
#include <stdio.h>
#include <Rd.ih>
#include <CRendDisplay.ih>
#include <string.h>

CRend_clp renderppmtocrend(CRendDisplay_clp display , int xpos, int ypos, FILE *f, int *sizex, int *sizey) {
    Rd_clp    rd = f->rd;
    int       sx,sy, depth;
    char      buf[128];
    int16_t  *ptr;
    int       stride;
    int       px,py;
    uint8_t  *databuffer;
    CRend_clp crend;

    Rd$GetLine(rd, buf, 128);

    if (!strcmp(buf, "P6")) {
	fprintf(stderr,"Unknown image file format; first line is [%s]\n", buf);
	return NULL;
    }
    do Rd$GetLine(rd, buf, 128); while (buf[0] == '#');
    sscanf(buf, "%d %d", &sx, &sy);

    *sizex = sx;
    *sizey = sy;

    crend = CRendDisplay$CreateWindow(display,xpos,ypos,sx,sy);
    CRend$Map(crend);
 

    do Rd$GetLine(rd, buf, 128); while (buf[0] == '#');
    sscanf(buf, "%d", &depth);
	    
    ptr = CRend$GetPixelData(crend, &stride);
    databuffer = Heap$Malloc(Pvs(heap), sx*3);
    
    for (py = 0; py<sy; py++) {
	uint32_t *scrptr;
	uint32_t pack;
	char *bufptr;
	
	Rd$GetChars(rd, databuffer, sx*3);
		
	bufptr = databuffer;
	scrptr = (uint32_t *) (ptr + (stride * py));

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
    Heap$Free(Pvs(heap),databuffer);
    
    CRend$ExposeRectangle(crend,0,0,sx,sy);
    CRend$Flush(crend);
    return crend;

}
