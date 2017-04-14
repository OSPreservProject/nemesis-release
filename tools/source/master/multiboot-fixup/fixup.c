/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      multiboot-fixup
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Fixup fields in multiboot info block in a Nemesis image
** 
** ENVIRONMENT: 
**
**      Posix
** 
** ID : $Id: fixup.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
** 
**
*/

#include <stdio.h>

#define TRC(_x)

int main(int argc, char **argv)
{
    FILE *f;
    unsigned long length;
    unsigned long header;
    int i;
    unsigned long w;
    unsigned long checksum;

    if (argc != 2) {
	fprintf(stderr,"Usage: %s filename\n",argv[0]);
	exit(1);
    }

    f=fopen(argv[1], "r+");

    if (!f) {
	fprintf(stderr,"%s: can't open %s for reading and writing\n",
		argv[0],argv[1]);
	exit(1);
    }

    /* Work out how long it is */
    fseek(f, 0, SEEK_END);
    length=ftell(f);
    rewind(f);

    /* Find the multiboot header (within first 4k of file) */
    header=-1;
    for (i=0; i<1024; i++) {
	fread(&w, sizeof(w), 1, f);
	if (w==0x1badb002) {
	    header=ftell(f)-4;
	    break;
	}
    }

    if (header==-1) {
	fprintf(stderr,"%s: no multiboot header found\n",argv[0]);
    }

    TRC(printf("header offset %d\n",header));

    /* Write end addresses */
    fseek(f, header+20, SEEK_SET);
    w=0x100000+length;
    fwrite(&w, sizeof(w), 1, f);
    fwrite(&w, sizeof(w), 1, f);

    /* Read header, calculating checksum */
    fseek(f, header, SEEK_SET);
    checksum=0;
    for (i=0; i<2; i++) {
	fread(&w, sizeof(w), 1, f);
	TRC(printf("0x%x ",w));
	checksum-=w;
    }

    TRC(printf("0x%x\n",checksum));

    /* Write checksum */
    fseek(f, header+8, SEEK_SET);
    fwrite(&checksum, sizeof(checksum), 1, f);

    fclose(f);

    exit(0);
}
