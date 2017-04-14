/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      GUNK Nemesis tools
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Sandpiper boot image builder.  
** 
** ENVIRONMENT: 
**
**      Alpha/OSF1
** 
** ID : $Id: nmerge.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
** 
**
*/

/*
 * Merge images for loading by BOOTP on AXP3000
 *
 * AXP3000 machines load programs at 0x200000 physical, 0x20000000 virtual
 *
 * The purpose of this program is to take a bootstrap loader image
 * and combine it with placode and a kernel in such a way that
 * the loader can switch to the new palcode and start execution of the
 * kernel.
 *
 */

#include <stdio.h>
#include <string.h>
#include <nlist.h>

#include "nmerge.h"

#define BOOT_PA         (0x200000)
#define BOOT_VA         (0x20000000)

#define ALIGN_PAGE(l)  (((l) + (PAGSIZ-1)) & ~(PAGSIZ-1))
#define ALIGN_PAL(l)   (((l) + ((PAGSIZ<<1)-1)) & ~((PAGSIZ<<1)-1))
#define IMAGE_LEN(ni)  (ni->tsize + ni->dsize + ni->bsize)
#define FILE_LEN(ni)   (ni->tsize + ni->dsize)

extern struct neminfo *load_ecoff(char *filename); /* ECOFF loader */
extern struct neminfo *load_exec(char *filename);  /* GAS exec loader */

struct neminfo nemesis_palcode;
struct neminfo nemesis_kernel;

int main(int argc, char **argv)
{
  struct neminfo *nemboot, *nempal, *kernel; /* headers for each image	*/
  struct neminfo *ni;		/* header in the boot image	*/
  struct nlist nl[2];
  unsigned long nemboot_len;
  unsigned long nempal_len;
  unsigned long kernel_len;
  FILE *fp;			/* output file descriptor	*/
  int rc;
  int i;
    
  if( argc != 5 ){
    printf("usage: %s bootstrap palcode kernel output\n", argv[0]);
    exit();
  }
    
  /*
   * Read the images into memory
   */
  nemboot = load_ecoff (argv[1]);
  nempal  = load_exec (argv[2]);
  kernel  = load_ecoff (argv[3]);
    
  /*
   * If any of the pointers are NULL we can't go on
   */
  if( !(nemboot && nempal && kernel) )
    exit(1);

  /*
   * Locate the neminfo structures in the bootstrap image
   */
  printf("\n");
  nl[0].n_name = "nemesis_palcode";
  nl[1].n_name = "nemesis_kernel";
  nlist(argv[1], nl);
  if (nl[0].n_type == 0 ){
    printf("Can't find symbol nemesis_palcode in bootstrap loader\n");
    exit(1);
  }
  printf("symbol \"nemesis_palcode\" located at %#010lx type=%x\n", 
	 nl[0].n_value, nl[0].n_type);
  if (nl[1].n_type == 0 ){
    printf("Can't find symbol nemesis_kernel in bootstrap loader\n");
    exit(1);
  }
  printf("symbol \"nemesis_kernel\"  located at %#010lx type=%x\n", 
	 nl[1].n_value, nl[1].n_type);
  printf("\n");
    
  /* compute lengths of each prog, rounding to appropriate page boundaries */

  nemboot_len = IMAGE_LEN(nemboot);
  nemboot_len = ALIGN_PAL(nemboot_len); /* Align for PALcode */

  nempal_len = IMAGE_LEN(nempal);
  nempal_len = ALIGN_PAGE(nempal_len);

  kernel_len = IMAGE_LEN(kernel);
    
  /* Bogosity checks */
  if (nemboot->entry != BOOT_VA) {
    printf ("bootstrap entry pt bogus\n");
    exit(1);
  }
  if (kernel->entry < (BOOT_PA + nemboot_len + nempal_len)) {
    printf ("Kernel entry point too low %#010lx (should be >= %#010lx)\n",
	    kernel->entry, BOOT_PA + nemboot_len + nempal_len);
    exit(1);
  }
    
  printf ("                    virt\t\t\tphys\t\t\tlen\n");
  printf ("%-20s%#010lx-%#010lx\t%#010lx-%#010lx\t%#010lx\n", 
	  strrchr(argv[1], '/') ? strrchr(argv[1], '/') + 1 : argv[1], 
	  BOOT_VA, BOOT_VA + IMAGE_LEN(nemboot),
	  BOOT_PA, BOOT_PA + IMAGE_LEN(nemboot),
	  nemboot_len);
  printf ("%-20s%#010lx-%#010lx\t%#010lx-%#010lx\t%#010lx\n", 
	  strrchr(argv[2], '/') ? strrchr(argv[2], '/') + 1 : argv[2], 
	  BOOT_VA + nemboot_len, BOOT_VA + nemboot_len + IMAGE_LEN(nempal),
	  BOOT_PA + nemboot_len, BOOT_PA + nemboot_len + IMAGE_LEN(nempal),
	  nempal_len);
  printf ("%-20s%#010lx-%#010lx\t%#010lx-%#010lx\t%#010lx\n", 
	  strrchr(argv[3], '/') ? strrchr(argv[3], '/') + 1 : argv[3], 
	  kernel->text_start - BOOT_PA + BOOT_VA , 
	  kernel->text_start - BOOT_PA + BOOT_VA + IMAGE_LEN(kernel), 
	  kernel->text_start,
	  kernel->text_start + IMAGE_LEN(kernel),
	  FILE_LEN(kernel));
    
  /*
   * Pass information to the bootstrap loader by patching its data.
   */
  ni = (struct neminfo *) (nemboot->image+nl[0].n_value-nemboot->text_start);
  *ni = *nempal;
  ni->image = nemboot->text_start + nemboot_len - BOOT_VA + BOOT_PA;
  ni = (struct neminfo *) (nemboot->image+nl[1].n_value-nemboot->text_start);
  *ni = *kernel;
  ni->image = kernel->text_start; /* Kernel is mapped 121 */
    
  /* write out the files */
  if ((fp=fopen(argv[4], "w")) == NULL){
    printf("Couldn't create %s\n", argv[4]);
    exit(1);
  }
  /* Write bootstrap prog */
  if ((rc=fwrite((char *)nemboot->image, 1, IMAGE_LEN(nemboot), fp)) 
      != IMAGE_LEN(nemboot)){
    printf("Couldn't write %s into %s\n", argv[1], argv[4]);
    printf("Requested %d wrote %d\n", IMAGE_LEN(nemboot), rc);
    exit(1);
  }

  /* Pad to page bndry */
  for (i = IMAGE_LEN(nemboot); i < nemboot_len; i++) putc (0, fp);

  /* Write palcode */
  if ((rc=fwrite((char *)nempal->image, 1, IMAGE_LEN(nempal), fp)) 
      != IMAGE_LEN(nempal)){
    printf("Couldn't write %s into %s\n", argv[2], argv[4]);
    printf("Requested %d wrote %d\n", IMAGE_LEN(nempal), rc);
    exit(1);
  }

  /* Pad to page bndry */
  for (i = IMAGE_LEN(nempal); i < nempal_len; i++) putc (0, fp);

  /* Pad to kernel start */
  for (i = BOOT_PA + nemboot_len + nempal_len; 
       i < kernel->text_start; i++) putc (0, fp);

  /* Write kernel image */
  if ((rc=fwrite((char *)kernel->image, 1, FILE_LEN(kernel), fp)) !=
      FILE_LEN(kernel)){
    printf("Couldn't write %s into %s\n", argv[3], argv[4]);
    printf("Requested %d wrote %d\n", FILE_LEN(kernel), rc);
    exit(1);
  }

  /* Pad with zeros for BOOTP bug */
  for (i = 0; i < 1024; i++) putc (0, fp);
    
  fclose(fp);

  exit(0);
}
