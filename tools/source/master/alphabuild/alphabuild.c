/*
 *****************************************************************************
 **                                                                          *
 **  Copyright 1996, University of Cambridge Computer Laboratory             *
 **                                                                          *
 **  All Rights Reserved.                                                    *
 **                                                                          *
 *****************************************************************************
 **
 ** FACILITY:
 **
 **      Nemesis tools
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 **      Alpha boot image builder.
 ** 
 ** ENVIRONMENT: 
 **
 **      Alpha/OSF1
 **
 */

/*
 * Merge together files to generate an image bootable over bootp.
 * Assumes alphas load programs at 0x200000 physical, 0x20000000 virtual
 *
 * The program takes a boot loader program, some Palcode, and an image
 * produced by nembuild, and combines these to make the boot image.
 *
 */

#include <stdio.h>
#include <string.h>
#include <nlist.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <a.out.h>
#include "alphabuild.h"

#define VERBOSE(_str...) { \
      if(verbose) \
	  fprintf(stderr, _str); \
}

#define DEBUG(_str...) { \
      if(debug) \
	  fprintf(stderr, _str); \
}

addr_t nemesisPA= NEMESIS_PA;

/* global vars */
static bool_t 		debug, verbose;
static char             *prog_name;
static char 		*out_fname, *boot_fname, *pal_fname, *nemesis_fname;
neminfo                 *nemboot, *nempal, *nemesis; 

static void usage(void)
{
    fprintf(stderr, "Usage: %s [-d -v -o <outfname> -p <platform> ] <boot> <pal> <sys>\n",
	    prog_name); 
    fprintf(stderr, "\twhere <boot> is an (ECOFF) boot loader, \n");
    fprintf(stderr, "\t      <pal> is some Palcode (in vax a.out), \n");
    fprintf(stderr, "\t  and <sys> is the result of nembuild.\n");
    fprintf(stderr, "\tFlags: d= debug, v= verbose, o= output file\n");
    fprintf(stderr, "\tValid platforms are: eb164, axp3000 or eb64.\n");
    exit(-1);
}



/*
 * The first part of the image to be built is the boot loader
 * This is a standard object file compiled using gcc on OSF, 
 * and hence is an ECOFF file.
 * The below loads this into memory, and maintains its info.
 */
neminfo *load_ecoff(char *filename)
{
    int     fd;
    struct aouthdr exec;
    struct filehdr fhdr;
    neminfo *ni;
    char  *bptr, *cptr;
    long mem_req;
    int i;

    if ((fd = open (filename, 0)) == -1) 
    {
	fprintf(stderr, "Cannot open %s\n", filename);
	return((neminfo *)NULL);
    }

    /* Read the file header information */

    i = read (fd, &fhdr, sizeof (struct filehdr));
    if (i != sizeof (struct filehdr) || fhdr.f_magic != ALPHAMAGIC )
    {
	printf ("%s is not an executable file\n", filename);
	close (fd);
	return((neminfo *)NULL);
    }

    /*
     * read the aouthdr and check for the right type of
     * file.
     */
    i = read (fd, &exec, sizeof (struct aouthdr));
    if( exec.magic != OMAGIC )
    {
	printf("%s is not an OMAGIC file, relink using -N\n", 
	       filename);
	close (fd);
	return((neminfo *)NULL);
    }

    /*
     * Get memory to load image
     */
    mem_req = exec.tsize+exec.dsize+exec.bsize;
    DEBUG("Image requires %#010x bytes\n", mem_req);
    bptr = (char *)malloc((int)(mem_req+PAGSIZ));
    if( !bptr )
    {
	printf("can't allocate memory\n");
	return((neminfo *)NULL);
    }


    /*
     * Read the text
     */
    lseek (fd, (int)N_TXTOFF(fhdr, exec), 0);
    i = read (fd, bptr, (int)exec.tsize);
    DEBUG("Loading %s\n\ttext at %#010lx\tlen=%#010lx\n",
	    filename, exec.text_start, exec.tsize);

    /*
     * Read the data
     */
    cptr = bptr+exec.tsize;
    i = read (fd, cptr, (int)exec.dsize);
    DEBUG("\tdata at %#010lx\tlen=%#010lx\n", exec.data_start, exec.dsize);
    close (fd);

    /*
     * Zero out bss
     */
    DEBUG("\tbss  at %#010lx\tlen=%#010lx\n", exec.bss_start, exec.bsize);
    cptr += exec.dsize;
    for (i = 0; i < exec.bsize; i++)
	*cptr++ = 0;
  
    DEBUG("\tent  at %#010lx\n", exec.entry);

    /*
     * Fill in the program header info and pass back to caller
     */
    ni = (neminfo *)malloc(sizeof(neminfo));
    if( !ni )
    {
	fprintf(stderr, "can't allocate memory\n");
	return((neminfo *)NULL);
    }
    ni->tsize = exec.tsize;
    ni->dsize = exec.dsize;
    ni->bsize = exec.bsize;
    ni->text_start = exec.text_start;
    ni->data_start = exec.data_start;
    ni->bss_start = exec.bss_start;
    ni->entry = exec.entry;
    ni->image = (long)bptr;
    return ni;
}

/*
 * The palcode part of the image is produced using the bizarre
 * digital EB_SDK tools, including a modified gas and gld. 
 * These conspire to produce a lump of VAX a.out fmt stuff, 
 * which the below can read.
 */
neminfo *load_exec(char *filename)
{
    int     fd;
    struct  exec ex;		/* exec header		*/
    neminfo *ni;		/* program header	*/
    char    *bptr, *cptr;
    long    mem_req;
    int     rc;
    int     i;
  
    if ((fd = open (filename, 0)) == -1) 
    {
	fprintf(stderr, "Cannot open %s\n", filename);
	return((neminfo *)NULL);
    }
  
    /*
     * Read and check the aouthdr
     */
    rc = read (fd, &ex, sizeof (struct exec));
    if( ex.a_magic != OMAGIC )
    {
	fprintf(stderr, "%s is not an OMAGIC file, relink using -N\n", 
		filename);
	close (fd);
	return((neminfo *)NULL);
    }
  
    /*
     * Is there enough memory to load ?
     */
    mem_req = ex.a_text + ex.a_data + ex.a_bss;
    DEBUG("Image requires %#010x bytes\n", mem_req);
    bptr = (char *)malloc((int)(mem_req+PAGSIZ));
    if( !bptr )
    {
	fprintf(stderr, "Can't allocate memory\n");
	return((neminfo *)NULL);
    }
  
    if(debug) 
    {
	fprintf(stderr, "Loading %s\n\ttext at %#010lx\tlen=%#010lx\n",
		filename, ex.a_tstart, ex.a_text);
	fprintf(stderr, "\tdata at %#010lx\tlen=%#010lx\n",
		ex.a_dstart, ex.a_data);
	fprintf(stderr, "\tbss  at %#010lx\tlen=%#010lx\n", 
		ex.a_dstart + ex.a_data, ex.a_bss);
	fprintf(stderr, "\tent  at %#010lx\n", ex.a_entry);
    }
    
    /* Read the file */
    lseek (fd, (int)EXEC_N_TXTOFF(ex), 0);
    rc = read (fd, bptr, (int)ex.a_text + (int)ex.a_data);
  
  
    /* Zero BSS */
    cptr = bptr + ex.a_text + ex.a_data;
    for (i = 0; i < ex.a_bss; i++)
	*cptr++ = 0;
  
    /*
     * Fill in the program header info and pass back to caller
     */
    ni = (neminfo *)malloc(sizeof(struct _neminfo));
    if( !ni )
    {
	fprintf(stderr, "Can't allocate memory\n");
	return((neminfo *)NULL);
    }
    ni->tsize = ex.a_text;
    ni->dsize = ex.a_data;
    ni->bsize = ex.a_bss;
    ni->text_start = ex.a_tstart;
    ni->data_start = ex.a_dstart;
    ni->bss_start = (int)ex.a_dstart + (int)ex.a_data;
    ni->entry = ex.a_entry;
    ni->image = (long)bptr;
    return ni;
}


/*
 * The file produced by nembuild is essentially 'raw'; that is, 
 * it contains binary data which should be lumped into memory at
 * a particular point - for the present, this is hardwired. 
 * Thus the entire 'neminfo' struct is hardly needed here, but
 * we stay with it for simplicity's sake.
 */
neminfo *load_raw(char *filename)
{
    int     fd;
    neminfo *ni;
    char    *image_ptr;
    struct stat sbuf;
    long    image_size;
    int     i;

    if((stat(filename, &sbuf))) 
    {
	fprintf(stderr, "Cannot stat %s\n", filename);
	return((neminfo *)NULL);
    }
    
    DEBUG("load_raw: image in %s requires %ld bytes.\n",
	  filename, (uint64_t)sbuf.st_size);

    image_size= sbuf.st_size;

    if((fd = open (filename, 0)) == -1) 
    {
	fprintf(stderr, "Cannot open %s\n", filename);
	return((neminfo *)NULL);
    }

    /*
     * Get memory to load image
     */
    DEBUG("%s: Image requires %#010x bytes\n", filename, image_size);
    image_ptr = (char *)malloc((int)(image_size));
    if( !image_ptr )
    {
	fprintf(stderr, "Can't allocate memory\n");
	return((neminfo *)NULL);
    }

    /*
     * Read the image
     */
    i = read (fd, image_ptr, (int)image_size);
    DEBUG("Loading %s\n\timage at %#010lx\tlen=%#010lx\n",
	    filename, image_ptr, image_size);

    /*
     * Fill in the program header info and pass back to caller
     */
    ni = (neminfo *)malloc(sizeof(neminfo));
    if( !ni )
    {
	fprintf(stderr, "Can't allocate memory\n");
	return((neminfo *)NULL);
    }


    /* XXX how can I get the real information for below? */
    DEBUG("Note: guessing at neminfo for %s", filename);    
    ni->tsize = image_size;
    ni->dsize = 0; 
    ni->bsize = 0;;
    ni->text_start = nemesisPA;;
    ni->data_start = 0;
    ni->bss_start = 0;
    ni->entry = nemesisPA;;
    ni->image = (long)image_ptr;
    return ni;
}


int main(int argc, char **argv)
{
    FILE *out_fp;
    neminfo *ni;
    struct nlist nl[3];
    int c, i, rc; 
	char *platform;
	
    prog_name= strrchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0];
    debug= verbose= False;

    while((c=getopt(argc, argv, "o:dvp:")) != EOF) 
    {
	switch(c) {
	  case 'o':
	    out_fname= optarg;
	    break;
	  case 'd':
	    debug= True;
	    break;
	  case 'v':
	    verbose= True;
	    break;
	  case 'p':
		platform= optarg;
		if(!(strcmp(platform,"eb164"))) 
			nemesisPA= 0x210000;
		else if(!(strcmp(platform,"axp3000"))) 
			nemesisPA= 0x210000;
		else if(!(strcmp(platform,"eb64"))) 
			nemesisPA= 0x220000;
		else usage();
		break;
	  default: 
	    usage();
	}
    }

    if (optind+3 != argc)
	usage();

    boot_fname= argv[optind++];
    pal_fname= argv[optind++];
    nemesis_fname= argv[optind];

    /*
     * Read the images into memory
     */
    if((nemboot=load_ecoff(boot_fname))==(neminfo *)NULL) 
    {
	fprintf(stderr, "%s: error loading ECOFF from %s\n", prog_name, boot_fname);
	goto end;
    }
		
    if((nempal=load_exec(pal_fname))==(neminfo *)NULL) 
    {
	fprintf(stderr, "%s: error loading EXEC from %s\n", prog_name, pal_fname);
	goto free_boot;
    }


    if((nemesis=load_raw(nemesis_fname))==(neminfo *)NULL) 
    {
	fprintf(stderr, "%s: error loading RAW from %s\n", prog_name, nemesis_fname);
	goto free_pal;
    }

    DEBUG("%s: loaded all images.\n", prog_name);


    /*
     * Locate the neminfo structures in the bootstrap image
     */
    nl[0].n_name = strdup("nemesis_palcode");
    nl[1].n_name = strdup("nemesis_kernel");
    nl[2].n_name = (char *)NULL;
    nlist(boot_fname, nl);
    
    if(nl[0].n_type == 0)
    {
	printf("Can't find symbol nemesis_palcode in bootstrap loader\n");
	exit(1);
    }
    DEBUG("symbol \"nemesis_palcode\" located at %#010lx type=%x\n", 
	   nl[0].n_value, nl[0].n_type);

    if (nl[1].n_type == 0 )
    {
	fprintf(stderr, "Can't find symbol nemesis_kernel in bootstrap loader\n");
	exit(1);
    }
    DEBUG("symbol \"nemesis_kernel\"  located at %#010lx type=%x\n\n", 
	   nl[1].n_value, nl[1].n_type);
    

    /* compute lengths of each prog, rounding to appropriate page boundaries */
    nemboot->length = IMAGE_LEN(nemboot);
    nemboot->length = ALIGN_PAL(nemboot->length); /* Align for PALcode */

    nempal->length = IMAGE_LEN(nempal);
    nempal->length = ALIGN_PAGE(nempal->length);

    nemesis->length = IMAGE_LEN(nemesis);
    nemesis->length = ALIGN_PAGE(nemesis->length);

    /* Bogosity checks */
    if (nemboot->entry != BOOT_VA) 
    {
	fprintf(stderr, "%s: bootstrap entry pt bogus\n", argv[0]);
	exit(31);
    }
    
    if (nemesis->entry < (BOOT_PA+nemboot->length+nempal->length)) 
    {
	fprintf(stderr, "Kernel entry point too low %#010lx (should be >= %#010lx)\n",
		nemesis->entry, BOOT_PA+nemboot->length+nempal->length);
	exit(32);
    }

    if(verbose) 
    {
	printf ("          virt\t\t\tphys\t\t\tlen\n");
	printf ("%-10s%#010lx-%#010lx\t%#010lx-%#010lx\t%#010lx\n", 
		strrchr(boot_fname, '/') ? strrchr(boot_fname, '/') + 1 : boot_fname,
		BOOT_VA, BOOT_VA + IMAGE_LEN(nemboot),
		BOOT_PA, BOOT_PA + IMAGE_LEN(nemboot),
		nemboot->length);
    
	printf ("%-10s%#010lx-%#010lx\t%#010lx-%#010lx\t%#010lx\n", 
		strrchr(pal_fname, '/') ? strrchr(pal_fname, '/') + 1 : pal_fname,
		BOOT_VA+nemboot->length, BOOT_VA+nemboot->length+IMAGE_LEN(nempal),
		BOOT_PA+nemboot->length, BOOT_PA+nemboot->length+IMAGE_LEN(nempal),
		nempal->length);
    
	printf ("%-10s%#010lx-%#010lx\t%#010lx-%#010lx\t%#010lx\n", 
		strrchr(nemesis_fname, '/') ? strrchr(nemesis_fname, '/') + 1 : nemesis_fname,
		nemesis->text_start - BOOT_PA + BOOT_VA , 
		nemesis->text_start - BOOT_PA + BOOT_VA + IMAGE_LEN(nemesis),
		nemesis->text_start,
		nemesis->text_start + IMAGE_LEN(nemesis),
		nemesis->length);
    }
    
	
    /*
     * Pass information to the bootstrap loader by patching its data.
     */
    ni = (neminfo *) (nemboot->image+nl[0].n_value-nemboot->text_start);
    *ni = *nempal;
    DEBUG("Setting nempal in bootstrap as %x\n",
	  nemboot->text_start + nemboot->length - BOOT_VA + BOOT_PA);
    ni->image = nemboot->text_start + nemboot->length - BOOT_VA + BOOT_PA;
    ni = (neminfo *) (nemboot->image+nl[1].n_value-nemboot->text_start);
    *ni = *nemesis;
    DEBUG("Setting nemesis in bootstrap as %x\n", nemesis->text_start);
    ni->image = nemesis->text_start;


    /* write out the files */
    if ((out_fp=fopen(out_fname, "w")) == NULL)
    {
	fprintf(stderr, "%s: Couldn't create %s\n", prog_name, out_fname);
	exit(1);
    }

    /* Write bootstrap prog */
    if ((rc=fwrite((char *)nemboot->image, 1, IMAGE_LEN(nemboot), out_fp)) 
	!= IMAGE_LEN(nemboot))
    {
	fprintf(stderr, "%s: Couldn't write %s into %s\n", 
	       prog_name, boot_fname, out_fname);
	fprintf(stderr, "\t: requested %d wrote %d\n", IMAGE_LEN(nemboot), rc);
	exit(1);
    }
    DEBUG("Wrote 0x%x bytes into image from nemboot.\n", rc);
    
    /* Pad to page bndry */
    for (i = IMAGE_LEN(nemboot); i < nemboot->length; i++) putc (0, out_fp);
    DEBUG("Padded up to 0x%x.\n", i);
    
    /* Write palcode */
    if ((rc=fwrite((char *)nempal->image, 1, IMAGE_LEN(nempal), out_fp)) 
	!= IMAGE_LEN(nempal))
    {
	fprintf(stderr, "%s: Couldn't write %s into %s\n", 
	       prog_name, pal_fname, out_fname);
	fprintf(stderr, "\t: requested %d wrote %d\n", IMAGE_LEN(nempal), rc);
	exit(1);
    }
    DEBUG("Wrote 0x%x bytes into image from nempal.\n", rc);
    
    /* Pad to start of nemesis */
    for (i += IMAGE_LEN(nempal); i < (nemesisPA-BOOT_PA); i++) putc (0, out_fp);
    DEBUG("Padded up to 0x%x.\n", i);
    
    /* Write 'nemesis' image */
    if ((rc=fwrite((char *)nemesis->image, 1, IMAGE_LEN(nemesis), out_fp)) !=
	IMAGE_LEN(nemesis))
    {
	fprintf(stderr, "%s: Couldn't write %s into %s\n", 
	       prog_name, nemesis_fname, out_fname);
	fprintf(stderr, "\t: requested %d wrote %d\n", IMAGE_LEN(nemesis), rc);
	exit(1);
    }
    DEBUG("Wrote 0x%x bytes into image from nemesis.\n", rc);
    
    /* Pad to page bndry- should be 'zero', since nemesis already page aligned */
    for (i = IMAGE_LEN(nemesis); i < nemesis->length; i++) putc (0, out_fp);
    DEBUG("Padded up to 0x%x.\n", i);
    
    /* Pad with zeros for BOOTP bug */
    for (i = 0; i < 1024; i++) putc (0, out_fp);
    DEBUG("Padded 0x%x bytes for BOOTP bug (?).\n", i);
    
    fclose(out_fp);

    
    free(nemesis->image);
free_pal:
    free(nempal->image);
free_boot:
    free(nemboot->image);
end:
    exit(0);
}


